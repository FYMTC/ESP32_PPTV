/* LwIP SNTP 示例

   这个示例代码是公共领域的（或CC0许可证）。

   除非适用法律要求或书面同意，
   这个软件按"原样"分发，不提供任何明示或暗示的保证或条件。

   注意：使用前请修改以下WiFi配置：
   - 编辑 wifi_manager.c 文件中的 WIFI_SSID 和 WIFI_PASSWORD
   - 设置为你的WiFi网络名称和密码
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include "wifi_service.h"

static const char *TAG = "SNTP"; // 日志标签

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48 // IPv6地址字符串最大长度
#endif

/* 保存ESP32从首次启动以来重启次数的变量。
 * 使用RTC_DATA_ATTR放置在RTC内存中，
 * 当ESP32从深度睡眠唤醒时保持其值不变。
 */
RTC_DATA_ATTR static int boot_count = 0;

static void obtain_time(void); // 获取时间的函数声明

/**
 * 时间同步通知回调函数
 * @param tv 时间值结构体指针
 */
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event"); // 记录时间同步事件通知
}

/**
 * 应用程序主函数
 */
void initialize_sntp(void)
{
    ++boot_count;                                // 增加启动计数
    ESP_LOGI(TAG, "Boot count: %d", boot_count); // 记录启动次数

    time_t now;                   // 当前时间
    struct tm timeinfo;           // 时间信息结构体
    time(&now);                   // 获取当前时间
    localtime_r(&now, &timeinfo); // 将时间转换为本地时间
    // 检查时间是否已设置？如果没有，tm_year将是(1970 - 1900)。
    if (timeinfo.tm_year < (2016 - 1900))
    {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP."); // 时间未设置，通过NTP获取时间
        obtain_time();                                                                       // 获取时间
        // 用当前时间更新'now'变量
        time(&now);
    }

    char strftime_buf[64]; // 格式化时间字符串缓冲区

    // 设置时区为中国标准时间（东八区）
    setenv("TZ", "CST-8", 1);
    tzset(); // 应用时区设置
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);                // 格式化时间
    ESP_LOGI(TAG, "The current date/time in China (UTC+8) is: %s", strftime_buf); // 显示中国时间

    // 如果使用平滑同步模式，等待时间调整完成
    // if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH)
    // {
    //     struct timeval outdelta;
    //     while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS)
    //     {
    //         adjtime(NULL, &outdelta); // 获取时间调整的剩余量
    //         ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %jd sec: %li ms: %li us",
    //                  (intmax_t)outdelta.tv_sec,
    //                  outdelta.tv_usec / 1000,
    //                  outdelta.tv_usec % 1000);     // 显示剩余调整时间
    //         vTaskDelay(2000 / portTICK_PERIOD_MS); // 延时2秒
    //     }
    // }
}

/**
 * 打印配置的NTP服务器列表
 */
static void print_servers(void)
{
    ESP_LOGI(TAG, "List of configured NTP servers:"); // 显示已配置的NTP服务器列表

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i)
    {
        if (esp_sntp_getservername(i))
        {                                                                 // 如果服务器有名称
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i)); // 显示服务器名称
        }
        else
        {
            // 我们有IPv4或IPv6地址，打印它
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i); // 获取服务器IP地址
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff); // 显示服务器IP地址
        }
    }
}

/**
 * 获取网络时间的主要函数
 */
static void obtain_time(void)
{
    ESP_ERROR_CHECK(nvs_flash_init()); // 初始化NVS闪存

    /* 使用WiFi管理器连接到网络 */
    ESP_LOGI(TAG, "Starting WiFi connection...");
    if (!wifi_manager_is_connected())
    {
        esp_err_t ret = wifi_manager_connect(); // 连接到WiFi
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to connect to WiFi: %s", esp_err_to_name(ret)); // 连接失败
            return;
        }
    }
    ESP_LOGI(TAG, "Initializing and starting SNTP"); // 初始化并启动SNTP
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    /* 这演示了配置多个服务器
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                                                                      ESP_SNTP_SERVER_LIST(CONFIG_SNTP_TIME_SERVER, "pool.ntp.org")); // 多服务器配置
#else
    /*
     * 基本的默认配置，使用一个服务器并启动服务
     */
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org"); // 单服务器配置
#endif
    config.sync_cb = time_sync_notification_cb; // 注意：只有当我们需要回调函数时才需要
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    config.smooth_sync = true; // 启用平滑同步
#endif

    esp_netif_sntp_init(&config); // 初始化SNTP配置

    print_servers(); // 打印服务器信息

    // 等待时间被设置
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;              // 重试计数器
    const int retry_count = 15; // 最大重试次数
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count); // 等待系统时间设置
    }
    time(&now);                   // 获取当前时间
    localtime_r(&now, &timeinfo); // 转换为本地时间

    esp_netif_sntp_deinit();   // 反初始化SNTP
}
