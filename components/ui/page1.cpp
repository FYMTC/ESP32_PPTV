// page1.cpp
#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
extern PageManager g_pageManager;
lv_obj_t* createPage1() {
    lv_obj_t *page = lv_obj_create(NULL);
    lv_obj_t *label = lv_label_create(page);
    lv_label_set_text(label, "this is Page 1");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *btn = lv_btn_create(page);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Jump to Page 2");
    lv_obj_add_event_cb(btn, [](lv_event_t *e){
        g_pageManager.gotoPage("page2", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
    }, LV_EVENT_CLICKED, NULL);
    return page;
}
