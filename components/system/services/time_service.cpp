/*
 * 统一的时间服务，整合了系统时间管理和SNTP时间同步功能
 * 功能包括：
 * 1. WiFi 连接管理（自动重连）
 * 2. SNTP 时间同步（WiFi连接后自动触发）
 * 3. 本地时间显示和格式化
 * 4. 时区设置（中国标准时间 UTC+8）
 * 5. 状态回调和错误处理
 * 6. 定时时间更新回调
 */

#include "time_service.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "wifi_manager.h"
#include "rtc_ds1302.h"
#include <cstring>
#include <string.h>
#include <sys/time.h>

static const char *TAG = "time_service";

#define DEFAULT_ESP_WIFI_SSID "FYMTC"
#define DEFAULT_ESP_WIFI_PASS "1234567891"
#define DEFAULT_ESP_MAXIMUM_RETRY 5 // 最大重连次数

namespace time_service {

// 静态变量
static TimeUpdateCallback g_time_callback = nullptr;
static TimerHandle_t g_update_timer = nullptr;
static bool g_is_initialized = false;
static bool g_sntp_running = false;
static TaskHandle_t g_sntp_task_handle = nullptr;
static bool g_rtc_synced_from_sntp = false; // 标记RTC是否已从SNTP同步过
static bool g_time_status_printed = false; // 防止重复打印时间状态
static time_t g_last_print_time = 0; // 记录上次打印时间

/* 保存ESP32重启次数的变量，存储在RTC内存中
 * 从深度睡眠唤醒时保持数值
 */
RTC_DATA_ATTR static int boot_count = 0;

// 内部函数声明
static void timer_callback(TimerHandle_t xTimer);
static void update_time_info();
static void print_current_time(void);
static void setup_sntp_config(void);
static void sntp_task_main(void *pvParameters);
static void wifi_status_callback(wifi_manager_status_t status, const char *ssid);
static void sntp_sync_callback(bool success);
static void init_system_time_from_rtc(void);
static void sync_rtc_from_sntp_once(const char* trigger_source);

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

// 打印当前时间（增加防重复机制）
static void print_current_time(void)
{
    time_t now;
    time(&now);
    
    // 如果距离上次打印时间小于5秒，则跳过打印（避免频繁重复）
    if (g_last_print_time != 0 && (now - g_last_print_time) < 5) {
        return;
    }
    
    g_last_print_time = now;
    
    struct tm timeinfo;
    char strftime_buf[64];

    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "当前时间戳: %ld", (long)now);
    ESP_LOGI(TAG, "年份: %d (应该>2020)", timeinfo.tm_year + 1900);

    // 检查时间是否有效（年份应该大于2020）
    if (timeinfo.tm_year < (2020 - 1900))
    {
        ESP_LOGI(TAG, "❌ 时间尚未同步，当前时间无效 (年份: %d)", timeinfo.tm_year + 1900);
        return;
    }

    // 格式化时间字符串（显示完整的日期时间）
    strftime(strftime_buf, sizeof(strftime_buf), "%Y年%m月%d日 %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "✅ 当前北京时间: %s", strftime_buf);

    // 额外显示星期信息
    const char *weekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    ESP_LOGI(TAG, "今天是: %s", weekdays[timeinfo.tm_wday]);
    
    // 打印RTC时间信息用于调试
    if (rtc_ds1302_get_status() == RTC_INITIALIZED) {
        struct tm rtc_time;
        esp_err_t ret = rtc_ds1302_get_time(&rtc_time);
        if (ret == ESP_OK) {
            char rtc_buf[64];
            strftime(rtc_buf, sizeof(rtc_buf), "%Y年%m月%d日 %H:%M:%S", &rtc_time);
            ESP_LOGI(TAG, "🕐 RTC时间: %s", rtc_buf);
            
            // 计算系统时间与RTC时间的差异
            time_t rtc_timestamp = mktime(&rtc_time);
            long time_diff = (long)(now - rtc_timestamp);
            if (abs(time_diff) > 5) { // 如果差异超过5秒
                ESP_LOGW(TAG, "⚠️  系统时间与RTC时间差异: %ld秒", time_diff);
            } else {
                ESP_LOGI(TAG, "✅ 系统时间与RTC时间同步良好 (差异: %ld秒)", time_diff);
            }
        } else {
            ESP_LOGE(TAG, "❌ 无法读取RTC时间: %s", esp_err_to_name(ret));
        }
        
        // 检查RTC运行状态
        bool running = false;
        ret = rtc_ds1302_is_running(&running);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "🔄 RTC运行状态: %s", running ? "运行中" : "已停止");
        }
    } else {
        ESP_LOGW(TAG, "⚠️  RTC未初始化或出现错误");
    }
}

// 统一的RTC同步函数，避免重复同步
static void sync_rtc_from_sntp_once(const char* trigger_source)
{
    // 检查是否已经同步过
    if (g_rtc_synced_from_sntp) {
        ESP_LOGI(TAG, "RTC已从SNTP同步过，跳过重复同步 (触发源: %s)", trigger_source);
        return;
    }
    
    // 检查RTC是否可用
    if (rtc_ds1302_get_status() != RTC_INITIALIZED) {
        ESP_LOGW(TAG, "⚠️  RTC未初始化，跳过RTC同步 (触发源: %s)", trigger_source);
        return;
    }
    
    ESP_LOGI(TAG, "开始将SNTP时间同步到RTC芯片... (触发源: %s)", trigger_source);
    esp_err_t ret = rtc_ds1302_sync_from_system_time();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ RTC芯片已同步SNTP时间 (触发源: %s)", trigger_source);
        g_rtc_synced_from_sntp = true; // 标记已同步
    } else {
        ESP_LOGE(TAG, "❌ RTC芯片同步失败: %s (触发源: %s)", esp_err_to_name(ret), trigger_source);
    }
}

// WiFi状态回调函数
static void wifi_status_callback(wifi_manager_status_t status, const char *ssid)
{
    // 注意：此回调在系统事件任务中运行，避免执行耗时操作或调用LVGL函数
    switch (status)
    {
    case WIFI_MANAGER_CONNECTING:
        ESP_LOGI(TAG, "正在连接到 WiFi: %s", ssid ? ssid : "Unknown");
        break;

    case WIFI_MANAGER_CONNECTED:
        ESP_LOGI(TAG, "✓ WiFi 连接成功: %s", ssid ? ssid : "Unknown");
        break;

    case WIFI_MANAGER_DISCONNECTED:
        ESP_LOGI(TAG, "WiFi 已断开连接");
        break;

    case WIFI_MANAGER_CONNECTION_FAILED:
        ESP_LOGE(TAG, "✗ WiFi 连接失败");
        break;

    case WIFI_MANAGER_TIME_SYNCING:
        ESP_LOGI(TAG, "正在同步网络时间...");
        break;

    case WIFI_MANAGER_TIME_SYNCED:
        ESP_LOGI(TAG, "✓ 网络时间同步完成");
        // 使用任务通知机制来异步处理时间打印和RTC同步
        if (g_sntp_task_handle) {
            xTaskNotify(g_sntp_task_handle, 0x01, eSetBits);
        }
        break;

    default:
        ESP_LOGW(TAG, "未知的WiFi状态: %d", status);
        break;
    }
}

// SNTP同步回调函数
static void sntp_sync_callback(bool success)
{
    // 注意：此回调在系统事件任务中运行，避免执行耗时操作
    if (success)
    {
        ESP_LOGI(TAG, "✓ SNTP 时间同步成功");
        // 使用任务通知机制来异步处理时间打印和RTC同步
        if (g_sntp_task_handle) {
            xTaskNotify(g_sntp_task_handle, 0x02, eSetBits);
        }
    }
    else
    {
        ESP_LOGE(TAG, "✗ SNTP 时间同步失败");
    }
}

// 设置SNTP配置
static void setup_sntp_config(void)
{
    // 配置SNTP服务器（使用中国NTP服务器以提高连接成功率）
    wifi_manager_sntp_config_t sntp_config = {
        .servers = {
            "ntp1.aliyun.com", // 阿里云NTP服务器
            "",                // 空服务器
            ""                 // 空服务器
        },
        .server_count = 1,   // 只使用一个服务器
        .timeout_ms = 15000, // 增加到15秒超时
        .sync_mode = 0       // 同步模式（使用默认值）
    };

    // 应用SNTP配置
    esp_err_t ret = wifi_manager_configure_sntp(&sntp_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SNTP 配置失败: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "SNTP 配置成功，使用中国NTP服务器: %s", sntp_config.servers[0]);
    }
}

// SNTP主任务
static void sntp_task_main(void *pvParameters)
{
    // 增加并显示启动次数
    ++boot_count;
    ESP_LOGI(TAG, "系统启动次数: %d", boot_count);

    // 显示重启原因
    esp_reset_reason_t reset_reason = esp_reset_reason();
    const char *reset_reasons[] = {
        "Unknown", "Power on", "External", "Software", "Panic",
        "Interrupt watchdog", "Task watchdog", "Other watchdog",
        "Deep sleep", "Brownout", "SDIO"};
    if (reset_reason < sizeof(reset_reasons) / sizeof(reset_reasons[0]))
    {
        ESP_LOGI(TAG, "重启原因: %s", reset_reasons[reset_reason]);
    }

    esp_err_t ret;
    ESP_LOGI(TAG, "✓ NVS存储初始化完成");

    // 显示当前时间（同步前）
    ESP_LOGI(TAG, "SNTP任务启动，检查当前时间状态:");
    print_current_time();

    // 初始化WiFi管理器（具有防重复初始化保护）
    ESP_LOGI(TAG, "初始化WiFi管理器...");
    ret = wifi_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "✗ WiFi 管理器初始化失败: %s", esp_err_to_name(ret));
        g_sntp_running = false;
        vTaskDelete(NULL); // 删除当前任务
        return;
    }

    // 检查WiFi管理器是否已经在其他地方初始化过
    wifi_manager_status_t current_status = wifi_manager_get_status();
    if (current_status != WIFI_MANAGER_DISCONNECTED)
    {
        ESP_LOGI(TAG, "✓ WiFi 管理器已初始化，当前状态: %d", current_status);
    }
    else
    {
        ESP_LOGI(TAG, "✓ WiFi 管理器初始化成功");
    }

    // 注册回调函数
    wifi_manager_register_status_cb(wifi_status_callback);
    wifi_manager_register_sntp_cb(sntp_sync_callback);
    ESP_LOGI(TAG, "✓ 状态回调函数注册完成");

    // 配置SNTP设置
    setup_sntp_config();

    // 设置WiFi连接参数
    static wifi_manager_config_t wifi_config = {
        .ssid = DEFAULT_ESP_WIFI_SSID,
        .password = DEFAULT_ESP_WIFI_PASS,
        .maximum_retry = DEFAULT_ESP_MAXIMUM_RETRY,
        .auto_reconnect = true // 启用自动重连
    };

    // 启动WiFi连接
    ESP_LOGI(TAG, "启动WiFi连接到: %s", DEFAULT_ESP_WIFI_SSID);
    ret = wifi_manager_connect_with_config(&wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "✗ 初次WiFi 连接失败: %s，将进入重试模式", esp_err_to_name(ret));
        // 不要立即退出，而是进入监控循环，WiFi管理器会自动重试
    }
    else
    {
        ESP_LOGI(TAG, "✓ WiFi 连接启动成功");

        // 等待一段时间让WiFi完全连接
        vTaskDelay(5000 / portTICK_PERIOD_MS); // 等待5秒

        // 手动触发时间同步
        ESP_LOGI(TAG, "手动触发时间同步...");
        wifi_manager_sync_time();
    }

    ESP_LOGI(TAG, "等待 WiFi 连接和时间同步...");
    ESP_LOGI(TAG, "程序将每30秒显示一次当前时间");

    // 主循环 - 每30秒显示一次当前时间和状态
    int loop_count = 0;
    while (g_sntp_running)
    {
        // 等待30秒或任务通知
        uint32_t notification_value = 0;
        BaseType_t result = xTaskNotifyWait(0x00, ULONG_MAX, &notification_value, pdMS_TO_TICKS(30000));
        
        if (!g_sntp_running) break; // 检查是否需要退出
        
        // 处理任务通知
        if (result == pdTRUE) {
            if (notification_value & 0x01) {
                // WiFi Manager 时间同步完成通知
                ESP_LOGI(TAG, "处理WiFi Manager时间同步完成通知");
                print_current_time();
                sync_rtc_from_sntp_once("WiFi Manager");
            }
            if (notification_value & 0x02) {
                // SNTP 时间同步完成通知
                ESP_LOGI(TAG, "处理SNTP时间同步完成通知");
                print_current_time();
                sync_rtc_from_sntp_once("SNTP Callback");
            }
            // 处理通知后继续循环，不执行下面的定时检查
            continue;
        }

        // 检查WiFi状态
        wifi_manager_status_t status = wifi_manager_get_status();
        const char *status_names[] = {
            "未连接", "连接中", "已连接", "连接失败", 
            "时间同步中", "时间已同步"};

        if (status < sizeof(status_names) / sizeof(status_names[0]))
        {
            ESP_LOGI(TAG, "WiFi状态: %s (状态码: %d)", status_names[status], status);
        }

        // 检查WiFi连接状态
        bool is_connected = wifi_manager_is_connected();
        ESP_LOGI(TAG, "WiFi连接检查: %s", is_connected ? "已连接" : "未连接");

        // 如果WiFi已连接，检查时间同步状态
        if (is_connected)
        {
            bool time_synced = wifi_manager_is_time_synced();
            ESP_LOGI(TAG, "时间同步状态: %s", time_synced ? "已同步" : "未同步");

            if (time_synced)
            {
                // 只在第一次同步成功时打印详细时间信息
                if (!g_time_status_printed) {
                    print_current_time();
                    g_time_status_printed = true;
                }

                // 可选：显示WiFi信号强度
                int8_t rssi;
                if (wifi_manager_get_rssi(&rssi) == ESP_OK)
                {
                    ESP_LOGI(TAG, "WiFi信号强度: %d dBm", rssi);
                }
            }
            else
            {
                ESP_LOGI(TAG, "WiFi已连接但时间未同步，尝试手动同步...");
                wifi_manager_sync_time();
                g_time_status_printed = false; // 重置状态，等待重新同步
            }
        }
        else
        {
            ESP_LOGI(TAG, "WiFi未连接，尝试重新连接...");
            g_time_status_printed = false; // 重置状态
            // 尝试重新连接
            esp_err_t reconnect_ret = wifi_manager_connect_with_config(&wifi_config);
            if (reconnect_ret != ESP_OK)
            {
                ESP_LOGE(TAG, "重新连接失败: %s", esp_err_to_name(reconnect_ret));
            }
        }
    }
    
    ESP_LOGI(TAG, "SNTP任务退出");
    g_sntp_task_handle = nullptr;
    vTaskDelete(NULL);
}

// 从RTC初始化系统时间
static void init_system_time_from_rtc(void)
{
    ESP_LOGI(TAG, "尝试从RTC初始化系统时间...");
    
    // 检查RTC是否可用
    if (rtc_ds1302_get_status() != RTC_INITIALIZED) {
        ESP_LOGW(TAG, "RTC未初始化，跳过从RTC设置系统时间");
        return;
    }
    
    // 从RTC读取时间
    struct tm rtc_time;
    esp_err_t ret = rtc_ds1302_get_time(&rtc_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取RTC时间失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 检查RTC时间是否有效（年份应该大于2020）
    if (rtc_time.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "RTC时间无效 (年份: %d)，跳过系统时间设置", rtc_time.tm_year + 1900);
        return;
    }
    
    // 将RTC时间设置为系统时间
    time_t rtc_timestamp = mktime(&rtc_time);
    struct timeval tv = {
        .tv_sec = rtc_timestamp,
        .tv_usec = 0
    };
    
    ret = settimeofday(&tv, NULL);
    if (ret == 0) {
        char time_buf[64];
        strftime(time_buf, sizeof(time_buf), "%Y年%m月%d日 %H:%M:%S", &rtc_time);
        ESP_LOGI(TAG, "✅ 系统时间已从RTC初始化: %s", time_buf);
        
        // 验证设置是否成功
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        long time_diff = (long)(now - rtc_timestamp);
        if (abs(time_diff) <= 2) { // 允许2秒误差
            ESP_LOGI(TAG, "✅ 系统时间设置成功，与RTC时间同步良好");
        } else {
            ESP_LOGW(TAG, "⚠️  系统时间设置后与RTC仍有差异: %ld秒", time_diff);
        }
    } else {
        ESP_LOGE(TAG, "❌ 设置系统时间失败");
    }
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
        ESP_LOGW(TAG, "Time service already initialized");
        return;
    }
    
    ESP_LOGI(TAG, "开始初始化时间服务...");
    
    // 重置相关标志
    g_rtc_synced_from_sntp = false;
    g_time_status_printed = false;
    g_last_print_time = 0;
    
    // 设置时区为中国标准时间 (UTC+8)
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "时区设置为中国标准时间 (UTC+8)");
    
    // 从RTC初始化系统时间（如果RTC可用且时间有效）
    init_system_time_from_rtc();
    
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
    ESP_LOGI(TAG, "Time service initialized");
    
    // 显示当前时间状态
    ESP_LOGI(TAG, "时间服务初始化后的时间状态:");
    print_current_time();
    
    // 自动启动SNTP同步
    start_sntp_sync();
}

// 反初始化时间服务
void deinit()
{
    if (!g_is_initialized) {
        return;
    }
    
    // 停止SNTP同步
    stop_sntp_sync();
    
    // 停止并删除定时器
    if (g_update_timer) {
        xTimerStop(g_update_timer, 0);
        xTimerDelete(g_update_timer, 0);
        g_update_timer = nullptr;
    }
    
    // 清除回调
    g_time_callback = nullptr;
    g_is_initialized = false;
    
    ESP_LOGI(TAG, "Time service deinitialized");
}

// 注册时间更新回调
void set_time_update_callback(TimeUpdateCallback callback)
{
    if (!g_is_initialized) {
        ESP_LOGE(TAG, "Time service not initialized");
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

// 启动SNTP同步
void start_sntp_sync()
{
    if (!g_is_initialized) {
        ESP_LOGE(TAG, "Time service not initialized");
        return;
    }
    
    if (g_sntp_running) {
        ESP_LOGW(TAG, "SNTP sync already running");
        return;
    }
    
    g_sntp_running = true;
    
    // 创建SNTP任务，增加栈大小以避免栈溢出，特别是Tmr Svc服务
    if (xTaskCreate(sntp_task_main, "sntp_task", 8192, NULL, 5, &g_sntp_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create SNTP task");
        g_sntp_running = false;
        g_sntp_task_handle = nullptr;
    } else {
        ESP_LOGI(TAG, "SNTP sync started");
    }
}

// 停止SNTP同步
void stop_sntp_sync()
{
    if (!g_sntp_running) {
        return;
    }
    
    g_sntp_running = false;
    
    // 等待任务退出
    if (g_sntp_task_handle) {
        // 给任务一些时间来清理
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        g_sntp_task_handle = nullptr;
    }
    
    ESP_LOGI(TAG, "SNTP sync stopped");
}

// 检查SNTP是否正在同步
bool is_sntp_syncing()
{
    return g_sntp_running;
}

// 检查RTC是否可用
bool is_rtc_available()
{
    return (rtc_ds1302_get_status() == RTC_INITIALIZED);
}

// 将系统时间同步到RTC
bool sync_rtc_from_system()
{
    if (rtc_ds1302_get_status() != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "RTC not initialized");
        return false;
    }
    
    esp_err_t ret = rtc_ds1302_sync_from_system_time();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "RTC synchronized from system time");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to sync RTC from system time: %s", esp_err_to_name(ret));
        return false;
    }
}

// 获取RTC时间
bool get_rtc_time(struct tm* rtc_time)
{
    if (rtc_ds1302_get_status() != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "RTC not initialized");
        return false;
    }
    
    if (rtc_time == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: rtc_time is NULL");
        return false;
    }
    
    esp_err_t ret = rtc_ds1302_get_time(rtc_time);
    if (ret == ESP_OK) {
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to get RTC time: %s", esp_err_to_name(ret));
        return false;
    }
}

// 公共接口：手动从RTC初始化系统时间
bool init_system_time_from_rtc_public()
{
    ESP_LOGI(TAG, "手动从RTC初始化系统时间...");
    
    // 检查RTC是否可用
    if (rtc_ds1302_get_status() != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "RTC未初始化，无法设置系统时间");
        return false;
    }
    
    // 从RTC读取时间
    struct tm rtc_time;
    esp_err_t ret = rtc_ds1302_get_time(&rtc_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取RTC时间失败: %s", esp_err_to_name(ret));
        return false;
    }
    
    // 检查RTC时间是否有效（年份应该大于2020）
    if (rtc_time.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "RTC时间无效 (年份: %d)，无法设置系统时间", rtc_time.tm_year + 1900);
        return false;
    }
    
    // 将RTC时间设置为系统时间
    time_t rtc_timestamp = mktime(&rtc_time);
    struct timeval tv = {
        .tv_sec = rtc_timestamp,
        .tv_usec = 0
    };
    
    ret = settimeofday(&tv, NULL);
    if (ret == 0) {
        char time_buf[64];
        strftime(time_buf, sizeof(time_buf), "%Y年%m月%d日 %H:%M:%S", &rtc_time);
        ESP_LOGI(TAG, "✅ 系统时间已手动从RTC设置: %s", time_buf);
        
        // 验证设置是否成功
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        long time_diff = (long)(now - rtc_timestamp);
        if (abs(time_diff) <= 2) { // 允许2秒误差
            ESP_LOGI(TAG, "✅ 系统时间设置成功，与RTC时间同步良好");
        } else {
            ESP_LOGW(TAG, "⚠️  系统时间设置后与RTC仍有差异: %ld秒", time_diff);
        }
        return true;
    } else {
        ESP_LOGE(TAG, "❌ 设置系统时间失败");
        return false;
    }
}

} // namespace time_service
