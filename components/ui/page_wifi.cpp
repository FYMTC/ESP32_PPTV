#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

extern PageManager g_pageManager;

static const char *TAG = "page_wifi";

// WiFi页面相关的静态变量
static lv_obj_t *wifi_list = NULL;               // WiFi列表容器
static lv_obj_t *status_label = NULL;            // 状态标签
static lv_obj_t *refresh_btn = NULL;             // 刷新按钮
static lv_timer_t *scan_timer = NULL;            // 扫描定时器
static char connected_ssid[WIFI_MANAGER_MAX_SSID_LEN] = {0}; // 当前连接的SSID

// 前向声明
static void refresh_wifi_list(void);
static void wifi_status_callback(wifi_manager_status_t status, const char* ssid);

/**
 * 返回按钮回调函数
 */
static void screen_backbtn_cb(lv_event_t * e) {
    // 停止扫描定时器
    if (scan_timer) {
        lv_timer_del(scan_timer);
        scan_timer = NULL;
    }
    
    // 注销状态回调
    wifi_manager_unregister_status_callback();
    
    g_pageManager.back(LV_SCR_LOAD_ANIM_FADE_OUT, 300);
}

/**
 * 刷新按钮回调函数
 */
static void refresh_btn_cb(lv_event_t * e) {
    ESP_LOGI(TAG, "Refresh WiFi list requested");
    refresh_wifi_list();
}

/**
 * 获取信号强度图标
 */
static const char* get_signal_icon(int8_t rssi) {
    if (rssi >= -50) {
        return LV_SYMBOL_WIFI;  // 强信号
    } else if (rssi >= -70) {
        return LV_SYMBOL_WIFI;  // 中等信号
    } else {
        return LV_SYMBOL_WIFI;  // 弱信号
    }
}

/**
 * 获取认证类型文本
 */
static const char* get_auth_text(wifi_auth_mode_t auth_mode) {
    switch (auth_mode) {
        case WIFI_AUTH_OPEN:
            return "Open";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3";
        default:
            return "Unknown";
    }
}

/**
 * WiFi项目点击回调
 */
static void wifi_item_cb(lv_event_t * e) {
    const char * ssid = (const char *)lv_event_get_user_data(e);
    
    if (ssid) {
        ESP_LOGI(TAG, "WiFi item clicked: %s", ssid);
        
        // 检查是否已连接到此SSID
        if (strcmp(connected_ssid, ssid) == 0) {
            ESP_LOGI(TAG, "Already connected to %s", ssid);
            return;
        }
        
        // 更新状态显示
        if (status_label) {
            lv_label_set_text_fmt(status_label, "Connecting to %s...", ssid);
        }
        
        // 尝试连接（这里简化处理，实际应该弹出密码输入框）
        // 对于开放网络直接连接，对于加密网络应该提示输入密码
        //wifi_manager_connect_to_ap(ssid, NULL);
    }
    
    // 释放分配的内存
    if (ssid) {
        free((void*)ssid);
    }
}

/**
 * WiFi状态变化回调函数
 */
static void wifi_status_callback(wifi_manager_status_t status, const char* ssid) {
    if (!status_label) return;
    
    // 确保在LVGL线程中执行UI更新
    switch (status) {
        case WIFI_MANAGER_DISCONNECTED:
            lv_label_set_text(status_label, "WiFi: Disconnected");
            memset(connected_ssid, 0, sizeof(connected_ssid));
            refresh_wifi_list(); // 刷新列表以更新连接状态
            break;
            
        case WIFI_MANAGER_CONNECTING:
            lv_label_set_text_fmt(status_label, "WiFi: Connecting to %s", ssid ? ssid : "...");
            break;
            
        case WIFI_MANAGER_CONNECTED:
            lv_label_set_text_fmt(status_label, "WiFi: Connected to %s", ssid ? ssid : "Unknown");
            if (ssid) {
                strncpy(connected_ssid, ssid, sizeof(connected_ssid) - 1);
                connected_ssid[sizeof(connected_ssid) - 1] = '\0';
            }
            refresh_wifi_list(); // 刷新列表以更新连接状态
            break;
            
        case WIFI_MANAGER_CONNECTION_FAILED:
            lv_label_set_text(status_label, "WiFi: Connection failed");
            break;
            
        case WIFI_MANAGER_TIME_SYNCING:
            lv_label_set_text_fmt(status_label, "WiFi: Connected, syncing time...");
            break;
            
        case WIFI_MANAGER_TIME_SYNCED:
            lv_label_set_text_fmt(status_label, "WiFi: Connected, time synced");
            break;
            
        default:
            break;
    }
}

/**
 * 扫描定时器回调
 */
static void scan_timer_cb(lv_timer_t * timer) {
    refresh_wifi_list();
}

/**
 * 刷新WiFi列表
 */
static void refresh_wifi_list(void) {
    if (!wifi_list) return;
    
    ESP_LOGI(TAG, "Refreshing WiFi list");
    
    // 清空现有列表
    lv_obj_clean(wifi_list);
    
    // 更新按钮状态
    if (refresh_btn) {
        lv_obj_add_state(refresh_btn, LV_STATE_DISABLED);
        lv_obj_t *btn_label = lv_obj_get_child(refresh_btn, 0);
        if (btn_label) {
            lv_label_set_text(btn_label, "Scanning...");
        }
    }
    
    // 启动WiFi扫描
    esp_err_t ret = wifi_manager_start_scan(false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi scan");
        
        // 创建错误提示
        lv_obj_t *error_item = lv_obj_create(wifi_list);
        lv_obj_set_size(error_item, LV_PCT(100), 40);
        lv_obj_set_style_bg_color(error_item, lv_color_hex(0xFF6B6B), 0);
        lv_obj_set_style_border_width(error_item, 0, 0);
        
        lv_obj_t *error_label = lv_label_create(error_item);
        lv_label_set_text(error_label, "Scan failed - Please try again");
        lv_obj_align(error_label, LV_ALIGN_CENTER, 0, 0);
        
        // 恢复按钮状态
        if (refresh_btn) {
            lv_obj_clear_state(refresh_btn, LV_STATE_DISABLED);
            lv_obj_t *btn_label = lv_obj_get_child(refresh_btn, 0);
            if (btn_label) {
                lv_label_set_text(btn_label, LV_SYMBOL_REFRESH " Refresh");
            }
        }
        return;
    }
    
    // 延迟获取扫描结果
    lv_timer_t *result_timer = lv_timer_create([](lv_timer_t *t) {
        wifi_manager_ap_info_t ap_list[WIFI_MANAGER_MAX_SCAN_RESULTS];
        uint16_t ap_count = 0;
        
        esp_err_t ret = wifi_manager_get_scan_results(ap_list, WIFI_MANAGER_MAX_SCAN_RESULTS, &ap_count);
        if (ret == ESP_OK && ap_count > 0) {
            ESP_LOGI(TAG, "Found %d WiFi networks", ap_count);
            
            // 重新获取当前连接的SSID，确保是最新的
            char current_ssid[WIFI_MANAGER_MAX_SSID_LEN] = {0};
            esp_err_t ssid_ret = wifi_manager_get_connected_ssid(current_ssid, sizeof(current_ssid));
            if (ssid_ret == ESP_OK && strlen(current_ssid) > 0) {
                strncpy(connected_ssid, current_ssid, sizeof(connected_ssid) - 1);
                connected_ssid[sizeof(connected_ssid) - 1] = '\0';
                ESP_LOGI(TAG, "Currently connected to: %s", connected_ssid);
            } else {
                ESP_LOGI(TAG, "No WiFi currently connected");
                memset(connected_ssid, 0, sizeof(connected_ssid));
            }
            
            // 创建WiFi列表项
            for (uint16_t i = 0; i < ap_count; i++) {
                if (strlen(ap_list[i].ssid) == 0) continue; // 跳过空SSID
                
                ESP_LOGI(TAG, "Processing WiFi: %s", ap_list[i].ssid);
                
                lv_obj_t *wifi_item = lv_btn_create(wifi_list);
                lv_obj_set_size(wifi_item, LV_PCT(100), 50);
                lv_obj_set_style_pad_all(wifi_item, 8, 0);
                lv_obj_set_style_border_width(wifi_item, 1, 0);
                lv_obj_set_style_border_color(wifi_item, lv_color_hex(0xCCCCCC), 0);
                lv_obj_set_style_radius(wifi_item, 5, 0);
                
                // 检查是否为当前连接的WiFi
                bool is_connected = (strlen(connected_ssid) > 0 && 
                                   strcmp(connected_ssid, ap_list[i].ssid) == 0);
                
                ESP_LOGI(TAG, "WiFi %s is_connected: %s (connected_ssid: '%s')", 
                        ap_list[i].ssid, is_connected ? "YES" : "NO", connected_ssid);
                
                if (is_connected) {
                    // 已连接的WiFi使用蓝色背景
                    lv_obj_set_style_bg_color(wifi_item, lv_color_hex(0x2196F3), 0);
                    lv_obj_set_style_text_color(wifi_item, lv_color_white(), 0);
                    ESP_LOGI(TAG, "Set blue background for connected WiFi: %s", ap_list[i].ssid);
                } else {
                    lv_obj_set_style_bg_color(wifi_item, lv_color_white(), 0);
                    lv_obj_set_style_text_color(wifi_item, lv_color_black(), 0);
                }
                
                // 创建WiFi信息布局
                lv_obj_t *content = lv_obj_create(wifi_item);
                lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
                lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(content, 0, 0);
                lv_obj_set_style_pad_all(content, 0, 0);
                
                // SSID名称
                lv_obj_t *ssid_label = lv_label_create(content);
                lv_label_set_text(ssid_label, ap_list[i].ssid);
                lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_14, 0);
                if (is_connected) {
                    lv_obj_set_style_text_color(ssid_label, lv_color_white(), 0);
                }
                lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 0, 0);
                
                // 信号强度和加密类型
                lv_obj_t *info_label = lv_label_create(content);
                lv_label_set_text_fmt(info_label, "%s  %s  %d dBm", 
                                    get_signal_icon(ap_list[i].rssi),
                                    get_auth_text(ap_list[i].auth_mode),
                                    ap_list[i].rssi);
                lv_obj_set_style_text_font(info_label, &lv_font_montserrat_12, 0);
                if (is_connected) {
                    lv_obj_set_style_text_color(info_label, lv_color_white(), 0);
                } else {
                    lv_obj_set_style_text_color(info_label, lv_color_hex(0x666666), 0);
                }
                lv_obj_align(info_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
                
                // 连接状态指示
                if (is_connected) {
                    lv_obj_t *status_icon = lv_label_create(content);
                    lv_label_set_text(status_icon, LV_SYMBOL_OK);
                    lv_obj_set_style_text_color(status_icon, lv_color_white(), 0);
                    lv_obj_align(status_icon, LV_ALIGN_RIGHT_MID, 0, 0);
                    ESP_LOGI(TAG, "Added connection status icon for: %s", ap_list[i].ssid);
                }
                
                // 添加点击事件，传递SSID作为用户数据
                char *ssid_copy = (char*)malloc(strlen(ap_list[i].ssid) + 1);
                if (ssid_copy) {
                    strcpy(ssid_copy, ap_list[i].ssid);
                    lv_obj_add_event_cb(wifi_item, wifi_item_cb, LV_EVENT_CLICKED, ssid_copy);
                }
            }
        } else {
            ESP_LOGW(TAG, "No WiFi networks found or scan failed");
            
            // 创建无结果提示
            lv_obj_t *no_result_item = lv_obj_create(wifi_list);
            lv_obj_set_size(no_result_item, LV_PCT(100), 40);
            lv_obj_set_style_bg_color(no_result_item, lv_color_hex(0xF5F5F5), 0);
            lv_obj_set_style_border_width(no_result_item, 0, 0);
            
            lv_obj_t *no_result_label = lv_label_create(no_result_item);
            lv_label_set_text(no_result_label, "No WiFi networks found");
            lv_obj_set_style_text_color(no_result_label, lv_color_hex(0x666666), 0);
            lv_obj_align(no_result_label, LV_ALIGN_CENTER, 0, 0);
        }
        
        // 恢复刷新按钮状态
        if (refresh_btn) {
            lv_obj_clear_state(refresh_btn, LV_STATE_DISABLED);
            lv_obj_t *btn_label = lv_obj_get_child(refresh_btn, 0);
            if (btn_label) {
                lv_label_set_text(btn_label, LV_SYMBOL_REFRESH " Refresh");
            }
        }
        
        // 删除此定时器
        lv_timer_del(t);
    }, 2000, NULL);
    
    lv_timer_set_repeat_count(result_timer, 1);
}

/**
 * 创建WiFi页面
 */
lv_obj_t* createPage_wifi(void) {
    ESP_LOGI(TAG, "Creating WiFi page");
    
    // 获取当前连接的SSID
    esp_err_t ret = wifi_manager_get_connected_ssid(connected_ssid, sizeof(connected_ssid));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Current connected SSID: %s", connected_ssid);
    } else {
        ESP_LOGW(TAG, "No WiFi connected or failed to get SSID");
        memset(connected_ssid, 0, sizeof(connected_ssid));
    }
    
    lv_obj_t *wifi_page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_page, lv_color_hex(0xF0F0F0), 0);
    
    // 主容器
    lv_obj_t *main_container = lv_obj_create(wifi_page);
    lv_obj_set_size(main_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(main_container, 10, 0);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_align(main_container, LV_ALIGN_CENTER, 0, 0);
    
    // 顶部标题栏
    lv_obj_t *title_bar = lv_obj_create(main_container);
    lv_obj_set_size(title_bar, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 5, 0);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // 返回按钮
    lv_obj_t *screen_backbtn = lv_btn_create(title_bar);
    lv_obj_set_size(screen_backbtn, 80, 20);
    lv_obj_set_style_bg_color(screen_backbtn, lv_color_hex(0x1976D2), 0);
    lv_obj_set_style_border_width(screen_backbtn, 0, 0);
    lv_obj_align(screen_backbtn, LV_ALIGN_LEFT_MID, 5, 0);
    
    lv_obj_t *back_label = lv_label_create(screen_backbtn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_align(back_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(screen_backbtn, screen_backbtn_cb, LV_EVENT_CLICKED, NULL);
    
    // 刷新按钮
    refresh_btn = lv_btn_create(title_bar);
    lv_obj_set_size(refresh_btn, 80, 20);
    lv_obj_set_style_bg_color(refresh_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_border_width(refresh_btn, 0, 0);
    lv_obj_align(refresh_btn, LV_ALIGN_RIGHT_MID, -5, 0);
    
    lv_obj_t *refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, LV_SYMBOL_REFRESH " Refresh");
    lv_obj_set_style_text_color(refresh_label, lv_color_white(), 0);
    lv_obj_align(refresh_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(refresh_btn, refresh_btn_cb, LV_EVENT_CLICKED, NULL);
    
    // 状态标签
    status_label = lv_label_create(main_container);
    lv_label_set_text(status_label, "WiFi: Checking status...");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x333333), 0);
    lv_obj_align_to(status_label, title_bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    // WiFi列表容器
    wifi_list = lv_obj_create(main_container);
    lv_obj_set_size(wifi_list, LV_PCT(100), LV_VER_RES - 100);
    lv_obj_set_style_bg_color(wifi_list, lv_color_white(), 0);
    lv_obj_set_style_border_width(wifi_list, 1, 0);
    lv_obj_set_style_border_color(wifi_list, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_radius(wifi_list, 5, 0);
    lv_obj_set_style_pad_all(wifi_list, 5, 0);
    lv_obj_set_flex_flow(wifi_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wifi_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(wifi_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_align_to(wifi_list, status_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    
    // 注册WiFi状态回调
    wifi_manager_register_status_callback(wifi_status_callback);
    
    // 初始化WiFi管理器（具有防重复初始化保护）
    esp_err_t init_ret = wifi_manager_init();
    if (init_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager: %s", esp_err_to_name(init_ret));
        lv_label_set_text(status_label, "WiFi: Initialization failed");
    } else {
        ESP_LOGI(TAG, "WiFi manager initialized successfully (or already initialized)");
        
        // 获取当前WiFi状态
        wifi_manager_status_t current_status = wifi_manager_get_status();
        ESP_LOGI(TAG, "Current WiFi status: %d", current_status);
        wifi_status_callback(current_status, connected_ssid[0] ? connected_ssid : NULL);
        
        // 自动刷新WiFi列表
        refresh_wifi_list();
        
        // 设置定期扫描定时器（每30秒扫描一次）
        scan_timer = lv_timer_create(scan_timer_cb, 30000, NULL);
    }
    
    return wifi_page;
}