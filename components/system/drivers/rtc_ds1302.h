#pragma once

#include "ds1302.h"
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// DS1302 引脚定义
#define RTC_SCLK_GPIO   GPIO_NUM_40
#define RTC_IO_GPIO     GPIO_NUM_41  
#define RTC_RST_GPIO    GPIO_NUM_42

// RTC初始化状态
typedef enum {
    RTC_NOT_INITIALIZED = 0,
    RTC_INITIALIZED,
    RTC_ERROR
} rtc_status_t;

/**
 * @brief 初始化DS1302 RTC
 * @return esp_err_t 
 */
esp_err_t rtc_ds1302_init(void);

/**
 * @brief 反初始化DS1302 RTC
 */
void rtc_ds1302_deinit(void);

/**
 * @brief 获取RTC状态
 * @return rtc_status_t 
 */
rtc_status_t rtc_ds1302_get_status(void);

/**
 * @brief 从DS1302获取时间
 * @param time 时间结构体指针
 * @return esp_err_t 
 */
esp_err_t rtc_ds1302_get_time(struct tm *time);

/**
 * @brief 设置DS1302时间
 * @param time 时间结构体指针
 * @return esp_err_t 
 */
esp_err_t rtc_ds1302_set_time(const struct tm *time);

/**
 * @brief 启动/停止RTC时钟
 * @param start true启动，false停止
 * @return esp_err_t 
 */
esp_err_t rtc_ds1302_start_clock(bool start);

/**
 * @brief 检查RTC时钟是否在运行
 * @param running 运行状态指针
 * @return esp_err_t 
 */
esp_err_t rtc_ds1302_is_running(bool *running);

/**
 * @brief 设置写保护
 * @param wp true启用写保护，false禁用
 * @return esp_err_t 
 */
esp_err_t rtc_ds1302_set_write_protect(bool wp);

/**
 * @brief 从系统时间同步到RTC（用于SNTP校准）
 * @return esp_err_t 
 */
esp_err_t rtc_ds1302_sync_from_system_time(void);

#ifdef __cplusplus
}
#endif
