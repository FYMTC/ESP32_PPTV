/*
 * PMU电源管理服务实现文件
 * 提供AXP2101电源管理功能
 */

#include "pmu_service.h"
#include "axp2101.hpp"
#include "XPowersLib.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "PMU_SERVICE";

// PMU服务状态
static pmu_status_t s_pmu_status = PMU_STATUS_DISCONNECTED;
static pmu_data_t s_pmu_data = {};
static bool s_service_running = false;

// 回调函数
static pmu_data_callback_t s_data_callback = NULL;
static pmu_event_callback_t s_event_callback = NULL;

// 定时器句柄
static esp_timer_handle_t s_pmu_timer = NULL;

// 外部PMU对象声明
extern XPowersPMU PMU;

/**
 * @brief 从AXP2101读取数据并填充到pmu_data结构
 */
static void update_pmu_data(void)
{
    if (s_pmu_status != PMU_STATUS_CONNECTED) {
        return;
    }

    // 更新时间戳
    s_pmu_data.timestamp = esp_timer_get_time() / 1000; // 转换为毫秒

    // 读取电池信息
    if (PMU.isBatteryConnect()) {
        s_pmu_data.battery_voltage = PMU.getBattVoltage();
        // 注意：XPowersAXP2101可能没有直接的电流读取方法，使用估算
        s_pmu_data.battery_current = 0; // 暂时设为0，或根据充电状态估算
        s_pmu_data.battery_percentage = PMU.getBatteryPercent();
        
        if (PMU.isCharging()) {
            s_pmu_data.battery_status = BATTERY_STATUS_CHARGING;
            s_pmu_data.charge_status = CHARGE_STATUS_CONSTANT_CURRENT; // 简化处理
            s_pmu_data.battery_current = 200; // 充电时估算为正值
        } else if (s_pmu_data.battery_percentage >= 100) {
            s_pmu_data.battery_status = BATTERY_STATUS_FULL;
            s_pmu_data.charge_status = CHARGE_STATUS_CHARGE_DONE;
        } else {
            s_pmu_data.battery_status = BATTERY_STATUS_DISCHARGING;
            s_pmu_data.charge_status = CHARGE_STATUS_NOT_CHARGING;
            s_pmu_data.battery_current = -100; // 放电时估算为负值
        }
    } else {
        s_pmu_data.battery_status = BATTERY_STATUS_NOT_PRESENT;
        s_pmu_data.charge_status = CHARGE_STATUS_NOT_CHARGING;
    }

    // 读取VBUS信息
    s_pmu_data.vbus_present = PMU.isVbusIn();
    if (s_pmu_data.vbus_present) {
        s_pmu_data.vbus_voltage = PMU.getVbusVoltage();
        // s_pmu_data.vbus_current = PMU.getVbusCurrent(); // 如果支持的话
    }

    // 读取系统信息
    s_pmu_data.system_voltage = PMU.getSystemVoltage();
    s_pmu_data.temperature = (int16_t)(PMU.getTemperature() * 10); // 转换为°C * 10

    // 读取电源通道状态
    s_pmu_data.dc1_enabled = PMU.isEnableDC1();
    s_pmu_data.dc3_enabled = PMU.isEnableDC3();
    s_pmu_data.aldo1_enabled = PMU.isEnableALDO1();
    s_pmu_data.aldo2_enabled = PMU.isEnableALDO2();
    s_pmu_data.aldo3_enabled = PMU.isEnableALDO3();
    s_pmu_data.aldo4_enabled = PMU.isEnableALDO4();
    s_pmu_data.bldo1_enabled = PMU.isEnableBLDO1();
    s_pmu_data.bldo2_enabled = PMU.isEnableBLDO2();

    // 读取充电设置信息
    // s_pmu_data.charge_current = PMU.getChargerConstantCurr(); // 需要API支持
    // s_pmu_data.charge_voltage = PMU.getChargeTargetVoltage(); // 需要API支持

    s_pmu_data.pmu_status = s_pmu_status;
}

/**
 * @brief 定时器回调函数
 */
static void pmu_timer_callback(void *arg)
{
    update_pmu_data();

    // 调用数据回调
    if (s_data_callback != NULL) {
        s_data_callback(&s_pmu_data);
    }
}

/**
 * @brief PMU中断处理包装函数
 */
extern "C" void pmu_service_isr_handler(void)
{
    // 获取中断状态
    PMU.getIrqStatus();

    // 触发事件回调
    if (s_event_callback != NULL) {
        if (PMU.isVbusInsertIrq()) {
            s_event_callback("vbus_insert", 1);
        }
        if (PMU.isVbusRemoveIrq()) {
            s_event_callback("vbus_remove", 0);
        }
        if (PMU.isBatInsertIrq()) {
            s_event_callback("battery_insert", 1);
        }
        if (PMU.isBatRemoveIrq()) {
            s_event_callback("battery_remove", 0);
        }
        if (PMU.isPekeyShortPressIrq()) {
            s_event_callback("power_key_short", 1);
        }
        if (PMU.isPekeyLongPressIrq()) {
            s_event_callback("power_key_long", 1);
        }
        if (PMU.isBatChagerDoneIrq()) {
            s_event_callback("charge_done", 1);
        }
        if (PMU.isBatChagerStartIrq()) {
            s_event_callback("charge_start", 1);
        }
    }

    // 清除中断状态
    PMU.clearIrqStatus();
}

// C接口实现
extern "C" {

esp_err_t pmu_service_init(void)
{
    ESP_LOGI(TAG, "Initializing PMU service...");

    // 初始化PMU硬件
    esp_err_t ret = pmu_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PMU hardware: %s", esp_err_to_name(ret));
        s_pmu_status = PMU_STATUS_ERROR;
        return ret;
    }

    // 初始化数据结构
    memset(&s_pmu_data, 0, sizeof(pmu_data_t));
    s_pmu_status = PMU_STATUS_CONNECTED;

    // 读取初始数据
    update_pmu_data();

    ESP_LOGI(TAG, "PMU service initialized successfully");
    return ESP_OK;
}

esp_err_t pmu_service_start(uint32_t interval_ms)
{
    if (s_pmu_status != PMU_STATUS_CONNECTED) {
        ESP_LOGE(TAG, "PMU not connected, cannot start service");
        return ESP_FAIL;
    }

    if (s_service_running) {
        ESP_LOGW(TAG, "PMU service is already running");
        return ESP_OK;
    }

    // 创建定时器
    esp_timer_create_args_t timer_args = {
        .callback = pmu_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "pmu_timer",
        .skip_unhandled_events = false
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_pmu_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PMU timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启动定时器
    ret = esp_timer_start_periodic(s_pmu_timer, interval_ms * 1000); // 转换为微秒
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start PMU timer: %s", esp_err_to_name(ret));
        esp_timer_delete(s_pmu_timer);
        s_pmu_timer = NULL;
        return ret;
    }

    s_service_running = true;
    ESP_LOGI(TAG, "PMU service started with %lu ms interval", interval_ms);
    return ESP_OK;
}

esp_err_t pmu_service_stop(void)
{
    if (!s_service_running) {
        ESP_LOGW(TAG, "PMU service is not running");
        return ESP_OK;
    }

    if (s_pmu_timer != NULL) {
        esp_timer_stop(s_pmu_timer);
        esp_timer_delete(s_pmu_timer);
        s_pmu_timer = NULL;
    }

    s_service_running = false;
    ESP_LOGI(TAG, "PMU service stopped");
    return ESP_OK;
}

esp_err_t pmu_service_register_data_callback(pmu_data_callback_t callback)
{
    s_data_callback = callback;
    ESP_LOGI(TAG, "PMU data callback registered");
    return ESP_OK;
}

esp_err_t pmu_service_register_event_callback(pmu_event_callback_t callback)
{
    s_event_callback = callback;
    ESP_LOGI(TAG, "PMU event callback registered");
    return ESP_OK;
}

esp_err_t pmu_service_get_data(pmu_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_pmu_status != PMU_STATUS_CONNECTED) {
        return ESP_FAIL;
    }

    // 更新数据
    update_pmu_data();
    
    // 复制数据
    memcpy(data, &s_pmu_data, sizeof(pmu_data_t));
    return ESP_OK;
}

pmu_status_t pmu_service_get_status(void)
{
    return s_pmu_status;
}

esp_err_t pmu_service_set_charge_current(uint16_t current_ma)
{
    if (s_pmu_status != PMU_STATUS_CONNECTED) {
        return ESP_FAIL;
    }

    // 将mA转换为AXP2101的充电电流枚举
    xpowers_axp2101_chg_curr_t chg_curr;
    if (current_ma <= 100) {
        chg_curr = XPOWERS_AXP2101_CHG_CUR_100MA;
    } else if (current_ma <= 200) {
        chg_curr = XPOWERS_AXP2101_CHG_CUR_200MA;
    } else if (current_ma <= 300) {
        chg_curr = XPOWERS_AXP2101_CHG_CUR_300MA;
    } else if (current_ma <= 400) {
        chg_curr = XPOWERS_AXP2101_CHG_CUR_400MA;
    } else if (current_ma <= 500) {
        chg_curr = XPOWERS_AXP2101_CHG_CUR_500MA;
    } else if (current_ma <= 600) {
        chg_curr = XPOWERS_AXP2101_CHG_CUR_600MA;
    } else if (current_ma <= 700) {
        chg_curr = XPOWERS_AXP2101_CHG_CUR_700MA;
    } else {
        chg_curr = XPOWERS_AXP2101_CHG_CUR_800MA;
    }

    PMU.setChargerConstantCurr(chg_curr);
    s_pmu_data.charge_current = current_ma;
    
    ESP_LOGI(TAG, "Charge current set to %u mA", current_ma);
    return ESP_OK;
}

esp_err_t pmu_service_set_charge_voltage(uint16_t voltage_mv)
{
    if (s_pmu_status != PMU_STATUS_CONNECTED) {
        return ESP_FAIL;
    }

    // 将mV转换为AXP2101的充电电压枚举
    xpowers_axp2101_chg_vol_t chg_vol;
    if (voltage_mv <= 4000) {
        chg_vol = XPOWERS_AXP2101_CHG_VOL_4V;
    } else if (voltage_mv <= 4100) {
        chg_vol = XPOWERS_AXP2101_CHG_VOL_4V1;
    } else if (voltage_mv <= 4200) {
        chg_vol = XPOWERS_AXP2101_CHG_VOL_4V2;
    } else {
        chg_vol = XPOWERS_AXP2101_CHG_VOL_4V35;
    }

    PMU.setChargeTargetVoltage(chg_vol);
    s_pmu_data.charge_voltage = voltage_mv;
    
    ESP_LOGI(TAG, "Charge voltage set to %u mV", voltage_mv);
    return ESP_OK;
}

esp_err_t pmu_service_set_power_channel(const char *channel, bool enable)
{
    if (s_pmu_status != PMU_STATUS_CONNECTED || channel == NULL) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Setting power channel %s to %s", channel, enable ? "ON" : "OFF");

    if (strcmp(channel, "dc1") == 0) {
        if (enable) PMU.enableDC1(); else PMU.disableDC1();
    } else if (strcmp(channel, "dc3") == 0) {
        if (enable) PMU.enableDC3(); else PMU.disableDC3();
    } else if (strcmp(channel, "aldo1") == 0) {
        if (enable) PMU.enableALDO1(); else PMU.disableALDO1();
    } else if (strcmp(channel, "aldo2") == 0) {
        if (enable) PMU.enableALDO2(); else PMU.disableALDO2();
    } else if (strcmp(channel, "aldo3") == 0) {
        if (enable) PMU.enableALDO3(); else PMU.disableALDO3();
    } else if (strcmp(channel, "aldo4") == 0) {
        if (enable) PMU.enableALDO4(); else PMU.disableALDO4();
    } else if (strcmp(channel, "bldo1") == 0) {
        if (enable) PMU.enableBLDO1(); else PMU.disableBLDO1();
    } else if (strcmp(channel, "bldo2") == 0) {
        if (enable) PMU.enableBLDO2(); else PMU.disableBLDO2();
    } else {
        ESP_LOGE(TAG, "Unknown power channel: %s", channel);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t pmu_service_reset(void)
{
    ESP_LOGI(TAG, "Resetting PMU service...");

    // 停止服务
    pmu_service_stop();

    // 重置状态
    s_pmu_status = PMU_STATUS_DISCONNECTED;
    memset(&s_pmu_data, 0, sizeof(pmu_data_t));
    s_data_callback = NULL;
    s_event_callback = NULL;

    // 重新初始化
    return pmu_service_init();
}

} // extern "C"
