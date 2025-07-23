/*
 * WiFi连接管理器实现文件
 * 提供完整的WiFi连接、断开、扫描和状态管理功能
 */

#include "wifi_service.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi_manager"; // 日志标签

// WiFi默认配置 - 可通过 wifi_manager_connect_to_ap() 覆盖
#define DEFAULT_WIFI_SSID      "FYMTC"      // 默认WiFi名称
#define DEFAULT_WIFI_PASSWORD  "1234567891"  // 默认WiFi密码
#define WIFI_MAXIMUM_RETRY     5             // 最大重试次数

// WiFi事件组位定义
#define WIFI_CONNECTED_BIT BIT0    // WiFi连接成功标志位
#define WIFI_FAIL_BIT      BIT1    // WiFi连接失败标志位
#define WIFI_SCAN_DONE_BIT BIT2    // WiFi扫描完成标志位

// WiFi相关全局变量
static EventGroupHandle_t s_wifi_event_group = NULL;  // WiFi事件组句柄
static esp_netif_t *sta_netif = NULL;                 // STA网络接口
static int s_retry_num = 0;                           // 当前重试次数
static wifi_manager_status_t s_wifi_status = WIFI_MANAGER_DISCONNECTED; // WiFi连接状态
static char s_connected_ssid[WIFI_MANAGER_MAX_SSID_LEN] = {0}; // 当前连接的SSID
static wifi_manager_status_cb_t s_status_callback = NULL;     // 状态变化回调函数
static bool s_wifi_initialized = false;               // WiFi是否已初始化

// 扫描相关变量
static bool s_scan_in_progress = false;              // 扫描是否在进行中
static wifi_ap_record_t s_scan_results[WIFI_MANAGER_MAX_SCAN_RESULTS]; // 扫描结果
static uint16_t s_scan_count = 0;                    // 扫描结果数量

/**
 * 更新WiFi状态并调用回调函数
 * @param new_status 新的状态
 * @param ssid 连接的SSID（可为NULL）
 */
static void update_wifi_status(wifi_manager_status_t new_status, const char* ssid)
{
    if (s_wifi_status != new_status) {
        s_wifi_status = new_status;
        
        // 更新连接的SSID
        if (ssid && new_status == WIFI_MANAGER_CONNECTED) {
            strncpy(s_connected_ssid, ssid, sizeof(s_connected_ssid) - 1);
            s_connected_ssid[sizeof(s_connected_ssid) - 1] = '\0';
        } else if (new_status == WIFI_MANAGER_DISCONNECTED) {
            memset(s_connected_ssid, 0, sizeof(s_connected_ssid));
        }
        
        // 调用状态回调函数
        if (s_status_callback) {
            s_status_callback(new_status, ssid);
        }
        
        ESP_LOGI(TAG, "WiFi status changed to: %d", new_status);
    }
}

/**
 * WiFi事件处理函数
 * @param arg 用户参数
 * @param event_base 事件基础类型
 * @param event_id 事件ID
 * @param event_data 事件数据
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started");
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                update_wifi_status(WIFI_MANAGER_DISCONNECTED, NULL);
                
                if (s_retry_num < WIFI_MAXIMUM_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    update_wifi_status(WIFI_MANAGER_CONNECTING, NULL);
                    ESP_LOGI(TAG, "Retry to connect to AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
                } else {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    update_wifi_status(WIFI_MANAGER_FAILED, NULL);
                    ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", WIFI_MAXIMUM_RETRY);
                }
                break;
            }
            
            case WIFI_EVENT_SCAN_DONE: {
                wifi_event_sta_scan_done_t* event = (wifi_event_sta_scan_done_t*) event_data;
                ESP_LOGI(TAG, "WiFi scan completed, found %d APs", event->number);
                s_scan_in_progress = false;
                xEventGroupSetBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
                break;
            }
            
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected! Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        update_wifi_status(WIFI_MANAGER_CONNECTED, s_connected_ssid);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * 初始化WiFi基础设施
 */
static esp_err_t wifi_manager_init(void)
{
    if (s_wifi_initialized) {
        return ESP_OK;
    }
    
    // 创建事件组
    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            ESP_LOGE(TAG, "Failed to create WiFi event group");
            return ESP_FAIL;
        }
    }

    // 初始化网络接口
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    // 创建默认事件循环
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    // 创建默认WiFi STA接口
    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == NULL) {
            ESP_LOGE(TAG, "Failed to create default WiFi STA interface");
            return ESP_FAIL;
        }
    }

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    s_wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(void)
{
    return wifi_manager_connect_to_ap(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
}

esp_err_t wifi_manager_connect_to_ap(const char* ssid, const char* password)
{
    if (!ssid) {
        ESP_LOGE(TAG, "SSID cannot be NULL");
        return ESP_FAIL;
    }
    
    if (strlen(ssid) >= WIFI_MANAGER_MAX_SSID_LEN) {
        ESP_LOGE(TAG, "SSID too long");
        return ESP_FAIL;
    }
    
    if (password && strlen(password) >= WIFI_MANAGER_MAX_PASSWORD_LEN) {
        ESP_LOGE(TAG, "Password too long");
        return ESP_FAIL;
    }

    // 初始化WiFi（如果尚未初始化）
    if (wifi_manager_init() != ESP_OK) {
        return ESP_FAIL;
    }

    // 如果已经连接到相同的SSID，直接返回成功
    if (s_wifi_status == WIFI_MANAGER_CONNECTED && 
        strcmp(s_connected_ssid, ssid) == 0) {
        ESP_LOGI(TAG, "Already connected to %s", ssid);
        return ESP_OK;
    }

    // 检查SSID和密码是否为默认配置
    if (strcmp(ssid, "FYMTC") == 0 && strcmp(password, "1234567891") == 0) {
        ESP_LOGW(TAG, "Using default WiFi credentials. Please configure your actual WiFi settings.");
    }

    // 断开当前连接
    if (s_wifi_status == WIFI_MANAGER_CONNECTED) {
        esp_wifi_disconnect();
    }

    // 配置WiFi连接参数
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 保存要连接的SSID
    strncpy(s_connected_ssid, ssid, sizeof(s_connected_ssid) - 1);
    s_connected_ssid[sizeof(s_connected_ssid) - 1] = '\0';
    
    s_retry_num = 0;
    update_wifi_status(WIFI_MANAGER_CONNECTING, ssid);
    
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start connection: %s", esp_err_to_name(ret));
        update_wifi_status(WIFI_MANAGER_FAILED, NULL);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);

    // 等待连接结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID: %s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi SSID: %s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Unexpected WiFi connection event");
        return ESP_FAIL;
    }
}

void wifi_manager_disconnect(void)
{
    if (s_wifi_status == WIFI_MANAGER_CONNECTED || s_wifi_status == WIFI_MANAGER_CONNECTING) {
        esp_wifi_disconnect();
    }
    
    if (sta_netif) {
        esp_err_t ret = esp_wifi_stop();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
        }
        
        ret = esp_wifi_deinit();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to deinit WiFi: %s", esp_err_to_name(ret));
        }
        
        esp_netif_destroy_default_wifi(sta_netif);
        sta_netif = NULL;
    }

    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    s_wifi_initialized = false;
    s_retry_num = 0;
    memset(s_connected_ssid, 0, sizeof(s_connected_ssid));
    update_wifi_status(WIFI_MANAGER_DISCONNECTED, NULL);
    
    ESP_LOGI(TAG, "WiFi disconnected and resources cleaned up");
}

bool wifi_manager_is_connected(void)
{
    return s_wifi_status == WIFI_MANAGER_CONNECTED;
}

esp_err_t wifi_manager_get_connected_ssid(char* ssid_buf, size_t buf_len)
{
    if (!ssid_buf || buf_len == 0) {
        return ESP_FAIL;
    }
    
    if (s_wifi_status != WIFI_MANAGER_CONNECTED) {
        return ESP_FAIL;
    }
    
    if (strlen(s_connected_ssid) >= buf_len) {
        return ESP_FAIL;
    }
    
    strncpy(ssid_buf, s_connected_ssid, buf_len - 1);
    ssid_buf[buf_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t wifi_manager_start_scan(bool block)
{
    if (!s_wifi_initialized) {
        if (wifi_manager_init() != ESP_OK) {
            return ESP_FAIL;
        }
    }
    
    if (s_scan_in_progress) {
        ESP_LOGW(TAG, "Scan already in progress");
        return ESP_FAIL;
    }
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };
    
    esp_err_t ret = esp_wifi_scan_start(&scan_config, block);
    if (ret == ESP_OK) {
        s_scan_in_progress = true;
        ESP_LOGI(TAG, "WiFi scan started");
        
        if (block) {
            // 等待扫描完成
            xEventGroupWaitBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT, 
                               pdTRUE, pdFALSE, portMAX_DELAY);
        }
    } else {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t wifi_manager_stop_scan(void)
{
    if (!s_scan_in_progress) {
        ESP_LOGW(TAG, "No scan in progress");
        return ESP_OK;
    }
    
    esp_err_t ret = esp_wifi_scan_stop();
    if (ret == ESP_OK) {
        s_scan_in_progress = false;
        ESP_LOGI(TAG, "WiFi scan stopped");
    } else {
        ESP_LOGE(TAG, "Failed to stop WiFi scan: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t wifi_manager_get_scan_results(wifi_manager_ap_info_t* ap_list, 
                                       uint16_t max_count, 
                                       uint16_t* actual_count)
{
    if (!ap_list || !actual_count || max_count == 0) {
        return ESP_FAIL;
    }
    
    uint16_t number = WIFI_MANAGER_MAX_SCAN_RESULTS;
    esp_err_t ret = esp_wifi_scan_get_ap_records(&number, s_scan_results);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(ret));
        *actual_count = 0;
        return ret;
    }
    
    s_scan_count = number;
    uint16_t copy_count = (number < max_count) ? number : max_count;
    
    for (uint16_t i = 0; i < copy_count; i++) {
        strncpy(ap_list[i].ssid, (char*)s_scan_results[i].ssid, 
                WIFI_MANAGER_MAX_SSID_LEN - 1);
        ap_list[i].ssid[WIFI_MANAGER_MAX_SSID_LEN - 1] = '\0';
        ap_list[i].rssi = s_scan_results[i].rssi;
        ap_list[i].auth_mode = s_scan_results[i].authmode;
        ap_list[i].is_open = (s_scan_results[i].authmode == WIFI_AUTH_OPEN);
    }
    
    *actual_count = copy_count;
    ESP_LOGI(TAG, "Retrieved %d scan results", copy_count);
    return ESP_OK;
}

esp_err_t wifi_manager_register_status_callback(wifi_manager_status_cb_t callback)
{
    if (!callback) {
        return ESP_FAIL;
    }
    
    s_status_callback = callback;
    ESP_LOGI(TAG, "Status callback registered");
    return ESP_OK;
}

esp_err_t wifi_manager_unregister_status_callback(void)
{
    s_status_callback = NULL;
    ESP_LOGI(TAG, "Status callback unregistered");
    return ESP_OK;
}

wifi_manager_status_t wifi_manager_get_status(void)
{
    return s_wifi_status;
}

esp_err_t wifi_manager_get_rssi(int8_t* rssi)
{
    if (!rssi) {
        return ESP_FAIL;
    }
    
    if (s_wifi_status != WIFI_MANAGER_CONNECTED) {
        return ESP_FAIL;
    }
    
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        *rssi = ap_info.rssi;
        ESP_LOGD(TAG, "WiFi RSSI: %d dBm", *rssi);
    } else {
        ESP_LOGE(TAG, "Failed to get AP info: %s", esp_err_to_name(ret));
    }
    
    return ret;
}
