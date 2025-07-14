#include "third_page.hpp"
#include "lvgl.h"
#include "page_manager.hpp"

static void back_cb(lv_event_t* e) {
    lv_async_call([](void*){
        PageManager::instance().pop();
    }, nullptr);
}
lv_obj_t* ThirdPage::onCreate() {
    root = lv_obj_create(NULL);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, LV_HOR_RES, LV_VER_RES);

    lv_obj_t* label = lv_label_create(root);
    lv_label_set_text(label, "This is Third Page");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t* btn_back = lv_btn_create(root);
    lv_obj_align(btn_back, LV_ALIGN_CENTER, 0, 20);
    lv_obj_t* label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_center(label_back);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_scr_load_anim(root, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    return root;
}
void ThirdPage::onDestroy() {
    Page::onDestroy();
}
