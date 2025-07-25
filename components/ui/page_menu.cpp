#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include <iostream>
#include <cstring>
#include <time.h>
#include <sys/time.h>

#include <esp_sleep.h>//用来关机和重启
#include "system_time.h"
#include "battery_service.h"
#include "wifi_manager.h"
#include "esp_log.h"

extern PageManager g_pageManager;

static const char *TAG = "page_menu";

// 状态栏相关的静态变量
static lv_obj_t *status_bar = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *battery_label = NULL;
static lv_obj_t *wifi_label = NULL;
static lv_timer_t *status_update_timer = NULL;

// 获取电池图标
static const char* get_battery_icon(int percentage, bool is_charging) {
    if (is_charging) {
        return LV_SYMBOL_CHARGE;
    }
    
    if (percentage >= 80) {
        return LV_SYMBOL_BATTERY_FULL;
    } else if (percentage >= 60) {
        return LV_SYMBOL_BATTERY_3;
    } else if (percentage >= 40) {
        return LV_SYMBOL_BATTERY_2;
    } else if (percentage >= 20) {
        return LV_SYMBOL_BATTERY_1;
    } else {
        return LV_SYMBOL_BATTERY_EMPTY;
    }
}

// 更新状态栏信息
static void update_status_bar(void) {
    if (!status_bar) return;
    
    // 更新时间 - 直接使用系统时间
    if (time_label) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        static int last_minute = -1;  // 记录上次的分钟，避免频繁日志
        int current_minute = timeinfo.tm_min;
        
        // 检查时间是否有效（年份应该大于2020）
        if (timeinfo.tm_year >= (2020 - 1900)) {
            // 时间已同步，显示当前时间
            char time_str[32];
            strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
            lv_label_set_text(time_label, time_str);
            
            // 只在分钟变化时输出日志
            if (current_minute != last_minute) {
                ESP_LOGI(TAG, "Status bar time updated to: %s (year=%d)", time_str, timeinfo.tm_year + 1900);
                last_minute = current_minute;
            }
        } else {
            // 时间未同步，显示未同步状态
            lv_label_set_text(time_label, "--:--");
            
            // 只在分钟变化时输出日志
            if (current_minute != last_minute) {
                ESP_LOGI(TAG, "Status bar time not synced, year=%d", timeinfo.tm_year + 1900);
                last_minute = current_minute;
            }
        }
    }
    
    // 更新电池状态
    if (battery_label) {
        battery_service::BatteryInfo battery_info = battery_service::get_battery_info();
        if (battery_info.is_valid) {
            const char* battery_icon = get_battery_icon(battery_info.percentage, battery_info.is_charging);
            lv_label_set_text_fmt(battery_label, "%s %d%%", battery_icon, battery_info.percentage);
        } else {
            lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_EMPTY " --%");
        }
    }
    
    // 更新WiFi状态
    if (wifi_label) {
        if (wifi_manager_is_connected()) {
            lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x4CAF50), 0); // 绿色表示连接
        } else {
            lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
            lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x666666), 0); // 灰色表示未连接
        }
    }
}

// 状态更新定时器回调
static void status_update_timer_cb(lv_timer_t * timer) {
    update_status_bar();
}

// 创建状态栏
static lv_obj_t* create_status_bar(lv_obj_t *parent) {
    // 创建状态栏容器
    lv_obj_t *status_container = lv_obj_create(parent);
    lv_obj_set_size(status_container, LV_PCT(100), 30);
    //lv_obj_set_style_bg_color(status_container, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_border_width(status_container, 0, 0);
    lv_obj_set_style_radius(status_container, 0, 0);
    lv_obj_set_style_pad_all(status_container, 5, 0);
    lv_obj_align(status_container, LV_ALIGN_TOP_MID, 0, 0);
    
    // 时间标签（左侧）
    time_label = lv_label_create(status_container);
    lv_label_set_text(time_label, "--:--");
    //lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 0, 0);
    
    // WiFi状态标签（时间右侧）
    wifi_label = lv_label_create(status_container);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    //lv_obj_set_style_text_color(wifi_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_14, 0);
    //lv_obj_align(wifi_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align_to(wifi_label,time_label, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    
    // 电池状态标签（右侧）
    battery_label = lv_label_create(status_container);
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_EMPTY " --%");
    //lv_obj_set_style_text_color(battery_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, 0, 0);
    
    return status_container;
}

// 定义菜单项结构体
typedef struct {
    const char *icon;
    const char *title;
    void (*action)(lv_event_t *e); // 使用函数指针实现多态
} MenuItem;

// 菜单项数组
static const MenuItem menu_items[] = {
    {LV_SYMBOL_SETTINGS, "Settings", [](lv_event_t *e){
        g_pageManager.gotoPage("page_settings", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Settings clicked!"<<std::endl; }},
    {LV_SYMBOL_WIFI, "WIFI", [](lv_event_t *e){
        g_pageManager.gotoPage("page_wifi", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"WIFI clicked!"<<std::endl; }},
    {"\xEF\x80\x97", "TIME", [](lv_event_t *e){
        g_pageManager.gotoPage("page_time", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"TIME clicked!"<<std::endl; }},
    {"\xEF\x8B\x9B", "PMU", [](lv_event_t *e){
        g_pageManager.gotoPage("page_pmu", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"PMU clicked!"<<std::endl; }},
    {"\xEF\x84\xA4", "MPU6050", [](lv_event_t *e){
        g_pageManager.gotoPage("page_mpu6050", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"MPU6050 clicked!"<<std::endl; }},
    {"\xEF\x80\x84", "MAX30105", [](lv_event_t *e){
        g_pageManager.gotoPage("page_max30105", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"MAX30105 clicked!"<<std::endl; }},
    {LV_SYMBOL_FILE, "cube game", [](lv_event_t *e){
        g_pageManager.gotoPage("page_cube_game", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Cube Game clicked!"<<std::endl; }},
    {LV_SYMBOL_FILE, "ball game", [](lv_event_t *e){
        g_pageManager.gotoPage("page_ball_game", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Ball Game clicked!"<<std::endl; }},
    {LV_SYMBOL_FILE, "pvz", [](lv_event_t *e){
        g_pageManager.gotoPage("page_pvz", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"PVZ clicked!"<<std::endl; }},
    {LV_SYMBOL_FILE, "fly game", [](lv_event_t *e){
        g_pageManager.gotoPage("page_fly_game", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Fly Game clicked!"<<std::endl; }},
    {LV_SYMBOL_SD_CARD, "SD files", [](lv_event_t *e){
        g_pageManager.gotoPage("page_sd_files", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"SD Files clicked!"<<std::endl; }},

    //*******//

    {LV_SYMBOL_REFRESH, "Restart", [](lv_event_t *e){
     std::cout<<"Restart clicked!"<<std::endl;
     //重启
     esp_restart();
     }},
    {LV_SYMBOL_POWER, "OFF", [](lv_event_t *e){
     std::cout<<"OFF clicked!"<<std::endl; 
     //关机
     esp_deep_sleep_start();
     }},
};

// 初始化样式
static void init_styles(lv_style_t *style) {
    lv_style_init(style);
    lv_style_set_text_font(style, &NotoSansSC_Medium_3500);
}

// 通用事件回调函数
static void menu_event_handler(lv_event_t *e) {
    const MenuItem *item = (const MenuItem *)lv_event_get_user_data(e);
    if (item && item->action) {
        // 在离开页面前清理资源
        if (status_update_timer) {
            lv_timer_del(status_update_timer);
            status_update_timer = NULL;
            ESP_LOGI(TAG, "Menu status update timer stopped");
        }
        
        item->action(e); // 调用对应的函数指针
    }
}

// 页面清理函数（当页面被销毁时调用）
void cleanup_menu_page(void) {
    ESP_LOGI(TAG, "Cleaning up menu page");
    
    if (status_update_timer) {
        lv_timer_del(status_update_timer);
        status_update_timer = NULL;
    }
    
    // 重置静态变量
    status_bar = NULL;
    time_label = NULL;
    battery_label = NULL;
    wifi_label = NULL;
    
    ESP_LOGI(TAG, "Menu page cleanup completed");
}

lv_obj_t* createPage_menu(){
    ESP_LOGI(TAG, "Creating menu page with status bar");

    lv_obj_t *main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0xF0F0F0), 0);
    
    // 创建状态栏
    status_bar = create_status_bar(main_screen);
    
    // 创建菜单列表容器
    lv_obj_t *menu_container = lv_obj_create(main_screen);
    lv_obj_set_size(menu_container, LV_PCT(100), LV_VER_RES - 35); // 减去状态栏高度
    lv_obj_set_style_bg_opa(menu_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 0, 0);
    lv_obj_align_to(menu_container, status_bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    
    // 创建菜单列表
    lv_obj_t *list = lv_list_create(menu_container);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 0);

    static lv_style_t style;
    init_styles(&style);

    for (size_t i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); i++) {
        lv_obj_t *btn = lv_list_add_btn(list, menu_items[i].icon, menu_items[i].title);
        lv_obj_add_event_cb(btn, menu_event_handler, LV_EVENT_CLICKED, (void *)&menu_items[i]);
        lv_obj_add_style(btn, &style, 0);

        // 特殊样式处理
        if (strcmp(menu_items[i].title, "Restart") == 0) {
            lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (strcmp(menu_items[i].title, "OFF") == 0) {
            lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
    
    // 初始化服务（如果需要）
    // 这些服务在初始化任务中已经初始化，这里只是确保它们可用
    // if (!system_time::is_time_synced()) {
    //     ESP_LOGW(TAG, "System time not yet synchronized");
    // }
    
    // if (!battery_service::is_available()) {
    //     ESP_LOGW(TAG, "Battery service not available");
    // }
    
    // esp_err_t ret = wifi_manager_init();
    // if (ret != ESP_OK) {
    //     ESP_LOGW(TAG, "WiFi manager not available: %s", esp_err_to_name(ret));
    // }
    
    // 立即更新状态栏
    update_status_bar();
    
    // 启动定时器，每秒更新状态栏
    status_update_timer = lv_timer_create(status_update_timer_cb, 1000, NULL);
    
    ESP_LOGI(TAG, "Menu page created with status bar and update timer");
    
    return main_screen;
}
