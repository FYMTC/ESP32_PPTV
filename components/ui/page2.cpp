// page2.cpp
#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
extern PageManager g_pageManager;
lv_obj_t* createPage2() {
    lv_obj_t *page = lv_obj_create(NULL);
    lv_obj_t *label = lv_label_create(page);
    lv_label_set_text(label, "this is Page 2");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *back_btn = lv_btn_create(page);
    lv_obj_align(back_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back to Page 1");
    lv_obj_add_event_cb(back_btn, [](lv_event_t *e){
        //g_pageManager.back(LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300);
        g_pageManager.back();
    }, LV_EVENT_CLICKED, NULL);
    return page;
}
