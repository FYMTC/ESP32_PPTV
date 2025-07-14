// page_manager.h
#pragma once
#include "lvgl/lvgl.h"
#include <stack>
#include <functional>
#include <string>
#include <unordered_map>


class PageManager {
public:
    using PageCreateFunc = std::function<lv_obj_t*()>;

    void registerPage(const std::string& name, PageCreateFunc func);
    // 普通切换
    void gotoPage(const std::string& name);
    // 带动画切换
    void gotoPage(const std::string& name, lv_screen_load_anim_t anim_type, uint32_t time);
    // 跳转并销毁当前页面
    void gotoPageAndDestroy(const std::string& name);
    // 带动画跳转并销毁当前页面
    void gotoPageAndDestroy(const std::string& name, lv_screen_load_anim_t anim_type, uint32_t time);

    void back();
    void back(lv_screen_load_anim_t anim_type, uint32_t time);

    void clear();
    std::string currentPage() const;
private:
    struct PageInfo {
        std::string name;
        lv_obj_t* page;
        bool keep;
    };
    std::stack<PageInfo> pageStack;
    std::unordered_map<std::string, PageCreateFunc> pageFactories;
};
