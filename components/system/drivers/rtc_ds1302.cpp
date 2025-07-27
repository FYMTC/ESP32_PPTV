#include "rtc_ds1302.h"
#include "esp_log.h"
#include "esp_err.h"
#include <sys/time.h>
#include <string.h>

static const char *TAG = "RTC_DS1302";

// DS1302设备句柄
static ds1302_t rtc_device = {
    .ce_pin = RTC_RST_GPIO,    // CE引脚连接到RST
    .io_pin = RTC_IO_GPIO,     // IO引脚
    .sclk_pin = RTC_SCLK_GPIO, // SCLK引脚
    .ch = false                // 时钟停止标志
};

static rtc_status_t rtc_status = RTC_NOT_INITIALIZED;

esp_err_t rtc_ds1302_init(void)
{
    if (rtc_status == RTC_INITIALIZED) {
        ESP_LOGW(TAG, "DS1302 already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing DS1302 RTC...");
    ESP_LOGI(TAG, "Pin configuration: RST=%d, IO=%d, SCLK=%d", 
             RTC_RST_GPIO, RTC_IO_GPIO, RTC_SCLK_GPIO);

    // 初始化DS1302设备
    esp_err_t ret = ds1302_init(&rtc_device);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DS1302: %s", esp_err_to_name(ret));
        rtc_status = RTC_ERROR;
        return ret;
    }

    // 禁用写保护以便配置
    ret = ds1302_set_write_protect(&rtc_device, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable write protection: %s", esp_err_to_name(ret));
        rtc_status = RTC_ERROR;
        return ret;
    }

    // 启动时钟
    ret = ds1302_start(&rtc_device, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start DS1302 clock: %s", esp_err_to_name(ret));
        rtc_status = RTC_ERROR;
        return ret;
    }

    // 检查时钟是否正在运行
    bool running = false;
    ret = ds1302_is_running(&rtc_device, &running);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "DS1302 clock is %s", running ? "running" : "stopped");
    }

    // 读取当前时间进行测试
    struct tm rtc_time;
    ret = ds1302_get_time(&rtc_device, &rtc_time);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Current RTC time: %04d-%02d-%02d %02d:%02d:%02d", 
                 rtc_time.tm_year + 1900, rtc_time.tm_mon + 1, rtc_time.tm_mday,
                 rtc_time.tm_hour, rtc_time.tm_min, rtc_time.tm_sec);
    } else {
        ESP_LOGW(TAG, "Failed to read RTC time: %s", esp_err_to_name(ret));
    }

    rtc_status = RTC_INITIALIZED;
    ESP_LOGI(TAG, "DS1302 RTC initialized successfully");
    return ESP_OK;
}

void rtc_ds1302_deinit(void)
{
    if (rtc_status == RTC_NOT_INITIALIZED) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing DS1302 RTC");
    rtc_status = RTC_NOT_INITIALIZED;
}

rtc_status_t rtc_ds1302_get_status(void)
{
    return rtc_status;
}

esp_err_t rtc_ds1302_get_time(struct tm *time)
{
    if (rtc_status != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "DS1302 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (time == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: time is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ds1302_get_time(&rtc_device, time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get time from DS1302: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t rtc_ds1302_set_time(const struct tm *time)
{
    if (rtc_status != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "DS1302 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (time == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: time is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Setting DS1302 time to: %04d-%02d-%02d %02d:%02d:%02d", 
             time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
             time->tm_hour, time->tm_min, time->tm_sec);

    // 禁用写保护
    esp_err_t ret = ds1302_set_write_protect(&rtc_device, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable write protection: %s", esp_err_to_name(ret));
        return ret;
    }

    // 设置时间
    ret = ds1302_set_time(&rtc_device, time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set DS1302 time: %s", esp_err_to_name(ret));
        return ret;
    }

    // 重新启用写保护（可选）
    ds1302_set_write_protect(&rtc_device, true);

    ESP_LOGI(TAG, "DS1302 time set successfully");
    return ESP_OK;
}

esp_err_t rtc_ds1302_start_clock(bool start)
{
    if (rtc_status != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "DS1302 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ds1302_start(&rtc_device, start);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to %s DS1302 clock: %s", 
                 start ? "start" : "stop", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "DS1302 clock %s", start ? "started" : "stopped");
    }

    return ret;
}

esp_err_t rtc_ds1302_is_running(bool *running)
{
    if (rtc_status != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "DS1302 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (running == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: running is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    return ds1302_is_running(&rtc_device, running);
}

esp_err_t rtc_ds1302_set_write_protect(bool wp)
{
    if (rtc_status != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "DS1302 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ds1302_set_write_protect(&rtc_device, wp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set write protection: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "DS1302 write protection %s", wp ? "enabled" : "disabled");
    }

    return ret;
}

esp_err_t rtc_ds1302_sync_from_system_time(void)
{
    if (rtc_status != RTC_INITIALIZED) {
        ESP_LOGE(TAG, "DS1302 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 获取系统时间
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // 检查系统时间是否有效（年份应该大于2020）
    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "System time not synchronized, cannot sync RTC");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Syncing DS1302 from system time: %04d-%02d-%02d %02d:%02d:%02d", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // 将系统时间写入RTC
    esp_err_t ret = rtc_ds1302_set_time(&timeinfo);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "DS1302 synchronized with system time successfully");
    } else {
        ESP_LOGE(TAG, "Failed to sync DS1302 with system time");
    }

    return ret;
}
