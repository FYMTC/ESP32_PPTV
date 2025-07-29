/*
 * QMC5883L三轴磁力计页面实现文件
 * 显示磁力计数据、指南针界面和传感器状态
 */

#include "pages_common.h"
#include "page_manager.h"
#include "pages_common.h"
#include "qmc5883l_service.h"
#include "esp_log.h"
#include <math.h>

extern PageManager g_pageManager;

static const char *TAG = "page_qmc5883l";

// 页面对象
static lv_obj_t *g_page_screen = NULL;
static lv_timer_t *g_update_timer = NULL;

// UI组件
static lv_obj_t *g_status_label = NULL;
static lv_obj_t *g_heading_label = NULL;
static lv_obj_t *g_mag_magnitude_label = NULL;
static lv_obj_t *g_compass_canvas = NULL;
static lv_obj_t *g_mag_values_label = NULL;
static lv_obj_t *g_mag_chart = NULL;

// 图表数据系列
static lv_chart_series_t *g_mag_x_series = NULL;
static lv_chart_series_t *g_mag_y_series = NULL;
static lv_chart_series_t *g_mag_z_series = NULL;

// 指南针画布缓冲区
#define COMPASS_SIZE 160
static lv_color_t compass_buf[COMPASS_SIZE * COMPASS_SIZE];

// 当前数据
static qmc5883l_service_data_t g_current_data = {
    0.0f,  // mag_x
    0.0f,  // mag_y
    0.0f,  // mag_z
    0.0f,  // heading
    0.0f,  // magnitude
    false, // is_valid
    0      // timestamp
};

// 数据滤波相关
#define FILTER_SIZE 10
static float mag_x_history[FILTER_SIZE] = {0};
static float mag_y_history[FILTER_SIZE] = {0};
static float mag_z_history[FILTER_SIZE] = {0};
static int filter_index = 0;
static bool filter_filled = false;

// 前向声明
static void create_status_section(lv_obj_t *parent);
static void create_compass_section(lv_obj_t *parent);
static void create_data_section(lv_obj_t *parent);
static void create_control_buttons(lv_obj_t *parent);
static void qmc5883l_data_callback(const qmc5883l_service_data_t *data);
static void update_ui_timer_cb(lv_timer_t *timer);
static void draw_compass(lv_obj_t *canvas, float heading, float magnitude);
static void update_mag_chart(const qmc5883l_service_data_t *data);
static void apply_data_filter(qmc5883l_service_data_t *data);
static float calculate_filtered_average(float *history, int size);

/**
 * 应用数据滤波
 */
static void apply_data_filter(qmc5883l_service_data_t *data)
{
    // 添加新数据到历史缓冲区
    mag_x_history[filter_index] = data->mag_x;
    mag_y_history[filter_index] = data->mag_y;
    mag_z_history[filter_index] = data->mag_z;

    filter_index = (filter_index + 1) % FILTER_SIZE;
    if (!filter_filled && filter_index == 0)
    {
        filter_filled = true;
    }

    // 只有缓冲区填满后才应用滤波，否则使用原始数据
    if (filter_filled)
    {
        data->mag_x = calculate_filtered_average(mag_x_history, FILTER_SIZE);
        data->mag_y = calculate_filtered_average(mag_y_history, FILTER_SIZE);
        data->mag_z = calculate_filtered_average(mag_z_history, FILTER_SIZE);

        // 重新计算幅度和航向
        data->magnitude = sqrtf(data->mag_x * data->mag_x + data->mag_y * data->mag_y + data->mag_z * data->mag_z);
        data->heading = atan2f(data->mag_y, data->mag_x) * 180.0f / M_PI;
        if (data->heading < 0)
        {
            data->heading += 360.0f;
        }
        ESP_LOGV(TAG, "Filtered data: X=%.1f, Y=%.1f, Z=%.1f", data->mag_x, data->mag_y, data->mag_z);
    }
    else
    {
        ESP_LOGV(TAG, "Using raw data (filter not filled): X=%.1f, Y=%.1f, Z=%.1f", data->mag_x, data->mag_y, data->mag_z);
    }
}

/**
 * 计算滤波后的平均值
 */
static float calculate_filtered_average(float *history, int size)
{
    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        sum += history[i];
    }
    return sum / size;
}

/**
 * 创建状态显示区域
 */
static void create_status_section(lv_obj_t *parent)
{
    // 状态容器
    lv_obj_t *status_cont = lv_obj_create(parent);
    lv_obj_set_size(status_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(status_cont, 8, 0);
    lv_obj_set_style_bg_color(status_cont, lv_color_hex(0xE3F2FD), 0);
    lv_obj_set_style_border_width(status_cont, 1, 0);
    lv_obj_set_style_border_color(status_cont, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(status_cont, 8, 0);
    lv_obj_set_flex_flow(status_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 标题
    lv_obj_t *title_label = lv_label_create(status_cont);
    lv_label_set_text(title_label, "\xEF\x85\x8E"
                                   " QMC");
    // lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(title_label, &NotoSansSC_Medium_3500, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x1976D2), 0);

    // 状态标签
    g_status_label = lv_label_create(status_cont);
    lv_label_set_text(g_status_label, "Disconnected");
    lv_obj_set_style_text_font(g_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xF44336), 0);
}

/**
 * 创建指南针显示区域
 */
static void create_compass_section(lv_obj_t *parent)
{
    // 指南针容器 - 竖向布局，减小高度适配320屏幕
    lv_obj_t *compass_cont = lv_obj_create(parent);
    lv_obj_set_size(compass_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(compass_cont, 8, 0);
    lv_obj_set_style_bg_color(compass_cont, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_border_width(compass_cont, 1, 0);
    lv_obj_set_style_border_color(compass_cont, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(compass_cont, 8, 0);
    lv_obj_set_flex_flow(compass_cont, LV_FLEX_FLOW_COLUMN); // 改为竖向布局
    lv_obj_set_flex_align(compass_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 指南针画布 - 减小尺寸适配竖屏
    g_compass_canvas = lv_canvas_create(compass_cont);
    lv_canvas_set_buffer(g_compass_canvas, compass_buf, COMPASS_SIZE, COMPASS_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_style_bg_color(g_compass_canvas, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_compass_canvas, 2, 0);
    lv_obj_set_style_border_color(g_compass_canvas, lv_color_hex(0x757575), 0);
    lv_obj_set_style_radius(g_compass_canvas, COMPASS_SIZE / 2, 0);

    // 航向信息容器 - 水平布局显示航向和磁场强度
    lv_obj_t *heading_cont = lv_obj_create(compass_cont);
    lv_obj_set_size(heading_cont, LV_PCT(100), 70);
    lv_obj_set_style_pad_all(heading_cont, 4, 0);
    lv_obj_set_style_bg_color(heading_cont, lv_color_white(), 0);
    lv_obj_set_style_border_width(heading_cont, 1, 0);
    lv_obj_set_style_border_color(heading_cont, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(heading_cont, 8, 0);
    lv_obj_set_flex_flow(heading_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(heading_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 航向显示
    lv_obj_t *heading_part = lv_obj_create(heading_cont);
    lv_obj_set_size(heading_part, LV_PCT(45), LV_PCT(100));
    lv_obj_set_style_pad_all(heading_part, 2, 0);
    lv_obj_set_style_border_width(heading_part, 0, 0);
    lv_obj_set_style_bg_opa(heading_part, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(heading_part, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(heading_part, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *heading_title = lv_label_create(heading_part);
    lv_label_set_text(heading_title, LV_SYMBOL_GPS " Heading");
    lv_obj_set_style_text_font(heading_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(heading_title, lv_color_hex(0x4CAF50), 0);

    g_heading_label = lv_label_create(heading_part);
    lv_label_set_text(g_heading_label, "---° N/A");
    lv_obj_set_style_text_font(g_heading_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_heading_label, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_text_align(g_heading_label, LV_TEXT_ALIGN_CENTER, 0);

    // 磁场强度显示
    lv_obj_t *mag_part = lv_obj_create(heading_cont);
    lv_obj_set_size(mag_part, LV_PCT(45), LV_PCT(100));
    lv_obj_set_style_pad_all(mag_part, 2, 0);
    lv_obj_set_style_border_width(mag_part, 0, 0);
    lv_obj_set_style_bg_opa(mag_part, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(mag_part, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mag_part, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *mag_title = lv_label_create(mag_part);
    lv_label_set_text(mag_title, LV_SYMBOL_CHARGE " Magnitude");
    lv_obj_set_style_text_font(mag_title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mag_title, lv_color_hex(0xFF9800), 0);

    g_mag_magnitude_label = lv_label_create(mag_part);
    lv_label_set_text(g_mag_magnitude_label, "--- mG");
    lv_obj_set_style_text_font(g_mag_magnitude_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_mag_magnitude_label, lv_color_hex(0xFF5722), 0);

    // 初始化指南针
    draw_compass(g_compass_canvas, 0.0f, 0.0f);
}

/**
 * 创建数据显示区域
 */
static void create_data_section(lv_obj_t *parent)
{
    // 数据容器 - 减小高度适配竖屏
    lv_obj_t *data_cont = lv_obj_create(parent);
    lv_obj_set_size(data_cont, LV_PCT(100), 120);
    lv_obj_set_style_pad_all(data_cont, 8, 0);
    lv_obj_set_style_bg_color(data_cont, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_border_width(data_cont, 1, 0);
    lv_obj_set_style_border_color(data_cont, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(data_cont, 8, 0);
    lv_obj_set_flex_flow(data_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(data_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    // 数值显示部分
    lv_obj_t *values_cont = lv_obj_create(data_cont);
    lv_obj_set_size(values_cont, LV_PCT(48), LV_PCT(100));
    lv_obj_set_style_pad_all(values_cont, 0, 0);
    lv_obj_set_style_border_width(values_cont, 0, 0);
    lv_obj_set_style_bg_color(values_cont, lv_color_white(), 0);
    lv_obj_set_style_radius(values_cont, 0, 0);
    lv_obj_set_flex_flow(values_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(values_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 磁场数值标题
    lv_obj_t *values_title = lv_label_create(values_cont);
    lv_label_set_text(values_title, " Magnetic Field");
    lv_obj_set_style_text_font(values_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(values_title, lv_color_hex(0x9C27B0), 0);

    // 磁场数值显示 - 使用更小的字体
    g_mag_values_label = lv_label_create(values_cont);
    lv_label_set_text(g_mag_values_label, "X: ----- mG\nY: ----- mG\nZ: ----- mG");
    lv_obj_set_style_text_font(g_mag_values_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(g_mag_values_label, LV_TEXT_ALIGN_LEFT, 0);

    // 图表部分
    lv_obj_t *chart_cont = lv_obj_create(data_cont);
    lv_obj_set_size(chart_cont, LV_PCT(48), LV_PCT(100));
    lv_obj_set_style_pad_all(chart_cont, 0, 0);
    lv_obj_set_style_border_width(chart_cont, 0, 0);
    lv_obj_set_style_bg_color(chart_cont, lv_color_white(), 0);
    lv_obj_set_style_radius(chart_cont, 0, 0);
    lv_obj_set_flex_flow(chart_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(chart_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 图表标题
    lv_obj_t *chart_title = lv_label_create(chart_cont);
    lv_label_set_text(chart_title, " Real-time Data");
    lv_obj_set_style_text_font(chart_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(chart_title, lv_color_hex(0x4CAF50), 0);

    // 磁场图表 - 减小高度
    g_mag_chart = lv_chart_create(chart_cont);
    lv_obj_set_size(g_mag_chart, LV_PCT(100), 80);
    lv_chart_set_type(g_mag_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_mag_chart, 30);
    lv_chart_set_range(g_mag_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_size(g_mag_chart, 0, 0, LV_PART_INDICATOR);

    g_mag_x_series = lv_chart_add_series(g_mag_chart, lv_color_hex(0xFF5722), LV_CHART_AXIS_PRIMARY_Y);
    g_mag_y_series = lv_chart_add_series(g_mag_chart, lv_color_hex(0x4CAF50), LV_CHART_AXIS_PRIMARY_Y);
    g_mag_z_series = lv_chart_add_series(g_mag_chart, lv_color_hex(0x2196F3), LV_CHART_AXIS_PRIMARY_Y);
}

/**
 * 创建控制按钮
 */
static void create_control_buttons(lv_obj_t *parent)
{
    // 按钮容器
    lv_obj_t *btn_cont = lv_obj_create(parent);
    lv_obj_set_size(btn_cont, LV_PCT(100), 60);
    lv_obj_set_style_pad_all(btn_cont, 8, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x757575), 0);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, [](lv_event_t *e)
                        { g_pageManager.back(LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300); }, LV_EVENT_CLICKED, NULL);

    // 校准按钮
    lv_obj_t *calib_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(calib_btn, 80, 40);
    lv_obj_set_style_bg_color(calib_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_t *calib_label = lv_label_create(calib_btn);
    lv_label_set_text(calib_label, LV_SYMBOL_SETTINGS " Calib");
    lv_obj_set_style_text_color(calib_label, lv_color_white(), 0);
    lv_obj_center(calib_label);
    lv_obj_add_event_cb(calib_btn, [](lv_event_t *e)
                        {
        qmc5883l_service_calibrate();
        ESP_LOGI(TAG, "Calibration requested"); }, LV_EVENT_CLICKED, NULL);

    // 重置按钮
    lv_obj_t *reset_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(reset_btn, 80, 40);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xFF5722), 0);
    lv_obj_t *reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, LV_SYMBOL_REFRESH " Reset");
    lv_obj_set_style_text_color(reset_label, lv_color_white(), 0);
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(reset_btn, [](lv_event_t *e)
                        {
        qmc5883l_service_reset();
        ESP_LOGI(TAG, "Reset requested"); }, LV_EVENT_CLICKED, NULL);
}

/**
 * QMC5883L数据更新回调
 */
static void qmc5883l_data_callback(const qmc5883l_service_data_t *data)
{
    if (data && data->is_valid)
    {
        // 复制数据（线程安全）
        qmc5883l_service_data_t filtered_data = *data;

        // 应用数据滤波
        apply_data_filter(&filtered_data);

        // 数据稳定性检查 - 放宽范围，允许低磁场强度
        if (filtered_data.magnitude >= 0.1f && filtered_data.magnitude < 10000.0f)
        {
            g_current_data = filtered_data;
            ESP_LOGV(TAG, "Magnetic data updated: X=%.1f, Y=%.1f, Z=%.1f, magnitude=%.1f, heading=%.1f",
                     filtered_data.mag_x, filtered_data.mag_y, filtered_data.mag_z,
                     filtered_data.magnitude, filtered_data.heading);
        }
        else
        {
            ESP_LOGW(TAG, "Magnetic data out of range: magnitude=%.1f", filtered_data.magnitude);
        }
    }
}

/**
 * UI更新定时器回调
 */
static void update_ui_timer_cb(lv_timer_t *timer)
{
    // 主动获取传感器数据
    qmc5883l_service_data_t current_data;
    if (qmc5883l_service_get_data(&current_data) == ESP_OK)
    {
        g_current_data = current_data;
    }

    // 如果没有有效数据，显示"等待数据"状态
    if (!g_current_data.is_valid)
    {
        if (g_mag_values_label)
        {
            lv_label_set_text(g_mag_values_label, "Waiting for\nmagnetometer\ndata...");
        }
        if (g_heading_label)
        {
            lv_label_set_text(g_heading_label, "--- N/A");
        }
        if (g_mag_magnitude_label)
        {
            lv_label_set_text(g_mag_magnitude_label, "--- mG");
        }
        return;
    }

    // 更新磁场数值显示
    if (g_mag_values_label)
    {
        lv_label_set_text_fmt(g_mag_values_label,
                              "X: %6.1f mG\nY: %6.1f mG\nZ: %6.1f mG",
                              g_current_data.mag_x, g_current_data.mag_y, g_current_data.mag_z);
    }

    // 更新航向显示
    if (g_heading_label)
    {
        const char *direction = "N/A";
        float heading = g_current_data.heading;

        if (heading >= 337.5 || heading < 22.5)
            direction = "N";
        else if (heading >= 22.5 && heading < 67.5)
            direction = "NE";
        else if (heading >= 67.5 && heading < 112.5)
            direction = "E";
        else if (heading >= 112.5 && heading < 157.5)
            direction = "SE";
        else if (heading >= 157.5 && heading < 202.5)
            direction = "S";
        else if (heading >= 202.5 && heading < 247.5)
            direction = "SW";
        else if (heading >= 247.5 && heading < 292.5)
            direction = "W";
        else if (heading >= 292.5 && heading < 337.5)
            direction = "NW";

        lv_label_set_text_fmt(g_heading_label, "%.1f° %s", heading, direction);
    }

    // 更新磁场强度显示
    if (g_mag_magnitude_label)
    {
        lv_label_set_text_fmt(g_mag_magnitude_label, "%.1f mG", g_current_data.magnitude);
    }

    // 更新指南针
    if (g_compass_canvas)
    {
        draw_compass(g_compass_canvas, g_current_data.heading, g_current_data.magnitude);
    }

    // 更新图表
    update_mag_chart(&g_current_data);
}

/**
 * 绘制指南针
 */
static void draw_compass(lv_obj_t *canvas, float heading, float magnitude)
{
    if (!canvas)
        return;

    // 清除画布
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);

    int center_x = COMPASS_SIZE / 2;
    int center_y = COMPASS_SIZE / 2;
    int radius = COMPASS_SIZE / 2 - 10;

    // 初始化层用于绘制
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    // 绘制圆形刻度
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x757575);
    line_dsc.width = 2;

    for (int i = 0; i < 360; i += 10)
    {
        float angle_rad = i * M_PI / 180.0f;
        int line_len = (i % 30 == 0) ? 15 : 8; // 主刻度长，次刻度短

        line_dsc.p1.x = (int32_t)(center_x + (radius - line_len) * cosf(angle_rad));
        line_dsc.p1.y = (int32_t)(center_y + (radius - line_len) * sinf(angle_rad));
        line_dsc.p2.x = (int32_t)(center_x + radius * cosf(angle_rad));
        line_dsc.p2.y = (int32_t)(center_y + radius * sinf(angle_rad));

        lv_draw_line(&layer, &line_dsc);
    }

    // 绘制指南针指针 - 降低显示阈值
    if (magnitude >= 0.1f)
    {
        float angle_rad = (heading - 90) * M_PI / 180.0f;

        // 北针（红色）- 指向磁北
        line_dsc.color = lv_color_hex(0xF44336);
        line_dsc.width = 4;
        line_dsc.p1.x = center_x;
        line_dsc.p1.y = center_y;
        line_dsc.p2.x = (int32_t)(center_x + (radius - 20) * cosf(angle_rad));
        line_dsc.p2.y = (int32_t)(center_y + (radius - 20) * sinf(angle_rad));
        lv_draw_line(&layer, &line_dsc);

        // 南针（灰色） - 指向相反方向
        line_dsc.color = lv_color_hex(0x757575);
        line_dsc.width = 2;
        line_dsc.p1.x = center_x;
        line_dsc.p1.y = center_y;
        line_dsc.p2.x = (int32_t)(center_x - (radius - 30) * cosf(angle_rad));
        line_dsc.p2.y = (int32_t)(center_y - (radius - 30) * sinf(angle_rad));
        lv_draw_line(&layer, &line_dsc);
    }
    else
    {
        // 如果没有磁场数据，显示一个灰色的指示器
        line_dsc.color = lv_color_hex(0xCCCCCC);
        line_dsc.width = 2;
        line_dsc.p1.x = center_x;
        line_dsc.p1.y = center_y - (radius - 20);
        line_dsc.p2.x = center_x;
        line_dsc.p2.y = center_y + (radius - 20);
        lv_draw_line(&layer, &line_dsc);

        line_dsc.p1.x = center_x - (radius - 20);
        line_dsc.p1.y = center_y;
        line_dsc.p2.x = center_x + (radius - 20);
        line_dsc.p2.y = center_y;
        lv_draw_line(&layer, &line_dsc);
    }

    // 绘制中心点
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_hex(0x424242);
    rect_dsc.radius = 4;
    lv_area_t center_area = {center_x - 4, center_y - 4, center_x + 4, center_y + 4};
    lv_draw_rect(&layer, &rect_dsc, &center_area);

    // 完成层绘制
    lv_canvas_finish_layer(canvas, &layer);
}

/**
 * 更新磁场图表
 */
static void update_mag_chart(const qmc5883l_service_data_t *data)
{
    if (!g_mag_chart || !data)
        return;

    // 归一化数据到0-100范围显示
    float max_val = fmaxf(fmaxf(fabsf(data->mag_x), fabsf(data->mag_y)), fabsf(data->mag_z));
    if (max_val < 100.0f)
        max_val = 100.0f;

    int x_val = (int)((data->mag_x / max_val) * 50 + 50);
    int y_val = (int)((data->mag_y / max_val) * 50 + 50);
    int z_val = (int)((data->mag_z / max_val) * 50 + 50);

    // 限制范围
    x_val = (x_val < 0) ? 0 : (x_val > 100) ? 100
                                            : x_val;
    y_val = (y_val < 0) ? 0 : (y_val > 100) ? 100
                                            : y_val;
    z_val = (z_val < 0) ? 0 : (z_val > 100) ? 100
                                            : z_val;

    lv_chart_set_next_value(g_mag_chart, g_mag_x_series, x_val);
    lv_chart_set_next_value(g_mag_chart, g_mag_y_series, y_val);
    lv_chart_set_next_value(g_mag_chart, g_mag_z_series, z_val);
}

/**
 * 创建QMC5883L页面
 */
lv_obj_t *createPage_qmc5883l(void)
{
    ESP_LOGI(TAG, "Creating QMC5883L page...");

    // 创建主屏幕
    g_page_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_page_screen, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_pad_all(g_page_screen, 8, 0);

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
    create_compass_section(scroll_cont);
    create_data_section(scroll_cont);
    create_control_buttons(scroll_cont);

    // 初始化QMC5883L服务
    esp_err_t ret = qmc5883l_service_init();
    if (ret == ESP_OK)
    {
        // 注册数据回调
        qmc5883l_service_register_callback(qmc5883l_data_callback);

        // 启动数据采集（500ms间隔，与传感器10Hz配置匹配）
        ret = qmc5883l_service_start(500);
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "QMC5883L service started successfully");

            // 更新状态显示
            if (g_status_label)
            {
                lv_label_set_text(g_status_label, " Connected  " LV_SYMBOL_REFRESH " 2.0 Hz");
                lv_obj_set_style_text_color(g_status_label, lv_color_hex(0x4CAF50), 0);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to start QMC5883L service: %s", esp_err_to_name(ret));
            if (g_status_label)
            {
                lv_label_set_text(g_status_label, " Service Error");
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize QMC5883L service: %s", esp_err_to_name(ret));
        if (g_status_label)
        {
            lv_label_set_text(g_status_label, " Init Failed");
        }
    }

    // 创建UI更新定时器（500ms更新UI，降低更新频率）
    g_update_timer = lv_timer_create(update_ui_timer_cb, 100, NULL);

    // 立即触发一次数据更新，确保UI有初始数据
    qmc5883l_service_data_t initial_data;
    if (qmc5883l_service_get_data(&initial_data) == ESP_OK)
    {
        g_current_data = initial_data;
    }

    // 设置页面用户数据，用于清理
    lv_obj_set_user_data(g_page_screen, g_update_timer);

    ESP_LOGI(TAG, "QMC5883L page created successfully");
    return g_page_screen;
}
