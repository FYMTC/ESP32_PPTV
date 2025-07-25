#include "system_time.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <cstring>

static const char *TAG = "system_time";

namespace system_time {

// 静态变量
static system_time::TimeUpdateCallback g_time_callback = nullptr;
static TimerHandle_t g_update_timer = nullptr;
static bool g_is_initialized = false;

// 内部函数声明
static void timer_callback(TimerHandle_t xTimer);
static void update_time_info();

// 格式化运行时间
const char* format_running_time(unsigned long millis)
{
    static char buffer[16];
    unsigned long seconds = millis / 1000;
    unsigned long hours = seconds / 3600;
    seconds %= 3600;
    unsigned long minutes = seconds / 60;
    seconds %= 60;
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    return buffer;
}

// 检查时间是否已同步
bool is_time_synced()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 检查时间是否有效（年份应该大于2020）
    return (timeinfo.tm_year >= (2020 - 1900));
}

// 获取当前时间信息
TimeInfo get_time_info()
{
    TimeInfo info = {};
    time_t now;
    struct tm timeinfo;
    
    // 获取当前时间
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // 填充时间信息
    info.timestamp = now;
    info.is_synced = is_time_synced();
    
    // 格式化时间字符串
    strftime(info.time_str, sizeof(info.time_str), "%H:%M", &timeinfo);
    strftime(info.datetime_str, sizeof(info.datetime_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // 格式化运行时间
    const char* running_time = format_running_time(esp_timer_get_time() / 1000);
    strncpy(info.running_str, running_time, sizeof(info.running_str) - 1);
    info.running_str[sizeof(info.running_str) - 1] = '\0';
    
    return info;
}

// 定时器回调函数（尽量轻量化，避免阻塞定时器任务）
static void timer_callback(TimerHandle_t xTimer)
{
    // 简单的通知机制，避免在定时器回调中做复杂操作
    if (g_time_callback) {
        // 可以考虑使用事件组或队列进行异步通知
        // 目前先保持简单的直接调用，但限制回调函数的复杂度
        update_time_info();
    }
}

// 更新时间信息并触发回调
static void update_time_info()
{
    if (g_time_callback) {
        TimeInfo info = get_time_info();
        // 注意：回调函数应该尽量轻量化，避免阻塞定时器任务
        g_time_callback(info);
    }
}

// 初始化时间服务
void init()
{
    if (g_is_initialized) {
        ESP_LOGW(TAG, "System time service already initialized");
        return;
    }
    
    // 创建定时器，每1000ms触发一次
    g_update_timer = xTimerCreate(
        "time_update_timer",
        pdMS_TO_TICKS(1000),
        pdTRUE,  // 自动重载
        NULL,
        timer_callback
    );
    
    if (g_update_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create time update timer");
        return;
    }
    
    g_is_initialized = true;
    ESP_LOGI(TAG, "System time service initialized");
}

// 反初始化时间服务
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
    g_time_callback = nullptr;
    g_is_initialized = false;
    
    ESP_LOGI(TAG, "System time service deinitialized");
}

// 注册时间更新回调
void set_time_update_callback(TimeUpdateCallback callback)
{
    if (!g_is_initialized) {
        ESP_LOGE(TAG, "System time service not initialized");
        return;
    }
    
    g_time_callback = callback;
    
    // 启动定时器
    if (g_update_timer && callback) {
        xTimerStart(g_update_timer, 0);
        ESP_LOGI(TAG, "Time update callback registered and timer started");
        
        // 立即触发一次回调
        update_time_info();
    } else if (g_update_timer && !callback) {
        xTimerStop(g_update_timer, 0);
        ESP_LOGI(TAG, "Time update callback removed and timer stopped");
    }
}

// 移除时间更新回调
void remove_time_update_callback()
{
    set_time_update_callback(nullptr);
}

} // namespace system_time
