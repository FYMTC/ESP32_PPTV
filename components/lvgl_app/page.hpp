#pragma once
#include "lvgl.h"
class Page {
public:
    virtual ~Page() {}
    // 页面根对象
    lv_obj_t* root = nullptr;
    // onCreate返回页面根对象
    virtual lv_obj_t* onCreate() = 0;
    // onDestroy负责销毁页面根对象
    virtual void onDestroy() {
        if (root) {
            lv_obj_del(root);
            root = nullptr;
        }
    }
    virtual void onEvent(uint32_t event, void* data) {}
};
