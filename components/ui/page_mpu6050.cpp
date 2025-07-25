#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include "mpu6050_service.h"
#include "esp_log.h"
#include <cmath>

extern PageManager g_pageManager;

static const char* TAG = "page_mpu6050";

// 页面UI组件
static lv_obj_t* g_page_screen = NULL;
static lv_obj_t* g_status_label = NULL;
static lv_obj_t* g_temp_label = NULL;

// 加速度计组件
static lv_obj_t* g_accel_chart = NULL;
static lv_chart_series_t* g_accel_x_series = NULL;
static lv_chart_series_t* g_accel_y_series = NULL;
static lv_chart_series_t* g_accel_z_series = NULL;
static lv_obj_t* g_accel_values_label = NULL;

// 陀螺仪组件
static lv_obj_t* g_gyro_chart = NULL;
static lv_chart_series_t* g_gyro_x_series = NULL;
static lv_chart_series_t* g_gyro_y_series = NULL;
static lv_chart_series_t* g_gyro_z_series = NULL;
static lv_obj_t* g_gyro_values_label = NULL;

// 3D指示器组件
static lv_obj_t* g_orientation_canvas = NULL;
static lv_obj_t* g_orientation_label = NULL;

// 定时器和数据
static lv_timer_t* g_update_timer = NULL;
static mpu6050_data_t g_last_data = {0};

// 前向声明
static void mpu6050_data_callback(const mpu6050_data_t* data);
static void update_ui_timer_cb(lv_timer_t* timer);
static void update_charts(const mpu6050_data_t* data);
static void update_orientation_display(const mpu6050_data_t* data);
static void create_status_section(lv_obj_t* parent);
static void create_charts_section(lv_obj_t* parent);
static void create_orientation_section(lv_obj_t* parent);
static void create_control_buttons(lv_obj_t* parent);

// 数据更新回调函数
static void mpu6050_data_callback(const mpu6050_data_t* data)
{
    if (data != NULL) {
        g_last_data = *data;
    }
}

// UI更新定时器回调
static void update_ui_timer_cb(lv_timer_t* timer)
{
    if (!g_last_data.is_valid) {
        return;
    }

    // 更新状态信息
    if (g_status_label) {
        lv_label_set_text_fmt(g_status_label, 
            LV_SYMBOL_WIFI " Connected  " LV_SYMBOL_REFRESH " %.1f Hz", 
            1000.0f / 50.0f); // 假设50ms更新间隔
    }

    // 更新温度显示
    if (g_temp_label) {
        lv_label_set_text_fmt(g_temp_label, 
            LV_SYMBOL_EYE_OPEN " Temperature: %.1f°C", g_last_data.temperature);
    }

    // 更新加速度计数值显示
    if (g_accel_values_label) {
        lv_label_set_text_fmt(g_accel_values_label,
            "X: %6.2f g\nY: %6.2f g\nZ: %6.2f g",
            g_last_data.accel_x, g_last_data.accel_y, g_last_data.accel_z);
    }

    // 更新陀螺仪数值显示
    if (g_gyro_values_label) {
        lv_label_set_text_fmt(g_gyro_values_label,
            "X: %6.1f °/s\nY: %6.1f °/s\nZ: %6.1f °/s",
            g_last_data.gyro_x, g_last_data.gyro_y, g_last_data.gyro_z);
    }

    // 更新图表
    update_charts(&g_last_data);
    
    // 更新方向显示
    update_orientation_display(&g_last_data);
}

// 更新图表数据
static void update_charts(const mpu6050_data_t* data)
{
    if (!data || !g_accel_chart || !g_gyro_chart) return;

    // 更新加速度计图表（将数据缩放到合适范围）
    lv_chart_set_next_value(g_accel_chart, g_accel_x_series, (int32_t)(data->accel_x * 20 + 50));
    lv_chart_set_next_value(g_accel_chart, g_accel_y_series, (int32_t)(data->accel_y * 20 + 50));
    lv_chart_set_next_value(g_accel_chart, g_accel_z_series, (int32_t)(data->accel_z * 20 + 50));

    // 更新陀螺仪图表（将数据缩放到合适范围）
    lv_chart_set_next_value(g_gyro_chart, g_gyro_x_series, (int32_t)(data->gyro_x / 5 + 50));
    lv_chart_set_next_value(g_gyro_chart, g_gyro_y_series, (int32_t)(data->gyro_y / 5 + 50));
    lv_chart_set_next_value(g_gyro_chart, g_gyro_z_series, (int32_t)(data->gyro_z / 5 + 50));
}

// 更新方向显示
static void update_orientation_display(const mpu6050_data_t* data)
{
    if (!data || !g_orientation_label) return;

    // 简单的倾斜角度计算
    float roll = atan2(data->accel_y, data->accel_z) * 180.0f / M_PI;
    float pitch = atan2(-data->accel_x, sqrt(data->accel_y * data->accel_y + data->accel_z * data->accel_z)) * 180.0f / M_PI;
    
    lv_label_set_text_fmt(g_orientation_label,
        "Roll:  %6.1f°\nPitch: %6.1f°", roll, pitch);
}

// 创建状态信息区域
static void create_status_section(lv_obj_t* parent)
{
    // 状态容器
    lv_obj_t* status_cont = lv_obj_create(parent);
    lv_obj_set_size(status_cont, LV_PCT(100), 60);
    lv_obj_set_style_pad_all(status_cont, 8, 0);
    lv_obj_set_style_bg_color(status_cont, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_border_width(status_cont, 0, 0);
    lv_obj_set_style_radius(status_cont, 8, 0);

    // 状态标签
    g_status_label = lv_label_create(status_cont);
    lv_label_set_text(g_status_label, LV_SYMBOL_CLOSE " Disconnected");
    lv_obj_set_style_text_color(g_status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_status_label, LV_ALIGN_LEFT_MID, 0, -8);

    // 温度标签
    g_temp_label = lv_label_create(status_cont);
    lv_label_set_text(g_temp_label, LV_SYMBOL_EYE_OPEN " Temperature: --°C");
    lv_obj_set_style_text_color(g_temp_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_temp_label, &lv_font_montserrat_12, 0);
    lv_obj_align(g_temp_label, LV_ALIGN_LEFT_MID, 0, 8);
}

// 创建图表区域
static void create_charts_section(lv_obj_t* parent)
{
    // 图表容器
    lv_obj_t* charts_cont = lv_obj_create(parent);
    lv_obj_set_size(charts_cont, LV_PCT(100), 280);
    lv_obj_set_style_pad_all(charts_cont, 8, 0);
    lv_obj_set_style_border_width(charts_cont, 1, 0);
    lv_obj_set_style_border_color(charts_cont, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(charts_cont, 8, 0);
    lv_obj_set_flex_flow(charts_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(charts_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 加速度计部分
    lv_obj_t* accel_section = lv_obj_create(charts_cont);
    lv_obj_set_size(accel_section, LV_PCT(48), LV_PCT(100));
    lv_obj_set_style_pad_all(accel_section, 4, 0);
    lv_obj_set_style_border_width(accel_section, 0, 0);
    lv_obj_set_flex_flow(accel_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(accel_section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 加速度计标题
    lv_obj_t* accel_title = lv_label_create(accel_section);
    lv_label_set_text(accel_title, LV_SYMBOL_SHUFFLE " Accelerometer");
    lv_obj_set_style_text_font(accel_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(accel_title, lv_color_hex(0x4CAF50), 0);

    // 加速度计图表
    g_accel_chart = lv_chart_create(accel_section);
    lv_obj_set_size(g_accel_chart, LV_PCT(100), 120);
    lv_chart_set_type(g_accel_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_accel_chart, 50);
    lv_chart_set_range(g_accel_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_size(g_accel_chart, 0, 0, LV_PART_INDICATOR);
    
    g_accel_x_series = lv_chart_add_series(g_accel_chart, lv_color_hex(0xFF5722), LV_CHART_AXIS_PRIMARY_Y);
    g_accel_y_series = lv_chart_add_series(g_accel_chart, lv_color_hex(0x4CAF50), LV_CHART_AXIS_PRIMARY_Y);
    g_accel_z_series = lv_chart_add_series(g_accel_chart, lv_color_hex(0x2196F3), LV_CHART_AXIS_PRIMARY_Y);

    // 加速度计数值显示
    g_accel_values_label = lv_label_create(accel_section);
    lv_label_set_text(g_accel_values_label, "X: ------ g\nY: ------ g\nZ: ------ g");
    lv_obj_set_style_text_font(g_accel_values_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(g_accel_values_label, LV_TEXT_ALIGN_LEFT, 0);

    // 陀螺仪部分
    lv_obj_t* gyro_section = lv_obj_create(charts_cont);
    lv_obj_set_size(gyro_section, LV_PCT(48), LV_PCT(100));
    lv_obj_set_style_pad_all(gyro_section, 4, 0);
    lv_obj_set_style_border_width(gyro_section, 0, 0);
    lv_obj_set_flex_flow(gyro_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(gyro_section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 陀螺仪标题
    lv_obj_t* gyro_title = lv_label_create(gyro_section);
    lv_label_set_text(gyro_title, LV_SYMBOL_LOOP " Gyroscope");
    lv_obj_set_style_text_font(gyro_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(gyro_title, lv_color_hex(0xFF9800), 0);

    // 陀螺仪图表
    g_gyro_chart = lv_chart_create(gyro_section);
    lv_obj_set_size(g_gyro_chart, LV_PCT(100), 120);
    lv_chart_set_type(g_gyro_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_gyro_chart, 50);
    lv_chart_set_range(g_gyro_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_obj_set_style_size(g_gyro_chart, 0, 0, LV_PART_INDICATOR);
    
    g_gyro_x_series = lv_chart_add_series(g_gyro_chart, lv_color_hex(0xFF5722), LV_CHART_AXIS_PRIMARY_Y);
    g_gyro_y_series = lv_chart_add_series(g_gyro_chart, lv_color_hex(0x4CAF50), LV_CHART_AXIS_PRIMARY_Y);
    g_gyro_z_series = lv_chart_add_series(g_gyro_chart, lv_color_hex(0x2196F3), LV_CHART_AXIS_PRIMARY_Y);

    // 陀螺仪数值显示
    g_gyro_values_label = lv_label_create(gyro_section);
    lv_label_set_text(g_gyro_values_label, "X: ------ °/s\nY: ------ °/s\nZ: ------ °/s");
    lv_obj_set_style_text_font(g_gyro_values_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(g_gyro_values_label, LV_TEXT_ALIGN_LEFT, 0);
}

// 创建方向显示区域
static void create_orientation_section(lv_obj_t* parent)
{
    // 方向容器
    lv_obj_t* orient_cont = lv_obj_create(parent);
    lv_obj_set_size(orient_cont, LV_PCT(100), 80);
    lv_obj_set_style_pad_all(orient_cont, 8, 0);
    lv_obj_set_style_bg_color(orient_cont, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_border_width(orient_cont, 1, 0);
    lv_obj_set_style_border_color(orient_cont, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(orient_cont, 8, 0);
    lv_obj_set_flex_flow(orient_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(orient_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 方向标题
    lv_obj_t* orient_title = lv_label_create(orient_cont);
    lv_label_set_text(orient_title, LV_SYMBOL_GPS " Orientation");
    lv_obj_set_style_text_font(orient_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(orient_title, lv_color_hex(0x9C27B0), 0);

    // 方向数值显示
    g_orientation_label = lv_label_create(orient_cont);
    lv_label_set_text(g_orientation_label, "Roll:  ------°\nPitch: ------°");
    lv_obj_set_style_text_font(g_orientation_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(g_orientation_label, LV_TEXT_ALIGN_RIGHT, 0);
}

// 创建控制按钮
static void create_control_buttons(lv_obj_t* parent)
{
    // 按钮容器
    lv_obj_t* btn_cont = lv_obj_create(parent);
    lv_obj_set_size(btn_cont, LV_PCT(100), 60);
    lv_obj_set_style_pad_all(btn_cont, 8, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 返回按钮
    lv_obj_t* back_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x757575), 0);
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back_btn, [](lv_event_t *e){
        g_pageManager.back(LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300);
    }, LV_EVENT_CLICKED, NULL);

    // 校准按钮
    lv_obj_t* calib_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(calib_btn, 80, 40);
    lv_obj_set_style_bg_color(calib_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_t* calib_label = lv_label_create(calib_btn);
    lv_label_set_text(calib_label, LV_SYMBOL_SETTINGS " Calib");
    lv_obj_set_style_text_color(calib_label, lv_color_white(), 0);
    lv_obj_center(calib_label);
    lv_obj_add_event_cb(calib_btn, [](lv_event_t *e){
        mpu6050_service_calibrate();
        ESP_LOGI(TAG, "Calibration requested");
    }, LV_EVENT_CLICKED, NULL);

    // 重置按钮
    lv_obj_t* reset_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(reset_btn, 80, 40);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xFF5722), 0);
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, LV_SYMBOL_REFRESH " Reset");
    lv_obj_set_style_text_color(reset_label, lv_color_white(), 0);
    lv_obj_center(reset_label);
    lv_obj_add_event_cb(reset_btn, [](lv_event_t *e){
        mpu6050_service_reset();
        ESP_LOGI(TAG, "Reset requested");
    }, LV_EVENT_CLICKED, NULL);
}

/**
 * 创建MPU6050页面
 */
lv_obj_t* createPage_mpu6050(void)
{
    ESP_LOGI(TAG, "Creating MPU6050 page...");

    // 创建主屏幕
    g_page_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_page_screen, lv_color_hex(0xFAFAFA), 0);
    lv_obj_set_style_pad_all(g_page_screen, 8, 0);

    // 创建滚动容器
    lv_obj_t* scroll_cont = lv_obj_create(g_page_screen);
    lv_obj_set_size(scroll_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(scroll_cont, 0, 0);
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(scroll_cont, 0, 0);
    lv_obj_set_flex_flow(scroll_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(scroll_cont, LV_SCROLLBAR_MODE_AUTO);

    // 创建各个区域
    create_status_section(scroll_cont);
    create_charts_section(scroll_cont);
    create_orientation_section(scroll_cont);
    create_control_buttons(scroll_cont);

    // 初始化MPU6050服务
    esp_err_t ret = mpu6050_service_init();
    if (ret == ESP_OK) {
        // 注册数据回调
        mpu6050_service_register_callback(mpu6050_data_callback);
        
        // 启动数据采集（50ms间隔）
        ret = mpu6050_service_start(50);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "MPU6050 service started successfully");
            
            // 更新状态显示
            if (g_status_label) {
                lv_label_set_text(g_status_label, LV_SYMBOL_WIFI " Connected  " LV_SYMBOL_REFRESH " 20.0 Hz");
            }
        } else {
            ESP_LOGE(TAG, "Failed to start MPU6050 service: %s", esp_err_to_name(ret));
            if (g_status_label) {
                lv_label_set_text(g_status_label, LV_SYMBOL_CLOSE " Service Error");
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to initialize MPU6050 service: %s", esp_err_to_name(ret));
        if (g_status_label) {
            lv_label_set_text(g_status_label, LV_SYMBOL_CLOSE " Init Failed");
        }
    }

    // 创建UI更新定时器
    g_update_timer = lv_timer_create(update_ui_timer_cb, 100, NULL); // 100ms更新UI
    
    // 设置页面用户数据，用于清理
    lv_obj_set_user_data(g_page_screen, g_update_timer);

    ESP_LOGI(TAG, "MPU6050 page created successfully");
    return g_page_screen;
}
