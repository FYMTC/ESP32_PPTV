#include "main_page.hpp"
#include "lvgl.h"
#include "page_manager.hpp"
#include "second_page.hpp"

static void to_second_page_cb(lv_event_t* e) {
    PageManager::instance().push(new SecondPage());
}
lv_obj_t* MainPage::onCreate() {
    root = lv_obj_create(NULL); // 创建独立页面
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, LV_HOR_RES, LV_VER_RES);

    lv_obj_t* label = lv_label_create(root);
    lv_label_set_text(label, "Hello, LVGL App Main Page!");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t* btn = lv_btn_create(root);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Go to Second Page");
    lv_obj_center(btn_label);
    lv_obj_add_event_cb(btn, to_second_page_cb, LV_EVENT_CLICKED, NULL);
    lv_scr_load_anim(root, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    return root;
}
void MainPage::onDestroy() {
    Page::onDestroy();
}
