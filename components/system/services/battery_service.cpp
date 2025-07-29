#include "battery_service.h"
#include "axp2101.hpp"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *BATTERY_TAG = "battery_service";

namespace battery_service {

// 静态变量
static BatteryUpdateCallback g_battery_callback = nullptr;
static LowBatteryWarningCallback g_low_battery_warning_callback = nullptr;
static TimerHandle_t g_update_timer = nullptr;
static bool g_is_initialized = false;
static bool g_pmu_available = false;

// 低电量关机配置
static LowBatteryConfig g_low_battery_config = {
    .shutdown_percentage = 5,    // 5%时自动关机
    .warning_percentage = 15,    // 15%时发出警告
    .enabled = true              // 默认启用
};

// 状态追踪
static bool g_warning_triggered = false;
static bool g_shutdown_in_progress = false;

// 内部函数声明
static void timer_callback(TimerHandle_t xTimer);
static void update_battery_status();
static void check_low_battery(const BatteryInfo& info);
static void perform_shutdown();

// 检查PMU是否可用
static bool check_pmu_availability()
{
    // 简单检查：尝试读取电池百分比
    int percent = PMU.getBatteryPercent();
    bool is_valid = (percent >= 0 && percent <= 100);
    
    if (!is_valid) {
        ESP_LOGW(BATTERY_TAG, "PMU battery percent out of range: %d", percent);
    }
    
    return is_valid;
}

// 获取当前电池信息
BatteryInfo get_battery_info()
{
    BatteryInfo info = {};
    
    if (!g_pmu_available) {
        info.is_valid = false;
        return info;
    }
    
    // 获取电池百分比
    info.percentage = PMU.getBatteryPercent();
    
    // 获取电池电压
    info.voltage_mv = PMU.getBattVoltage();
    
    // 检查是否正在充电
    info.is_charging = PMU.isCharging();
    
    // 检查电池是否连接
    info.is_connected = PMU.isBatteryConnect();
    
    // 数据有效性检查
    info.is_valid = (info.percentage >= 0 && info.percentage <= 100);
    
    ESP_LOGD(BATTERY_TAG, "Battery: %d%%, %dmV, charging: %s, connected: %s", 
            info.percentage, info.voltage_mv, 
            info.is_charging ? "yes" : "no",
            info.is_connected ? "yes" : "no");
    
    return info;
}

// 定时器回调函数
static void timer_callback(TimerHandle_t xTimer)
{
    update_battery_status();
}

// 更新电池状态并触发回调
static void update_battery_status()
{
    if (g_pmu_available) {
        BatteryInfo info = get_battery_info();
        if (info.is_valid) {
            // 检查低电量情况
            check_low_battery(info);
            
            // 触发回调
            if (g_battery_callback) {
                g_battery_callback(info);
            }
        }
    }
}

// 检查低电量并执行相应操作
static void check_low_battery(const BatteryInfo& info)
{
    if (!g_low_battery_config.enabled || g_shutdown_in_progress) {
        return;
    }
    
    // 只有在电池连接且不在充电时才检查关机条件
    if (!info.is_connected || info.is_charging) {
        // 重置警告状态（如果开始充电）
        if (info.is_charging && g_warning_triggered) {
            g_warning_triggered = false;
            ESP_LOGI(BATTERY_TAG, "Battery charging, low battery warning reset");
        }
        return;
    }
    
    // 检查是否需要关机
    if (info.percentage <= g_low_battery_config.shutdown_percentage) {
        ESP_LOGW(BATTERY_TAG, "Critical low battery: %d%%, initiating shutdown", info.percentage);
        perform_shutdown();
        return;
    }
    
    // 检查是否需要发出警告
    if (info.percentage <= g_low_battery_config.warning_percentage && !g_warning_triggered) {
        g_warning_triggered = true;
        ESP_LOGW(BATTERY_TAG, "Low battery warning: %d%%", info.percentage);
        
        if (g_low_battery_warning_callback) {
            g_low_battery_warning_callback(info.percentage);
        }
    }
    
    // 如果电量回升，重置警告状态
    if (info.percentage > g_low_battery_config.warning_percentage && g_warning_triggered) {
        g_warning_triggered = false;
        ESP_LOGI(BATTERY_TAG, "Battery level recovered: %d%%, warning reset", info.percentage);
    }
}

// 执行系统关机
static void perform_shutdown()
{
    if (g_shutdown_in_progress) {
        return;
    }
    
    g_shutdown_in_progress = true;
    ESP_LOGE(BATTERY_TAG, "CRITICAL: Battery too low, shutting down system in 3 seconds...");
    
    // 给用户3秒钟的时间看到关机消息
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 调用PMU关机
    ESP_LOGE(BATTERY_TAG, "Shutting down now...");
    PMU.shutdown();
    
    // 如果PMU关机失败，使用ESP32的深度睡眠作为备用方案
    ESP_LOGE(BATTERY_TAG, "PMU shutdown failed, entering deep sleep");
    esp_deep_sleep_start();
}

// 初始化电池服务
bool init()
{
    if (g_is_initialized) {
        ESP_LOGW(BATTERY_TAG, "Battery service already initialized");
        return true;
    }
    
    // 检查PMU是否可用
    g_pmu_available = check_pmu_availability();
    if (!g_pmu_available) {
        ESP_LOGW(BATTERY_TAG, "PMU not available, battery service will provide dummy data");
    }
    
    // 创建定时器，每5秒更新一次电池信息
    g_update_timer = xTimerCreate(
        "battery_update_timer",
        pdMS_TO_TICKS(5000),  // 5秒间隔
        pdTRUE,  // 自动重载
        NULL,
        timer_callback
    );
    
    if (g_update_timer == NULL) {
        ESP_LOGE(BATTERY_TAG, "Failed to create battery update timer");
        return false;
    }
    
    g_is_initialized = true;
    ESP_LOGI(BATTERY_TAG, "Battery service initialized (PMU available: %s)", 
             g_pmu_available ? "yes" : "no");
    
    return true;
}

// 反初始化电池服务
void deinit()
{
    if (!g_is_initialized) {
        return;
    }
    
    // 停止并删除定时器
    if (g_update_timer) {
        xTimerStop(g_update_timer, 0);
        xTimerDelete(g_update_timer, 0);
        g_update_timer = nullptr;
    }
    
    // 清除回调
    g_battery_callback = nullptr;
    g_low_battery_warning_callback = nullptr;
    g_is_initialized = false;
    g_pmu_available = false;
    
    // 重置状态
    g_warning_triggered = false;
    g_shutdown_in_progress = false;
    
    ESP_LOGI(BATTERY_TAG, "Battery service deinitialized");
}

// 注册电池状态更新回调
void set_battery_update_callback(BatteryUpdateCallback callback)
{
    if (!g_is_initialized) {
        ESP_LOGE(BATTERY_TAG, "Battery service not initialized");
        return;
    }
    
    g_battery_callback = callback;
    
    // 启动定时器
    if (g_update_timer && callback) {
        xTimerStart(g_update_timer, 0);
        ESP_LOGI(BATTERY_TAG, "Battery update callback registered and timer started");
        
        // 立即触发一次回调
        update_battery_status();
    } else if (g_update_timer && !callback) {
        xTimerStop(g_update_timer, 0);
        ESP_LOGI(BATTERY_TAG, "Battery update callback removed and timer stopped");
    }
}

// 移除电池状态更新回调
void remove_battery_update_callback()
{
    set_battery_update_callback(nullptr);
}

// 设置低电量警告回调
void set_low_battery_warning_callback(LowBatteryWarningCallback callback)
{
    if (!g_is_initialized) {
        ESP_LOGE(BATTERY_TAG, "Battery service not initialized");
        return;
    }
    
    g_low_battery_warning_callback = callback;
    ESP_LOGI(BATTERY_TAG, "Low battery warning callback %s", callback ? "registered" : "removed");
}

// 配置低电量关机设置
void configure_low_battery_shutdown(const LowBatteryConfig& config)
{
    if (!g_is_initialized) {
        ESP_LOGE(BATTERY_TAG, "Battery service not initialized");
        return;
    }
    
    // 验证配置参数
    if (config.shutdown_percentage < 1 || config.shutdown_percentage > 50) {
        ESP_LOGE(BATTERY_TAG, "Invalid shutdown percentage: %d (must be 1-50)", config.shutdown_percentage);
        return;
    }
    
    if (config.warning_percentage < config.shutdown_percentage || config.warning_percentage > 100) {
        ESP_LOGE(BATTERY_TAG, "Invalid warning percentage: %d (must be %d-100)", 
                 config.warning_percentage, config.shutdown_percentage);
        return;
    }
    
    g_low_battery_config = config;
    
    // 重置状态
    g_warning_triggered = false;
    g_shutdown_in_progress = false;
    
    ESP_LOGI(BATTERY_TAG, "Low battery config updated: shutdown=%d%%, warning=%d%%, enabled=%s",
             config.shutdown_percentage, config.warning_percentage, config.enabled ? "yes" : "no");
}

// 获取当前低电量配置
LowBatteryConfig get_low_battery_config()
{
    return g_low_battery_config;
}

// 检查电池服务是否可用
bool is_available()
{
    return g_is_initialized && g_pmu_available;
}

// 手动更新电池信息
void update_battery_info()
{
    if (g_is_initialized) {
        update_battery_status();
    }
}

} // namespace battery_service
