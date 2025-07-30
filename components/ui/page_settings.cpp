#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include <cstring>

// 系统服务头文件
#include "wifi_manager.h"
#include "lcd_brightness.hpp"
#include "sd_init.hpp"
#include "pmu_service.h"
#include "task_manager.hpp"
#include <algorithm>

extern PageManager g_pageManager;
lv_obj_t *lottie = NULL;
static lv_obj_t *settings_list;

// 设置页面控件
static lv_obj_t *wifi_switch = NULL;
static lv_obj_t *auto_brightness_switch = NULL;
static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *sleep_timeout_slider = NULL;
static lv_obj_t *brightness_label = NULL;
static lv_obj_t *sleep_timeout_label = NULL;

// 函数声明
void Brightness_msgbox(lv_event_t *e);
void WiFi_msgbox(lv_event_t *e);
void Status_msgbox(lv_event_t *e);
void Version_msgbox(lv_event_t *e);
void SDCard_msgbox(lv_event_t *e);
void Battery_msgbox(lv_event_t *e);

// WiFi相关回调
static void wifi_status_callback(wifi_manager_status_t status, const char* ssid);
static void wifi_switch_cb(lv_event_t *e);
static void wifi_connect_cb(lv_event_t *e);

// 亮度相关回调
// 自动亮度开关回调
static void auto_brightness_switch_cb(lv_event_t *e);
static void auto_sleep_switch_cb(lv_event_t *e);
static void slider_brightness_cb(lv_event_t *e);
static void slider_sleep_cb(lv_event_t *e);
static void lottie_buf_free_cb(lv_event_t *e)
{

    g_pageManager.back(LV_SCR_LOAD_ANIM_FADE_OUT, 300);
    if (lottie)
    {
        lv_obj_del(lottie);
    }
    uint8_t *buf = (uint8_t *)lv_event_get_user_data(e);
    if (buf)
    {
        heap_caps_free(buf);
    }
}
static void event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);
    if (code == LV_EVENT_CLICKED)
    {
        LV_UNUSED(obj);
        const char* button_text = lv_list_get_button_text(settings_list, obj);
        
        // 没有实现，其他功能暂时跳过，保留UI
    }
}
static void screen_backbtn_cb(lv_event_t *e)
{
    g_pageManager.gotoPage("page_menu", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
}
// WiFi状态回调函数
static void wifi_status_callback(wifi_manager_status_t status, const char* ssid)
{
    // 更新WiFi状态显示
    // 这里可以添加状态更新逻辑
}

// WiFi开关回调
static void wifi_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    if (enabled) {
        wifi_manager_enable();
    } else {
        wifi_manager_disable();
    }
}

// WiFi连接回调
static void wifi_connect_cb(lv_event_t *e)
{
    // WiFi连接逻辑 - 暂时跳过，保留占位
    // 可以在这里实现WiFi扫描和连接界面
}

// 自动息屏开关回调
static void auto_sleep_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    brightness_set_auto_sleep_enabled(enabled);
}

static void auto_brightness_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target_obj(e);
    bool auto_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    brightness_mode_t mode = auto_mode ? BRIGHTNESS_MODE_AUTO : BRIGHTNESS_MODE_MANUAL;
    set_brightness_mode(mode);
    
    // 更新亮度滑块的可用状态
    if (brightness_slider) {
        if (auto_mode) {
            lv_obj_add_state(brightness_slider, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(brightness_slider, LV_STATE_DISABLED);
        }
    }
}

void WiFi_msgbox(lv_event_t *e)
{
    lv_obj_t *wifi_msgbox = lv_msgbox_create(lv_screen_active());
    lv_obj_set_style_clip_corner(wifi_msgbox, true, 0);
    lv_obj_set_size(wifi_msgbox, LV_HOR_RES, LV_VER_RES);
    
    lv_msgbox_add_title(wifi_msgbox, "WiFi Settings");
    lv_msgbox_add_close_button(wifi_msgbox);
    
    lv_obj_t *content = lv_msgbox_get_content(wifi_msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // WiFi开关容器
    lv_obj_t *wifi_cont = lv_obj_create(content);
    lv_obj_set_size(wifi_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wifi_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wifi_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *wifi_label = lv_label_create(wifi_cont);
    lv_label_set_text(wifi_label, "Enable WiFi");
    
    wifi_switch = lv_switch_create(wifi_cont);
    lv_obj_add_event_cb(wifi_switch, wifi_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 设置开关状态
    if (wifi_manager_is_enabled()) {
        lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
    }
    
    // WiFi状态显示
    lv_obj_t *status_cont = lv_obj_create(content);
    lv_obj_set_size(status_cont, lv_pct(100), LV_SIZE_CONTENT);
    
    lv_obj_t *status_label = lv_label_create(status_cont);
    wifi_manager_status_t status = wifi_manager_get_status();
    char status_text[128];
    
    switch (status) {
        case WIFI_MANAGER_DISCONNECTED:
            snprintf(status_text, sizeof(status_text), "Status: Disconnected");
            break;
        case WIFI_MANAGER_CONNECTING:
            snprintf(status_text, sizeof(status_text), "Status: Connecting...");
            break;
        case WIFI_MANAGER_CONNECTED:
            {
                char ssid[64] = {0};
                if (wifi_manager_get_connected_ssid(ssid, sizeof(ssid)) == ESP_OK) {
                    snprintf(status_text, sizeof(status_text), "Status: Connected to %s", ssid);
                } else {
                    snprintf(status_text, sizeof(status_text), "Status: Connected");
                }
            }
            break;
        default:
            snprintf(status_text, sizeof(status_text), "Status: Unknown");
            break;
    }
    
    lv_label_set_text(status_label, status_text);
    
    // 连接按钮 (占位)
    lv_obj_t *connect_btn = lv_btn_create(content);
    lv_obj_set_width(connect_btn, lv_pct(100));
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect to Network");
    lv_obj_center(connect_label);
    lv_obj_add_event_cb(connect_btn, wifi_connect_cb, LV_EVENT_CLICKED, NULL);
}
static void slider_brightness_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    lv_obj_t *lb_brightness = (lv_obj_t *)lv_event_get_user_data(e);
    int value = (int)lv_slider_get_value(slider);
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "Manual Brightness: %d%%", value);
    lv_label_set_text(lb_brightness, buf);
    
    // 设置手动亮度
    set_brightness_manual((uint8_t)(value * 255 / 100));
}

static void slider_sleep_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target_obj(e);
    lv_obj_t *lb_sleep = (lv_obj_t *)lv_event_get_user_data(e);
    int value = (int)lv_slider_get_value(slider);
    char buf[32];
    lv_snprintf(buf, sizeof(buf), "Sleep Timeout: %d sec", value);
    lv_label_set_text(lb_sleep, buf);
    
    // 设置息屏超时时间
    brightness_set_sleep_timeout(value * 1000); // 转换为毫秒
}

void Brightness_msgbox(lv_event_t *e)
{
    lv_obj_t *brightness_msgbox = lv_msgbox_create(lv_screen_active());
    lv_obj_set_style_clip_corner(brightness_msgbox, true, 0);
    lv_obj_set_size(brightness_msgbox, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_text_font(brightness_msgbox,&NotoSansSC_Medium_3500, 0);
    
    lv_msgbox_add_title(brightness_msgbox, "屏幕设置");
    lv_msgbox_add_close_button(brightness_msgbox);
    
    lv_obj_t *content = lv_msgbox_get_content(brightness_msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(content, -1, LV_PART_SCROLLBAR);
    
    // 自动亮度开关容器
    lv_obj_t *auto_brightness_cont = lv_obj_create(content);
    lv_obj_set_size(auto_brightness_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(auto_brightness_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(auto_brightness_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *auto_brightness_label = lv_label_create(auto_brightness_cont);
    lv_label_set_text(auto_brightness_label, "自动亮度");
    
    auto_brightness_switch = lv_switch_create(auto_brightness_cont);
    lv_obj_add_event_cb(auto_brightness_switch, auto_brightness_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 设置开关状态
    brightness_mode_t mode = get_brightness_mode();
    if (mode == BRIGHTNESS_MODE_AUTO) {
        lv_obj_add_state(auto_brightness_switch, LV_STATE_CHECKED);
    }
    
    // 手动亮度控制容器
    lv_obj_t *manual_brightness_cont = lv_obj_create(content);
    lv_obj_set_size(manual_brightness_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(manual_brightness_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(manual_brightness_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    
    brightness_label = lv_label_create(manual_brightness_cont);
    uint8_t current_brightness = get_current_brightness();
    char brightness_text[32];
    snprintf(brightness_text, sizeof(brightness_text), "亮度调节: %d%%", current_brightness * 100 / 255);
    lv_label_set_text(brightness_label, brightness_text);
    
    brightness_slider = lv_slider_create(manual_brightness_cont);
    lv_obj_set_width(brightness_slider, lv_pct(100));
    lv_slider_set_range(brightness_slider, 1, 100);
    lv_slider_set_value(brightness_slider, current_brightness * 100 / 255, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, slider_brightness_cb, LV_EVENT_VALUE_CHANGED, brightness_label);
    
    // 如果是自动模式，禁用手动亮度滑块
    if (mode == BRIGHTNESS_MODE_AUTO) {
        lv_obj_add_state(brightness_slider, LV_STATE_DISABLED);
    }
    
    // 息屏时间控制容器
    lv_obj_t *sleep_timeout_cont = lv_obj_create(content);
    lv_obj_set_size(sleep_timeout_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sleep_timeout_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sleep_timeout_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    
    sleep_timeout_label = lv_label_create(sleep_timeout_cont);
    uint32_t current_timeout = brightness_get_sleep_timeout() / 1000; // 转换为秒
    char timeout_text[32];
    snprintf(timeout_text, sizeof(timeout_text), "息屏时间: %lu 秒", current_timeout);
    lv_label_set_text(sleep_timeout_label, timeout_text);
    
    sleep_timeout_slider = lv_slider_create(sleep_timeout_cont);
    lv_obj_set_width(sleep_timeout_slider, lv_pct(100));
    lv_slider_set_range(sleep_timeout_slider, 10, 300); // 10秒到300秒(5分钟)
    lv_slider_set_value(sleep_timeout_slider, current_timeout, LV_ANIM_OFF);
    lv_obj_add_event_cb(sleep_timeout_slider, slider_sleep_cb, LV_EVENT_VALUE_CHANGED, sleep_timeout_label);
    
    // 自动息屏开关容器
    lv_obj_t *auto_sleep_cont = lv_obj_create(content);
    lv_obj_set_size(auto_sleep_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(auto_sleep_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(auto_sleep_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t *auto_sleep_label = lv_label_create(auto_sleep_cont);
    lv_label_set_text(auto_sleep_label, "自动息屏");
    
    lv_obj_t *auto_sleep_switch = lv_switch_create(auto_sleep_cont);
    lv_obj_add_event_cb(auto_sleep_switch, auto_sleep_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (brightness_get_auto_sleep_enabled()) {
        lv_obj_add_state(auto_sleep_switch, LV_STATE_CHECKED);
    }
    
    // 应用和取消按钮
    // lv_obj_t *apply_button = lv_msgbox_add_footer_button(brightness_msgbox, "应用");
    // lv_obj_set_flex_grow(apply_button, 1);
    
    // lv_obj_t *cancel_button = lv_msgbox_add_footer_button(brightness_msgbox, "取消");
    // lv_obj_set_flex_grow(cancel_button, 1);
}
static void draw_event_cb(lv_event_t * e)
{
    lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t * base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    /*If the cells are drawn...*/
    if(base_dsc->part == LV_PART_ITEMS) {
        uint32_t row = base_dsc->id1;
        uint32_t col = base_dsc->id2;

        /*Make the texts in the first cell center aligned*/
        if(row == 0) {
            lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
            if(label_draw_dsc) {
                label_draw_dsc->align = LV_TEXT_ALIGN_CENTER;
            }
            lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
            if(fill_draw_dsc) {
                fill_draw_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_BLUE), fill_draw_dsc->color, LV_OPA_20);
                fill_draw_dsc->opa = LV_OPA_COVER;
            }
        }
        /*In the first column align the texts to the right*/
        else if(col == 0) {
            lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
            if(label_draw_dsc) {
                label_draw_dsc->align = LV_TEXT_ALIGN_RIGHT;
            }
        }

        /*Make every 2nd row grayish*/
        if((row != 0 && row % 2) == 0) {
            lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
            if(fill_draw_dsc) {
                fill_draw_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_GREY), fill_draw_dsc->color, LV_OPA_10);
                fill_draw_dsc->opa = LV_OPA_COVER;
            }
        }
    }
}

void Status_msgbox(lv_event_t *e)
{
    lv_obj_t *status_msgbox = lv_msgbox_create(lv_screen_active());
    lv_obj_set_style_clip_corner(status_msgbox, true, 0);
    lv_obj_set_size(status_msgbox, LV_HOR_RES, LV_VER_RES);
    
    lv_msgbox_add_title(status_msgbox, "System Status");
    lv_msgbox_add_close_button(status_msgbox);
    
    lv_obj_t *content = lv_msgbox_get_content(status_msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(content, -1, LV_PART_SCROLLBAR);
    
    // 系统内存信息
    lv_obj_t *memory_cont = lv_obj_create(content);
    lv_obj_set_size(memory_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(memory_cont, LV_FLEX_FLOW_COLUMN);
    
    lv_obj_t *memory_title = lv_label_create(memory_cont);
    lv_label_set_text(memory_title, "Memory Information");
    lv_obj_set_style_text_font(memory_title, &lv_font_montserrat_14, 0);
    
    TaskManager& task_manager = TaskManager::instance();
    auto system_info = task_manager.get_system_info();
    
    char memory_text[256];
    snprintf(memory_text, sizeof(memory_text), 
        "Free Heap: %u KB\n"
        "Min Free Heap: %u KB\n"
        "Total Allocated: %u KB",
        (unsigned int)(system_info.free_heap / 1024),
        (unsigned int)(system_info.min_free_heap / 1024),
        (unsigned int)(system_info.total_allocated / 1024));
    
    lv_obj_t *memory_label = lv_label_create(memory_cont);
    lv_label_set_text(memory_label, memory_text);
    
    // 任务列表表格
    lv_obj_t *table = lv_table_create(content);
    lv_obj_set_size(table, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_10, 0);

    // 设置表格列数
    lv_table_set_column_count(table, 4);
    
    // 设置表头
    lv_table_set_cell_value(table, 0, 0, "Task Name");
    lv_table_set_cell_value(table, 0, 1, "Priority");
    lv_table_set_cell_value(table, 0, 2, "Stack Free");
    lv_table_set_cell_value(table, 0, 3, "State");
    
    // 获取任务信息并按剩余栈大小排序
    auto tasks_info = task_manager.get_all_tasks_info();
    
    // 按剩余栈大小排序（从小到大）
    std::sort(tasks_info.begin(), tasks_info.end(), 
        [](const TaskManager::TaskInfo &a, const TaskManager::TaskInfo &b) {
            return a.stack_high_water_mark < b.stack_high_water_mark;
        });
    
    // 填充任务信息（最多显示15个任务）
    int max_tasks = std::min(static_cast<int>(tasks_info.size()), 15);
    for (int i = 0; i < max_tasks; i++) {
        const auto& task = tasks_info[i];
        
        // 任务名称
        lv_table_set_cell_value(table, i + 1, 0, task.name.c_str());
        
        // 优先级
        char priority_str[8];
        snprintf(priority_str, sizeof(priority_str), "%lu", (unsigned long)task.priority);
        lv_table_set_cell_value(table, i + 1, 1, priority_str);
        
        // 剩余栈大小
        char stack_str[16];
        snprintf(stack_str, sizeof(stack_str), "%lu B", task.stack_high_water_mark);
        lv_table_set_cell_value(table, i + 1, 2, stack_str);
        
        // 状态
        const char* state_str;
        switch (task.state) {
            case eRunning: state_str = "R"; break;
            case eReady: state_str = "R"; break;
            case eBlocked: state_str = "B"; break;
            case eSuspended: state_str = "S"; break;
            case eDeleted: state_str = "D"; break;
            default: state_str = "U"; break;
        }
        lv_table_set_cell_value(table, i + 1, 3, state_str);
        
        // 如果剩余栈空间很小，可以通过单元格控制位来标记
        if (task.stack_high_water_mark < 512) {
            // 使用自定义控制位标记危险的栈使用情况
            lv_table_set_cell_ctrl(table, i + 1, 2, LV_TABLE_CELL_CTRL_CUSTOM_1);
        }
    }
    
    // 设置表格样式
    lv_obj_add_event_cb(table, draw_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_flag(table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    
    // 设置表格列宽
    lv_table_set_column_width(table, 0, 80);  // 任务名
    lv_table_set_column_width(table, 1, 40);  // 优先级
    lv_table_set_column_width(table, 2, 60);  // 栈空间
    lv_table_set_column_width(table, 3, 40);  // 状态
}
void SDCard_msgbox(lv_event_t *e)
{
    lv_obj_t *sdcard_msgbox = lv_msgbox_create(lv_screen_active());
    lv_obj_set_style_clip_corner(sdcard_msgbox, true, 0);
    lv_obj_set_size(sdcard_msgbox, LV_HOR_RES, LV_VER_RES);
    
    lv_msgbox_add_title(sdcard_msgbox, "SD Card Information");
    lv_msgbox_add_close_button(sdcard_msgbox);
    
    lv_obj_t *content = lv_msgbox_get_content(sdcard_msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(content, -1, LV_PART_SCROLLBAR);
    
    // SD卡状态容器
    lv_obj_t *status_cont = lv_obj_create(content);
    lv_obj_set_size(status_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(status_cont, LV_FLEX_FLOW_COLUMN);
    
    char sd_info_text[512];
    
    if (is_sd_card_mounted()) {
        sd_card_info_t sd_info;
        esp_err_t ret = get_sd_card_info(&sd_info);
        
        if (ret == ESP_OK) {
            snprintf(sd_info_text, sizeof(sd_info_text),
                "Status: Mounted\n"
                "Card Name: %s\n"
                "Capacity: %lu MB\n"
                "Sector Size: %lu bytes\n"
                "Mount Point: %s",
                sd_info.name,
                sd_info.capacity_mb,
                sd_info.sector_size,
                sdcard_mount_point);
        } else {
            snprintf(sd_info_text, sizeof(sd_info_text),
                "Status: Mounted\n"
                "Error: Failed to get card information");
        }
    } else {
        snprintf(sd_info_text, sizeof(sd_info_text),
            "Status: Not mounted or not present\n"
            "Please insert an SD card and restart");
    }
    
    lv_obj_t *info_label = lv_label_create(status_cont);
    lv_label_set_text(info_label, sd_info_text);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_label, lv_pct(100));
    
    // 如果SD卡已挂载，显示更多操作选项
    if (is_sd_card_mounted()) {
        // 刷新按钮
        lv_obj_t *refresh_btn = lv_btn_create(content);
        lv_obj_set_width(refresh_btn, lv_pct(100));
        lv_obj_t *refresh_label = lv_label_create(refresh_btn);
        lv_label_set_text(refresh_label, "Refresh");
        lv_obj_center(refresh_label);
        
        // 卸载按钮 (占位功能)
        lv_obj_t *unmount_btn = lv_btn_create(content);
        lv_obj_set_width(unmount_btn, lv_pct(100));
        lv_obj_t *unmount_label = lv_label_create(unmount_btn);
        lv_label_set_text(unmount_label, "Unmount (Not Implemented)");
        lv_obj_center(unmount_label);
        lv_obj_add_state(unmount_btn, LV_STATE_DISABLED);
    }
}

void Battery_msgbox(lv_event_t *e)
{
    g_pageManager.gotoPage("page_pmu", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
}

void Version_msgbox(lv_event_t *e)
{
    lv_obj_t *version_msgbox = lv_msgbox_create(lv_screen_active());
    lv_obj_set_style_clip_corner(version_msgbox, true, 0);
    lv_obj_set_size(version_msgbox, LV_HOR_RES, LV_VER_RES);
    lv_msgbox_add_title(version_msgbox, "Version Information");
    lv_msgbox_add_close_button(version_msgbox);
    
    lv_obj_t *content = lv_msgbox_get_content(version_msgbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(content, -1, LV_PART_SCROLLBAR);

    lv_obj_t *label = lv_label_create(content);
    lv_label_set_text(label, "Version: 1.0.0");
    
    lv_obj_t *label2 = lv_label_create(content);
    lv_label_set_text_fmt(label2, "Build Date: %s", __DATE__);
    
    lv_obj_t *label3 = lv_label_create(content);
    lv_label_set_text_fmt(label3, "Build Time: %s", __TIME__);
    
    lv_obj_t *label4 = lv_label_create(content);
    lv_label_set_text(label4, "ESP32-S3 Multithreading Demo\nhttps://github.com/FYMTC/ESP32_PPTV");
    lv_label_set_long_mode(label4, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label4, lv_pct(100));
}
lv_obj_t *createPage_settings(void)
{

    lv_obj_t *setting_page = lv_obj_create(NULL);
    lv_obj_t *status = lv_obj_create(setting_page);
    lv_obj_set_size(status, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_ver(status, 0, 0);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_COLUMN); // 按钮内容弹性行增长
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_width(status, 0, 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *screen_backbtn = lv_btn_create(status);
    lv_obj_set_size(screen_backbtn, LV_PCT(100), 20);
    lv_obj_t *img = lv_img_create(screen_backbtn);
    lv_img_set_src(img, LV_SYMBOL_NEW_LINE);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *label = lv_label_create(screen_backbtn);
    lv_label_set_text(label, "BACK");
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(screen_backbtn, screen_backbtn_cb, LV_EVENT_CLICKED, NULL);

    settings_list = lv_list_create(status);
    lv_obj_set_size(settings_list, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_center(settings_list);
    lv_obj_t *btn;
    lv_list_add_text(settings_list, "Connectivity");
    btn = lv_list_add_button(settings_list, LV_SYMBOL_WIFI, "WLAN");
    lv_obj_add_event_cb(btn, WiFi_msgbox, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, LV_SYMBOL_BLUETOOTH, "Bluetooth");
    lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, LV_SYMBOL_GPS, "Navigation");
    lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, LV_SYMBOL_USB, "USB");
    lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, LV_SYMBOL_BATTERY_FULL, "Battery");
    lv_obj_add_event_cb(btn, Battery_msgbox, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, LV_SYMBOL_SD_CARD, "SD Card");
    lv_obj_add_event_cb(btn, SDCard_msgbox, LV_EVENT_CLICKED, NULL);

    lv_list_add_text(settings_list, "System");
    btn = lv_list_add_button(settings_list, MY_SYMBOL_PHONE, "Brightness");
    lv_obj_add_event_cb(btn, Brightness_msgbox, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, LV_SYMBOL_VOLUME_MID, "Volume");
    lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, MY_SYMBOL_TIME, "Time");
    lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, MY_SYMBOL_BELL, "Alarm");
    lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);

    lv_list_add_text(settings_list, "About");
    btn = lv_list_add_button(settings_list, LV_SYMBOL_HOME, "Version");
    lv_obj_add_event_cb(btn, Version_msgbox, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, LV_SYMBOL_LIST, "Status");
    lv_obj_add_event_cb(btn, Status_msgbox, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_button(settings_list, LV_SYMBOL_DOWNLOAD, "OTA Update");
    lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, NULL);

    return setting_page;
}
