#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include "esp_log.h"
#include "access/Check_Mark.c"
#include "access/Ripple_loading_animation.c"
#include "access/Material_wave_loading.c"
#include "access/Liquid_4_Dot_Loader.c"
#include "access/loading.c"
#include "access/Rocket_in_space.c"
#include "access/MtkNxUsr7K.c"
#include "access/MkdrU8kJNP.c"
#include  "access/kIz6PnBYLY.c"

extern PageManager g_pageManager;
uint8_t* buf = nullptr;

static void lottie_anim_completed_cb(lv_anim_t *a)
{
    g_pageManager.gotoPageAndDestroy("page_menu", LV_SCR_LOAD_ANIM_FADE_IN, 300);
     if (buf) {
        heap_caps_free(buf);
        buf = nullptr;
    }
}

lv_obj_t* createPage_prepage(void)
{


    lv_obj_t * pre_page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pre_page, lv_color_black(), 0); // 设置背景为黑色
    lv_obj_t * lottie = lv_lottie_create(pre_page);

    //lv_lottie_set_src_data(lottie, Material_wave_loading, material_wave_loading_len);
    //lv_lottie_set_src_data(lottie, Ripple_loading_animation, Ripple_loading_animation_len);
    //lv_lottie_set_src_data(lottie, loading, loading_len); // 散点加载动画
    //lv_lottie_set_src_data(lottie, MtkNxUsr7K, MtkNxUsr7K_len);//简单打勾
    //lv_lottie_set_src_data(lottie, kIz6PnBYLY, kIz6PnBYLY_len); // 使用自定义动画


    lv_lottie_set_src_data(lottie, Check_Mark, Check_Mark_len);//条带打勾动画
    //lv_lottie_set_src_data(lottie, Liquid_4_Dot_Loader, Liquid_4_Dot_Loader_len); // 使用液体加载动画
    //lv_lottie_set_src_data(lottie, Rocket_in_space, Rocket_in_space_len);
    //lv_lottie_set_src_data(lottie, MkdrU8kJNP, MkdrU8kJNP_len);//撒花
    //lv_lottie_set_src_data(lottie, Material_wave_loading, material_wave_loading_len);
    

    uint8_t* buf = (uint8_t*)heap_caps_calloc(240 * 240 * 4, sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE("PrePage", "Failed to allocate memory for Lottie buffer");
        return pre_page; // 返回空页面以避免崩溃
    }
    lv_lottie_set_buffer(lottie, 240, 240, buf);
    lv_obj_center(lottie);

    lv_anim_t *a = lv_lottie_get_anim(lottie);
	a->repeat_cnt=1;
    if (a) {
       a->completed_cb = lottie_anim_completed_cb;
    }

    return pre_page;
}
