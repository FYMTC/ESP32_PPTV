/*
 * WiFi连接管理器头文件
 * 提供WiFi连接、断开、扫描、状态管理和SNTP时间同步功能
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MANAGER_MAX_SSID_LEN       32    // WiFi SSID最大长度
#define WIFI_MANAGER_MAX_PASSWORD_LEN   64    // WiFi密码最大长度
#define WIFI_MANAGER_MAX_SCAN_RESULTS   20    // 最大扫描结果数量

/**
 * WiFi连接状态枚举
 */
typedef enum {
    WIFI_MANAGER_DISCONNECTED = 0,    // 未连接
    WIFI_MANAGER_CONNECTING,          // 连接中
    WIFI_MANAGER_CONNECTED,           // 已连接
    WIFI_MANAGER_CONNECTION_FAILED,   // 连接失败
    WIFI_MANAGER_TIME_SYNCING,        // 时间同步中
    WIFI_MANAGER_TIME_SYNCED          // 时间同步完成
} wifi_manager_status_t;

/**
 * WiFi配置结构体
 */
typedef struct {
    char ssid[WIFI_MANAGER_MAX_SSID_LEN];           // WiFi SSID
    char password[WIFI_MANAGER_MAX_PASSWORD_LEN];   // WiFi密码
    int maximum_retry;                              // 最大重试次数
    bool auto_reconnect;                            // 是否自动重连
} wifi_manager_config_t;

/**
 * WiFi扫描结果结构体
 */
typedef struct {
    char ssid[WIFI_MANAGER_MAX_SSID_LEN];    // SSID名称
    int8_t rssi;                             // 信号强度
    wifi_auth_mode_t auth_mode;              // 认证模式
    bool is_open;                            // 是否为开放网络
} wifi_manager_ap_info_t;

/**
 * SNTP配置结构体
 */
typedef struct {
    char servers[3][64];                     // NTP服务器数组
    int server_count;                        // 服务器数量
    int timeout_ms;                          // 超时时间（毫秒）
    int sync_mode;                           // 同步模式
} wifi_manager_sntp_config_t;

/**
 * SNTP回调函数类型
 */
typedef void (*wifi_manager_sntp_cb_t)(bool success);

/**
 * WiFi连接状态改变回调函数类型
 * @param status 新的连接状态
 * @param ssid 连接的SSID（如果已连接）
 */
typedef void (*wifi_manager_status_cb_t)(wifi_manager_status_t status, const char* ssid);

/**
 * 时间同步完成回调函数类型
 * @param sync_time 同步的时间
 */
typedef void (*wifi_manager_time_sync_cb_t)(struct tm* sync_time);

/**
 * 初始化WiFi管理器
 * @return ESP_OK 初始化成功，其他值表示失败
 */
esp_err_t wifi_manager_init(void);

/**
 * 使用配置结构体连接WiFi
 * @param config WiFi配置结构体
 * @return ESP_OK 开始连接，其他值表示失败
 */
esp_err_t wifi_manager_connect_with_config(const wifi_manager_config_t* config);

/**
 * 获取WiFi管理器状态
 * @return 当前状态
 */
wifi_manager_status_t wifi_manager_get_status(void);

/**
 * 注册状态回调函数
 * @param cb 回调函数
 */
void wifi_manager_register_status_cb(wifi_manager_status_cb_t cb);

/**
 * 注册SNTP回调函数
 * @param cb 回调函数
 */
void wifi_manager_register_sntp_cb(wifi_manager_sntp_cb_t cb);

/**
 * 初始化WiFi管理器
 * @return ESP_OK 初始化成功，其他值表示失败
 */
esp_err_t wifi_manager_init(void);

/**
 * 使用配置连接WiFi
 * @param config WiFi配置结构体
 * @return ESP_OK 开始连接，其他值表示失败
 */
esp_err_t wifi_manager_connect_with_config(const wifi_manager_config_t* config);

/**
 * 获取当前WiFi状态
 * @return 当前WiFi状态
 */
wifi_manager_status_t wifi_manager_get_status(void);

/**
 * 注册状态回调函数
 * @param cb 状态回调函数
 */
void wifi_manager_register_status_cb(wifi_manager_status_cb_t cb);

/**
 * 注册SNTP回调函数
 * @param cb SNTP回调函数
 */
void wifi_manager_register_sntp_cb(wifi_manager_sntp_cb_t cb);

/**
 * 连接到指定的WiFi网络（使用配置结构体）
 * @param config WiFi连接配置
 * @return ESP_OK 开始连接，ESP_FAIL 参数错误或初始化失败
 */
esp_err_t wifi_manager_connect_with_config(const wifi_manager_config_t* config);

/**
 * 初始化WiFi管理器
 * @return ESP_OK 初始化成功，ESP_FAIL 初始化失败
 */
esp_err_t wifi_manager_init(void);

/**
 * 获取当前WiFi连接状态
 * @return 当前WiFi状态
 */
wifi_manager_status_t wifi_manager_get_status(void);

/**
 * 注册状态回调函数
 * @param cb 回调函数指针
 */
void wifi_manager_register_status_cb(wifi_manager_status_cb_t cb);

/**
 * 注册SNTP回调函数
 * @param cb 回调函数指针
 */
void wifi_manager_register_sntp_cb(wifi_manager_sntp_cb_t cb);

/**
 * 连接到指定的WiFi网络
 * @param ssid WiFi网络名称
 * @param password WiFi密码（开放网络可传NULL）
 * @return ESP_OK 开始连接，ESP_FAIL 参数错误或初始化失败
 */
esp_err_t wifi_manager_connect_to_ap(const char* ssid, const char* password);

/**
 * 断开WiFi连接并清理所有资源
 */
void wifi_manager_disconnect(void);

/**
 * 检查WiFi是否已连接
 * @return true 已连接，false 未连接
 */
bool wifi_manager_is_connected(void);

/**
 * 获取当前连接的WiFi网络名称
 * @param ssid_buf 用于存储SSID的缓冲区
 * @param buf_len 缓冲区长度
 * @return ESP_OK 获取成功，ESP_FAIL 未连接或缓冲区不足
 */
esp_err_t wifi_manager_get_connected_ssid(char* ssid_buf, size_t buf_len);

/**
 * 开始WiFi扫描
 * @param block 是否阻塞等待扫描完成
 * @return ESP_OK 扫描开始成功，ESP_FAIL 扫描失败
 */
esp_err_t wifi_manager_start_scan(bool block);

/**
 * 停止WiFi扫描
 * @return ESP_OK 停止成功，ESP_FAIL 停止失败
 */
esp_err_t wifi_manager_stop_scan(void);

/**
 * 获取WiFi扫描结果列表
 * @param ap_list 存储扫描结果的数组
 * @param max_count 数组最大容量
 * @param actual_count 实际获取到的AP数量（输出参数）
 * @return ESP_OK 获取成功，ESP_FAIL 获取失败
 */
esp_err_t wifi_manager_get_scan_results(wifi_manager_ap_info_t* ap_list, 
                                       uint16_t max_count, 
                                       uint16_t* actual_count);

/**
 * 注册连接状态改变回调函数
 * @param callback 回调函数指针
 * @return ESP_OK 注册成功，ESP_FAIL 注册失败
 */
esp_err_t wifi_manager_register_status_callback(wifi_manager_status_cb_t callback);

/**
 * 注销连接状态改变回调函数
 * @return ESP_OK 注销成功，ESP_FAIL 注销失败
 */
esp_err_t wifi_manager_unregister_status_callback(void);

/**
 * 获取当前WiFi连接状态
 * @return 当前连接状态
 */
wifi_manager_status_t wifi_manager_get_status(void);

/**
 * 获取连接的WiFi信号强度
 * @param rssi 信号强度（输出参数）
 * @return ESP_OK 获取成功，ESP_FAIL 未连接或获取失败
 */
esp_err_t wifi_manager_get_rssi(int8_t* rssi);

/**
 * SNTP时间同步相关接口
 */

/**
 * 配置SNTP设置
 * @param config SNTP配置结构体
 * @return ESP_OK 配置成功，ESP_FAIL 配置失败
 */
esp_err_t wifi_manager_configure_sntp(const wifi_manager_sntp_config_t* config);

/**
 * 手动触发时间同步
 * @return ESP_OK 同步开始成功，ESP_FAIL 同步失败
 */
esp_err_t wifi_manager_sync_time(void);

/**
 * 获取当前时间（已考虑时区）
 * @param time_info 时间信息结构体（输出参数）
 * @return ESP_OK 获取成功，ESP_FAIL 获取失败
 */
esp_err_t wifi_manager_get_time(struct tm* time_info);

/**
 * 获取时间字符串
 * @param time_str 时间字符串缓冲区
 * @param buf_size 缓冲区大小
 * @param format 时间格式（NULL使用默认格式）
 * @return ESP_OK 获取成功，ESP_FAIL 获取失败
 */
esp_err_t wifi_manager_get_time_string(char* time_str, size_t buf_size, const char* format);

/**
 * 检查时间是否已同步
 * @return true 已同步，false 未同步
 */
bool wifi_manager_is_time_synced(void);

/**
 * 注册时间同步完成回调函数
 * @param callback 回调函数指针
 * @return ESP_OK 注册成功，ESP_FAIL 注册失败
 */
esp_err_t wifi_manager_register_time_sync_callback(wifi_manager_time_sync_cb_t callback);

/**
 * 设置时区
 * @param timezone 时区字符串（如"CST-8"）
 * @return ESP_OK 设置成功，ESP_FAIL 设置失败
 */
esp_err_t wifi_manager_set_timezone(const char* timezone);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
