#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
extern PageManager g_pageManager;

static void add_data(lv_timer_t * t)
{
    lv_obj_t * chart = (lv_obj_t *)lv_timer_get_user_data(t);
    lv_chart_series_t * ser = lv_chart_get_series_next(chart, NULL);

    lv_chart_set_next_value(chart, ser, (int32_t)lv_rand(10, 90));

    uint32_t p = lv_chart_get_point_count(chart);
    uint32_t s = lv_chart_get_x_start_point(chart, ser);
    int32_t * a = lv_chart_get_series_y_array(chart, ser);

    a[(s + 1) % p] = LV_CHART_POINT_NONE;
    a[(s + 2) % p] = LV_CHART_POINT_NONE;
    a[(s + 2) % p] = LV_CHART_POINT_NONE;

    lv_chart_refresh(chart);
}

/**
 * Circular line chart with gap
 */
lv_obj_t* createPage_mpu6050(void)
{

    lv_obj_t *mpu6050_screen = lv_obj_create(NULL);
    lv_obj_t *status = lv_obj_create(mpu6050_screen);
    lv_obj_set_size(status, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_ver(status, 0, 0);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_COLUMN); // 按钮内容弹性行增长
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_width(status, 0, 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *mpu6050_screen_backbtn = lv_btn_create(status);
    lv_obj_set_size(mpu6050_screen_backbtn, LV_PCT(100), 20);
    lv_obj_t *img = lv_img_create(mpu6050_screen_backbtn);
    lv_img_set_src(img, LV_SYMBOL_NEW_LINE);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *label = lv_label_create(mpu6050_screen_backbtn);
    lv_label_set_text(label, "BACK");
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(mpu6050_screen_backbtn, [](lv_event_t *e){
        g_pageManager.back(LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300);
        //g_pageManager.back();
    }, LV_EVENT_CLICKED, NULL);


    lv_obj_t * chart = lv_chart_create(mpu6050_screen);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_size(chart, LV_PCT(100), 200);
    lv_obj_center(chart);

    lv_chart_set_point_count(chart, 80);
    lv_chart_series_t * ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    /*Prefill with data*/
    uint32_t i;
    for(i = 0; i < 80; i++) {
        lv_chart_set_next_value(chart, ser, (int32_t)lv_rand(10, 90));
    }

    lv_timer_t* timer = lv_timer_create(add_data, 30, chart);
    lv_obj_set_user_data(mpu6050_screen, timer);
    return mpu6050_screen;
}
