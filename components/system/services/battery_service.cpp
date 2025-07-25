#include "battery_service.h"
#include "axp2101.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *BATTERY_TAG = "battery_service";

namespace battery_service {

// 静态变量
static BatteryUpdateCallback g_battery_callback = nullptr;
static TimerHandle_t g_update_timer = nullptr;
static bool g_is_initialized = false;
static bool g_pmu_available = false;

// 内部函数声明
static void timer_callback(TimerHandle_t xTimer);
static void update_battery_status();

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
    if (g_battery_callback && g_pmu_available) {
        BatteryInfo info = get_battery_info();
        if (info.is_valid) {
            g_battery_callback(info);
        }
    }
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
    g_is_initialized = false;
    g_pmu_available = false;
    
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
