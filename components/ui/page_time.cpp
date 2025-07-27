#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include "time_service.h"
#include "battery_service.h"
#include "wifi_manager.h"
#include "esp_log.h"

extern PageManager g_pageManager;
static const char *TAG = "TIME PAGE";

// 全局变量
lv_obj_t *label_time;
lv_obj_t *label_second;	
lv_obj_t *label_running;
lv_obj_t *label_BATTERY;
lv_obj_t *label_weather;
static lv_timer_t *ui_update_timer = NULL;
static battery_service::BatteryInfo g_battery_info = {};

void time_page_cb(lv_event_t *event){
    g_pageManager.back(LV_SCR_LOAD_ANIM_MOVE_TOP, 300);
}

// LVGL 定时器回调函数（在 LVGL 任务中执行）
static void lvgl_time_update_cb(lv_timer_t *timer)
{
    if (label_time && label_second && label_running && label_BATTERY) {
        // 获取最新时间信息
        time_service::TimeInfo info = time_service::get_time_info();
        
        // 更新主时间显示 (HH:MM)
        lv_label_set_text_fmt(label_time, "#ffffff %s#", info.time_str);

        // 更新电池信息显示
        if (g_battery_info.is_valid) {
            const char* charging_icon = g_battery_info.is_charging ? LV_SYMBOL_CHARGE : "";
            lv_label_set_text_fmt(label_BATTERY, "Battery: %d%% %s", 
                                  g_battery_info.percentage, charging_icon);
        } else {
            lv_label_set_text(label_BATTERY, "Battery: N/A");
        }
        
        // 更新详细时间显示 (YYYY-MM-DD HH:MM:SS) 
        lv_label_set_text_fmt(label_second, "#ffffff %s#", info.datetime_str);
        
        // 更新运行时间
        lv_label_set_text(label_running, info.running_str);
    }
}

// 电池信息更新回调函数
static void battery_update_callback(const battery_service::BatteryInfo& info)
{
    // 更新全局电池信息（UI在LVGL定时器中更新）
    g_battery_info = info;
    ESP_LOGD(TAG, "Battery info updated: %d%%, charging: %s", 
             info.percentage, info.is_charging ? "yes" : "no");
}

// 时间更新回调函数（用于接收系统时间服务的通知，但不直接更新UI）
static void time_update_callback(const time_service::TimeInfo& info)
{
    // 这里可以处理非UI相关的时间更新逻辑
    // UI更新由LVGL定时器处理，避免在定时器上下文中操作UI
}

// 页面删除事件回调
static void time_page_delete_cb(lv_event_t *e)
{
    // 移除时间更新回调
    time_service::remove_time_update_callback();
    
    // 移除电池更新回调
    battery_service::remove_battery_update_callback();
    
    // 删除LVGL定时器
    if (ui_update_timer) {
        lv_timer_del(ui_update_timer);
        ui_update_timer = NULL;
    }
    
    ESP_LOGI(TAG, "Time page cleanup completed");
}

lv_obj_t *createPage_time()
{
    // 检查 WiFi 连接状态
    if (!wifi_manager_is_connected())
    {
        ESP_LOGW(TAG, "WiFi not connected");
    }
    else
    {
    }

    lv_obj_t *time_page = lv_obj_create(NULL);
    lv_obj_add_event_cb(time_page, time_page_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(time_page, time_page_delete_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_color(time_page, lv_color_black(), 0);

    label_time = lv_label_create(time_page);
    lv_label_set_recolor(label_time, true);
    lv_label_set_text(label_time, "#ffffff 00:00#");
    //lv_obj_set_style_text_font(label_time, &my_font, 0);
	lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, 0);
    lv_obj_center(label_time);
    lv_obj_set_style_text_color(label_time, lv_color_white(), 0);

    label_second = lv_label_create(time_page);
    lv_label_set_recolor(label_second, true);
    lv_label_set_text(label_second, "#ffffff 00-00-00 00:00:00#");
    lv_obj_set_style_text_color(label_second, lv_color_white(), 0);
    lv_obj_align_to(label_second, label_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    label_BATTERY = lv_label_create(time_page);
    lv_label_set_text(label_BATTERY, "Battery: --");
    lv_obj_align_to(label_BATTERY, label_second, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    label_running = lv_label_create(time_page);
    lv_label_set_text(label_running, "00:00:00");
    lv_obj_align_to(label_running, label_BATTERY, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    label_weather = lv_label_create(time_page);
    lv_label_set_text(label_weather, "");
    lv_obj_set_style_text_font(label_weather, &NotoSansSC_Medium_3500, 0);
    lv_obj_align(label_weather, LV_ALIGN_TOP_LEFT, 0, 0);

    // 创建LVGL定时器用于UI更新（在LVGL任务中执行，避免看门狗超时）
    ui_update_timer = lv_timer_create(lvgl_time_update_cb, 1000, NULL);
    
    // 注册系统时间回调用于其他逻辑
    time_service::set_time_update_callback(time_update_callback);
    
    // 注册电池状态回调
    battery_service::set_battery_update_callback(battery_update_callback);
    
    // 立即获取一次电池信息
    g_battery_info = battery_service::get_battery_info();
    
    // 立即更新一次UI
    lvgl_time_update_cb(NULL);
    
    ESP_LOGI(TAG, "Time page created with LVGL timer and battery service");

    //lv_obj_set_user_data(time_page, time_update_timer);// 本页面使用删除回调删除定时器，不使用页面管理器删除
    return time_page;
}