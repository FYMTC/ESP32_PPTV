#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include <iostream>
#include <cstring>
#include "access/Check_Mark.c"
#include "access/Ripple_loading_animation.c"
#include "access/Material_wave_loading.c"
#include "access/loading.c"
extern PageManager g_pageManager;
lv_obj_t * lottie = NULL;
static void lottie_buf_free_cb(lv_event_t *e)
{
    
    g_pageManager.back(LV_SCR_LOAD_ANIM_FADE_OUT, 300);
    if (lottie) {
        lv_obj_del(lottie);
    }
    uint8_t* buf = (uint8_t*)lv_event_get_user_data(e);
    if (buf) {
        heap_caps_free(buf);
    }
}

lv_obj_t* createPage_settings(void)
{

    
    lv_obj_t * setting_page = lv_obj_create(NULL);
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


    uint8_t *buf = (uint8_t*)heap_caps_malloc(240 * 240 * 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_obj_add_event_cb(screen_backbtn, lottie_buf_free_cb, LV_EVENT_CLICKED, buf);

    lottie = lv_lottie_create(status);

    //lv_lottie_set_src_data(lottie, Material_wave_loading, material_wave_loading_len);
    //lv_lottie_set_src_data(lottie, Ripple_loading_animation, Ripple_loading_animation_len);
    //lv_lottie_set_src_data(lottie, Check_Mark, Check_Mark_len);//条带打勾动画
    lv_lottie_set_src_data(lottie, loading, loading_len); // 散点加载动画


    
    lv_lottie_set_buffer(lottie, 240, 240, buf);

    lv_obj_center(lottie);

    return setting_page;
}
