/*
 * 此程序演示如何使用集成的 WiFi 管理器和 SNTP 功能进行自动时间同步
 * 程序包含以下功能：
 * 1. WiFi 连接管理（自动重连）
 * 2. SNTP 时间同步（WiFi连接后自动触发）
 * 3. 本地时间显示
 * 4. 时区设置（中国标准时间 UTC+8）
 * 5. 状态回调和错误处理
 */

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
// #include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi_manager.h"

static const char *TAG = "sntp";

#define DEFAULT_ESP_WIFI_SSID "FYMTC"
#define DEFAULT_ESP_WIFI_PASS "1234567891"
#define DEFAULT_ESP_MAXIMUM_RETRY 5 // 最大重连次数

/* 保存ESP32重启次数的变量，存储在RTC内存中
 * 从深度睡眠唤醒时保持数值
 */
RTC_DATA_ATTR static int boot_count = 0;

static void print_current_time(void);
static void setup_sntp_config(void);

// WiFi状态回调函数
static void wifi_status_callback(wifi_manager_status_t status, const char *ssid)
{
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
        print_current_time();
        break;

    default:
        ESP_LOGW(TAG, "未知的WiFi状态: %d", status);
        break;
    }
}

// SNTP同步回调函数
static void sntp_sync_callback(bool success)
{
    if (success)
    {
        ESP_LOGI(TAG, "✓ SNTP 时间同步成功");
        print_current_time();
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

// 打印当前时间
static void print_current_time(void)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    time(&now);
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
}

void sntp_task_main(void *pvParameters)
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
    // // 初始化NVS存储(已在别处处理)
    // ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //     ESP_LOGI(TAG, "清理NVS存储...");
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "✓ NVS存储初始化完成");

    // 设置时区为中国标准时间 (UTC+8)
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "时区设置为中国标准时间 (UTC+8)");

    // 显示当前时间（同步前）
    ESP_LOGI(TAG, "检查当前时间状态:");
    print_current_time();

    // 初始化WiFi管理器（具有防重复初始化保护）
    ESP_LOGI(TAG, "初始化WiFi管理器...");
    ret = wifi_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "✗ WiFi 管理器初始化失败: %s", esp_err_to_name(ret));
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
    while (1)
    {
        vTaskDelay(30000 / portTICK_PERIOD_MS); // 等待30秒
        loop_count++;

        ESP_LOGI(TAG, "--- 状态检查 (循环 #%d) ---", loop_count);

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
                print_current_time();

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
            }
        }
        else
        {
            ESP_LOGI(TAG, "WiFi未连接，尝试重新连接...");
            // 尝试重新连接
            esp_err_t reconnect_ret = wifi_manager_connect_with_config(&wifi_config);
            if (reconnect_ret != ESP_OK)
            {
                ESP_LOGE(TAG, "重新连接失败: %s", esp_err_to_name(reconnect_ret));
            }
        }
    }
}

void initialize_sntp(void)
{
    // 增加栈大小以避免栈溢出，特别是Tmr Svc服务
    xTaskCreate(sntp_task_main, "sntp_task", 8192, NULL, 5, NULL); // 从4096增加到8192
}