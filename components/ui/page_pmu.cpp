#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include "pmu_service.h"
#include "esp_log.h"
#include <stdio.h>
#include <cstring>

extern PageManager g_pageManager;

static const char *TAG = "PAGE_PMU";

// UI对象
static lv_obj_t *g_page_screen = NULL;
static lv_obj_t *g_status_label = NULL;
static lv_obj_t *g_battery_bar = NULL;
static lv_obj_t *g_battery_label = NULL;
static lv_obj_t *g_voltage_label = NULL;
static lv_obj_t *g_current_label = NULL;
static lv_obj_t *g_temp_label = NULL;
static lv_obj_t *g_charge_status_label = NULL;
static lv_obj_t *g_vbus_status_label = NULL;

// 电源通道开关
static lv_obj_t *g_dc1_switch = NULL;
static lv_obj_t *g_dc3_switch = NULL;
static lv_obj_t *g_aldo1_switch = NULL;
static lv_obj_t *g_aldo2_switch = NULL;
static lv_obj_t *g_aldo3_switch = NULL;
static lv_obj_t *g_aldo4_switch = NULL;
static lv_obj_t *g_bldo1_switch = NULL;
static lv_obj_t *g_bldo2_switch = NULL;

// 更新定时器
static lv_timer_t *g_update_timer = NULL;

// 数据缓存，避免在定时器回调中直接更新UI
static pmu_data_t g_cached_pmu_data = {};
static bool g_data_updated = false;

/**
 * @brief PMU数据回调函数（在定时器线程中调用，只缓存数据）
 */
static void pmu_data_callback(const pmu_data_t *data)
{
    if (!data) return;

    // 只缓存数据，不直接更新UI，避免在定时器上下文中进行复杂操作
    g_cached_pmu_data = *data;  // 直接赋值而不是memcpy
    g_data_updated = true;
}

/**
 * @brief 更新UI显示（在LVGL定时器中调用）
 */
static void update_ui_display(void)
{
    if (!g_data_updated) return;

    const pmu_data_t *data = &g_cached_pmu_data;

    // 使用静态缓冲区避免栈溢出
    static char display_buf[64];

    // 更新电池状态
    if (g_battery_bar) {
        lv_bar_set_value(g_battery_bar, data->battery_percentage, LV_ANIM_ON);
    }

    if (g_battery_label) {
        snprintf(display_buf, sizeof(display_buf), "%d%%", data->battery_percentage);
        lv_label_set_text(g_battery_label, display_buf);
    }

    // 更新电压电流信息
    if (g_voltage_label) {
        snprintf(display_buf, sizeof(display_buf), "电池: %dmV | 系统: %dmV",
                data->battery_voltage, data->system_voltage);
        lv_label_set_text(g_voltage_label, display_buf);
    }

    if (g_current_label) {
        if (data->battery_current > 0) {
            snprintf(display_buf, sizeof(display_buf), "充电: %dmA", data->battery_current);
        } else if (data->battery_current < 0) {
            snprintf(display_buf, sizeof(display_buf), "放电: %dmA", -data->battery_current);
        } else {
            snprintf(display_buf, sizeof(display_buf), "待机: 0mA");
        }
        lv_label_set_text(g_current_label, display_buf);
    }

    // 更新温度
    if (g_temp_label) {
        snprintf(display_buf, sizeof(display_buf), "温度: %d.%d°C",
                data->temperature / 10, abs(data->temperature % 10));
        lv_label_set_text(g_temp_label, display_buf);
    }

    // 更新充电状态
    if (g_charge_status_label) {
        const char *status_text;
        switch (data->charge_status) {
            case CHARGE_STATUS_NOT_CHARGING:
                status_text = "Not Charging";
                break;
            case CHARGE_STATUS_PRECHARGE:
                status_text = "Pre-charge";
                break;
            case CHARGE_STATUS_CONSTANT_CURRENT:
                status_text = "CC Charging";
                break;
            case CHARGE_STATUS_CONSTANT_VOLTAGE:
                status_text = "CV Charging";
                break;
            case CHARGE_STATUS_CHARGE_DONE:
                status_text = "Charge Done";
                break;
            default:
                status_text = "Unknown";
                break;
        }
        lv_label_set_text(g_charge_status_label, status_text);
    }

    // 更新VBUS状态
    if (g_vbus_status_label) {
        if (data->vbus_present) {
            snprintf(display_buf, sizeof(display_buf), LV_SYMBOL_USB " USB: %dmV", data->vbus_voltage);
            lv_label_set_text(g_vbus_status_label, display_buf);
            lv_obj_set_style_text_color(g_vbus_status_label, lv_color_hex(0x00AA00), 0);
        } else {
            lv_label_set_text(g_vbus_status_label, MY_SYMBOL_DISCONNECTED " USB 断开");
            lv_obj_set_style_text_color(g_vbus_status_label, lv_color_hex(0xAA0000), 0);
        }
    }

    // 更新电源通道开关状态（简化状态更新逻辑）
    if (g_dc1_switch) {
        if (data->dc1_enabled) {
            lv_obj_add_state(g_dc1_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(g_dc1_switch, LV_STATE_CHECKED);
        }
        lv_obj_add_state(g_dc1_switch, LV_STATE_DISABLED);
    }
    if (g_dc3_switch) {
        if (data->dc3_enabled) {
            lv_obj_add_state(g_dc3_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(g_dc3_switch, LV_STATE_CHECKED);
        }
        lv_obj_add_state(g_dc3_switch, LV_STATE_DISABLED);
    }

    // 简化其他开关的状态更新
    if (g_aldo1_switch) {
        if (data->aldo1_enabled) lv_obj_add_state(g_aldo1_switch, LV_STATE_CHECKED);
        else lv_obj_clear_state(g_aldo1_switch, LV_STATE_CHECKED);
    }
    if (g_aldo2_switch) {
        if (data->aldo2_enabled) lv_obj_add_state(g_aldo2_switch, LV_STATE_CHECKED);
        else lv_obj_clear_state(g_aldo2_switch, LV_STATE_CHECKED);
    }
    if (g_aldo3_switch) {
        if (data->aldo3_enabled) lv_obj_add_state(g_aldo3_switch, LV_STATE_CHECKED);
        else lv_obj_clear_state(g_aldo3_switch, LV_STATE_CHECKED);
    }
    if (g_aldo4_switch) {
        if (data->aldo4_enabled) lv_obj_add_state(g_aldo4_switch, LV_STATE_CHECKED);
        else lv_obj_clear_state(g_aldo4_switch, LV_STATE_CHECKED);
    }
    if (g_bldo1_switch) {
        if (data->bldo1_enabled) lv_obj_add_state(g_bldo1_switch, LV_STATE_CHECKED);
        else lv_obj_clear_state(g_bldo1_switch, LV_STATE_CHECKED);
    }
    if (g_bldo2_switch) {
        if (data->bldo2_enabled) lv_obj_add_state(g_bldo2_switch, LV_STATE_CHECKED);
        else lv_obj_clear_state(g_bldo2_switch, LV_STATE_CHECKED);
    }

    // 更新连接状态
    if (g_status_label) {
        const char *status_text;
        lv_color_t status_color;

        switch (data->pmu_status) {
            case PMU_STATUS_CONNECTED:
                status_text = MY_SYMBOL_CONNECTED "PMU 已连接 " LV_SYMBOL_REFRESH "正常运行";
                status_color = lv_color_hex(0x00AA00);
                break;
            case PMU_STATUS_ERROR:
                status_text = LV_SYMBOL_CLOSE "PMU 错误";
                status_color = lv_color_hex(0xAA0000);
                break;
            default:
                status_text = MY_SYMBOL_DISCONNECTED "PMU 断开";
                status_color = lv_color_hex(0x888888);
                break;
        }

        lv_label_set_text(g_status_label, status_text);
        lv_obj_set_style_text_color(g_status_label, status_color, 0);
    }

    // 标记数据已处理
    g_data_updated = false;
}

/**
 * @brief PMU事件回调函数
 */
static void pmu_event_callback(const char *event, uint32_t value)
{
    ESP_LOGI(TAG, "PMU Event: %s = %lu", event, value);

    // 可以在这里处理特殊事件，比如电源键按下、USB插拔等
    if (strcmp(event, "power_key_short") == 0) {
        // 短按电源键的处理
    } else if (strcmp(event, "vbus_insert") == 0) {
        // USB插入的处理
    } else if (strcmp(event, "vbus_remove") == 0) {
        // USB拔出的处理
    }
}

/**
 * @brief 电源通道开关事件处理
 */
static void power_switch_event_cb(lv_event_t *e)
{
    lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
    bool is_checked = lv_obj_has_state(target, LV_STATE_CHECKED);

    const char *channel = NULL;
    // DC1和DC3是DCDC电源，不处理其开关事件
    if (target == g_dc1_switch || target == g_dc3_switch) {
        ESP_LOGW(TAG, "DCDC power channels cannot be manually controlled");
        return;
    }
    else if (target == g_aldo1_switch) channel = "aldo1";
    else if (target == g_aldo2_switch) channel = "aldo2";
    else if (target == g_aldo3_switch) channel = "aldo3";
    else if (target == g_aldo4_switch) channel = "aldo4";
    else if (target == g_bldo1_switch) channel = "bldo1";
    else if (target == g_bldo2_switch) channel = "bldo2";

    if (channel) {
        esp_err_t ret = pmu_service_set_power_channel(channel, is_checked);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set power channel %s", channel);
        }
    }
}

/**
 * @brief 创建状态区域
 */
static void create_status_section(lv_obj_t *parent)
{
    // 状态容器
    lv_obj_t *status_cont = lv_obj_create(parent);
    lv_obj_set_width(status_cont, LV_PCT(100));
    lv_obj_set_height(status_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(status_cont, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_border_width(status_cont, 1, 0);
    lv_obj_set_style_border_color(status_cont, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(status_cont, 8, 0);
    lv_obj_set_style_pad_all(status_cont, 8, 0);

    // 标题
    lv_obj_t *title = lv_label_create(status_cont);
    lv_label_set_text(title, "PMU 电源管理");
    lv_obj_set_style_text_font(title, &NotoSansSC_Medium_3500, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);

    // 连接状态
    g_status_label = lv_label_create(status_cont);
    lv_label_set_text(g_status_label, "PMU Disconnected");
    lv_obj_set_style_text_font(g_status_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0x888888), 0);
    lv_obj_align_to(g_status_label, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    // VBUS状态
    g_vbus_status_label = lv_label_create(status_cont);
    lv_label_set_text(g_vbus_status_label, "USB Disconnected");
    lv_obj_set_style_text_font(g_vbus_status_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_align_to(g_vbus_status_label, g_status_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
}

/**
 * @brief 创建电池信息区域
 */
static void create_battery_section(lv_obj_t *parent)
{
    // 电池容器
    lv_obj_t *battery_cont = lv_obj_create(parent);
    lv_obj_set_width(battery_cont, LV_PCT(100));
    lv_obj_set_height(battery_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(battery_cont, lv_color_hex(0xF8F8FF), 0);
    lv_obj_set_style_border_width(battery_cont, 1, 0);
    lv_obj_set_style_border_color(battery_cont, lv_color_hex(0xDDD), 0);
    lv_obj_set_style_radius(battery_cont, 8, 0);
    lv_obj_set_style_pad_all(battery_cont, 12, 0);

    // 标题
    lv_obj_t *title = lv_label_create(battery_cont);
    lv_label_set_text(title, LV_SYMBOL_BATTERY_2 " 电池信息");
    lv_obj_set_style_text_font(title, &NotoSansSC_Medium_3500, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);

    // 电池电量条
    g_battery_bar = lv_bar_create(battery_cont);
    lv_obj_set_size(g_battery_bar, LV_PCT(70), 25);
    lv_obj_align_to(g_battery_bar, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_bar_set_range(g_battery_bar, 0, 100);
    lv_bar_set_value(g_battery_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_battery_bar, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_battery_bar, lv_color_hex(0x4CAF50), LV_PART_INDICATOR);

    // 电池百分比
    g_battery_label = lv_label_create(battery_cont);
    lv_label_set_text(g_battery_label, "0%");
    lv_obj_set_style_text_font(g_battery_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_align_to(g_battery_label, g_battery_bar, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // 电压信息
    g_voltage_label = lv_label_create(battery_cont);
    lv_label_set_text(g_voltage_label, "电池: 0mV | 系统: 0mV");
    lv_obj_set_style_text_font(g_voltage_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_align_to(g_voltage_label, g_battery_bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    // 电流信息
    g_current_label = lv_label_create(battery_cont);
    lv_label_set_text(g_current_label, "待机: 0mA");
    lv_obj_set_style_text_font(g_current_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_align_to(g_current_label, g_voltage_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    // 温度信息
    g_temp_label = lv_label_create(battery_cont);
    lv_label_set_text(g_temp_label, "温度: 0.0°C");
    lv_obj_set_style_text_font(g_temp_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_align_to(g_temp_label, g_current_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    // 充电状态
    g_charge_status_label = lv_label_create(battery_cont);
    lv_label_set_text(g_charge_status_label, LV_SYMBOL_BATTERY_EMPTY " Not Charging");
    lv_obj_align_to(g_charge_status_label, g_temp_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
    lv_obj_set_style_text_font(g_charge_status_label, &lv_font_montserrat_12, 0);
}

/**
 * @brief 创建电源通道控制区域
 */
static void create_power_channels_section(lv_obj_t *parent)
{
    // 电源通道容器
    lv_obj_t *power_cont = lv_obj_create(parent);
    lv_obj_set_width(power_cont, LV_PCT(100));
    lv_obj_set_height(power_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(power_cont, lv_color_hex(0xFFF8DC), 0);
    lv_obj_set_style_border_width(power_cont, 1, 0);
    lv_obj_set_style_border_color(power_cont, lv_color_hex(0xDDD), 0);
    lv_obj_set_style_radius(power_cont, 8, 0);
    lv_obj_set_style_pad_all(power_cont, 12, 0);

    // 标题
    lv_obj_t *title = lv_label_create(power_cont);
    lv_label_set_text(title, MY_SYMBOL_SLIDERS " 电源通道控制");
    lv_obj_set_style_text_font(title, &NotoSansSC_Medium_3500, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);

    // 创建网格布局 - 简化为简单的垂直布局
    lv_obj_t *channels_list = lv_obj_create(power_cont);
    lv_obj_set_size(channels_list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align_to(channels_list, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_obj_set_style_border_width(channels_list, 0, 0);
    lv_obj_set_style_bg_opa(channels_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(channels_list, 2, 0); // 减小容器内边距
    lv_obj_set_style_pad_gap(channels_list, 2, 0); // 减小项目间间距
    lv_obj_set_flex_flow(channels_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(channels_list, LV_SCROLLBAR_MODE_OFF); // 禁用滚动条

    // 创建电源通道开关项目的辅助函数
    auto create_power_item = [&](const char *name, lv_obj_t **switch_obj, bool is_dcdc = false) {
        lv_obj_t *item = lv_obj_create(channels_list);
        lv_obj_set_size(item, LV_PCT(100), LV_SIZE_CONTENT); // 使用内容自适应高度
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_pad_all(item, 4, 0); // 进一步减小内边距到4
        lv_obj_set_style_pad_gap(item, 5, 0); // 设置元素间间距
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *label = lv_label_create(item);
        lv_label_set_text(label, name);
        lv_obj_set_style_text_font(label, &NotoSansSC_Medium_3500, 0);

        // 如果是DCDC电源，显示为灰色表示不可控制
        if (is_dcdc) {
            lv_obj_set_style_text_color(label, lv_color_hex(0x888888), 0);
        }

        *switch_obj = lv_switch_create(item);
        lv_obj_set_size(*switch_obj, 35, 18); // 进一步减小开关尺寸

        if (is_dcdc) {
            // DCDC电源不允许手动开关，设置为禁用状态
            lv_obj_add_state(*switch_obj, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(*switch_obj, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        } else {
            lv_obj_add_event_cb(*switch_obj, power_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }
    };

    // 创建各个电源通道开关
    create_power_item("DC1 External 3.3V", &g_dc1_switch, true);  // DCDC电源，不可手动开关（系统电源）
    create_power_item("DC3 ESP32 Core", &g_dc3_switch, true);  // DCDC电源，不可手动开关（未使用）
    create_power_item("ALDO1 Camera Digital", &g_aldo1_switch);
    create_power_item("ALDO2 Camera Analog", &g_aldo2_switch);
    create_power_item("ALDO3 PIR Power", &g_aldo3_switch);
    create_power_item("ALDO4 Camera AVDD", &g_aldo4_switch);
    create_power_item("BLDO1 OLED Power", &g_bldo1_switch);
    create_power_item("BLDO2 MIC Power", &g_bldo2_switch);
}

/**
 * @brief 创建控制按钮区域
 */
static void create_control_buttons(lv_obj_t *parent)
{
    // 按钮容器
    lv_obj_t *btn_cont = lv_obj_create(parent);
    lv_obj_set_width(btn_cont, LV_PCT(100));
    lv_obj_set_height(btn_cont, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_pad_all(btn_cont, 5, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(back_btn, 100, 35);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " 返回");
    lv_obj_set_style_text_font(back_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, [](lv_event_t *e) {
        g_pageManager.back(LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300);
    }, LV_EVENT_CLICKED, NULL);

    // 重置按钮
    lv_obj_t *reset_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(reset_btn, 100, 35);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xFF6B35), 0);
    lv_obj_t *reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, LV_SYMBOL_REFRESH " 重置");
    lv_obj_set_style_text_font(reset_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(reset_btn, [](lv_event_t *e) {
        ESP_LOGI(TAG, "Resetting PMU service...");
        pmu_service_reset();
    }, LV_EVENT_CLICKED, NULL);
}

/**
 * @brief UI更新定时器回调
 */
static void update_ui_timer_cb(lv_timer_t *timer)
{
    // 主动获取PMU数据，确保数据始终是最新的
    pmu_data_t current_data;
    esp_err_t ret = pmu_service_get_data(&current_data);
    if (ret == ESP_OK) {
        // 缓存数据并标记更新
        g_cached_pmu_data = current_data;
        g_data_updated = true;
    }

    // 更新UI显示
    if (g_data_updated) {
        update_ui_display();
    }
}

/**
 * @brief PMU页面删除事件回调
 */
static void pmu_page_delete_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "PMU page delete event triggered, cleaning up resources...");

    // 停止PMU服务
    pmu_service_stop();

    // 清理定时器
    if (g_update_timer) {
        lv_timer_del(g_update_timer);
        g_update_timer = NULL;
    }

    // 重置全局变量
    g_status_label = NULL;
    g_battery_bar = NULL;
    g_battery_label = NULL;
    g_voltage_label = NULL;
    g_current_label = NULL;
    g_temp_label = NULL;
    g_charge_status_label = NULL;
    g_vbus_status_label = NULL;
    g_dc1_switch = NULL;
    g_dc3_switch = NULL;
    g_aldo1_switch = NULL;
    g_aldo2_switch = NULL;
    g_aldo3_switch = NULL;
    g_aldo4_switch = NULL;
    g_bldo1_switch = NULL;
    g_bldo2_switch = NULL;
    g_data_updated = false;

    ESP_LOGI(TAG, "PMU page cleanup completed");
}

/**
 * @brief 创建PMU页面
 */
lv_obj_t* createPage_pmu(void)
{
    ESP_LOGI(TAG, "Creating PMU page...");

    // 创建主屏幕
    g_page_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_page_screen, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_pad_all(g_page_screen, 8, 0);

    // 添加页面删除事件回调
    lv_obj_add_event_cb(g_page_screen, pmu_page_delete_cb, LV_EVENT_DELETE, NULL);

    // 创建滚动容器
    lv_obj_t *scroll_cont = lv_obj_create(g_page_screen);
    lv_obj_set_size(scroll_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(scroll_cont, 0, 0);
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(scroll_cont, 0, 0);
    lv_obj_set_flex_flow(scroll_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(scroll_cont, LV_SCROLLBAR_MODE_AUTO);

    // 创建各个区域
    create_status_section(scroll_cont);
    create_battery_section(scroll_cont);
    create_power_channels_section(scroll_cont);
    create_control_buttons(scroll_cont);

    // 初始化PMU服务
    esp_err_t ret = pmu_service_init();
    if (ret == ESP_OK) {
        // 注册数据回调
        pmu_service_register_data_callback(pmu_data_callback);
        pmu_service_register_event_callback(pmu_event_callback);

        // 启动数据采集（进一步降低频率到5秒间隔，大幅减少CPU负载）
        ret = pmu_service_start(5000);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "PMU service started successfully");

            // 更新状态显示
            if (g_status_label) {
                lv_label_set_text(g_status_label, LV_SYMBOL_WIFI " PMU 已连接  " LV_SYMBOL_REFRESH " 正常运行");
                lv_obj_set_style_text_color(g_status_label, lv_color_hex(0x00AA00), 0);
            }
        } else {
            ESP_LOGE(TAG, "Failed to start PMU service: %s", esp_err_to_name(ret));
            if (g_status_label) {
                lv_label_set_text(g_status_label, LV_SYMBOL_WARNING " PMU 服务启动失败");
                lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xAA0000), 0);
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to initialize PMU service: %s", esp_err_to_name(ret));
        if (g_status_label) {
            lv_label_set_text(g_status_label, "PMU Init Failed");
            lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xAA0000), 0);
        }
    }

    // 创建UI更新定时器（500ms更新一次UI，降低刷新频率）
    g_update_timer = lv_timer_create(update_ui_timer_cb, 500, NULL);

    ESP_LOGI(TAG, "PMU page created successfully");
    return g_page_screen;
}
