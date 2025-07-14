#include "page_manager.hpp"
#include "lvgl.h"
PageManager& PageManager::instance() {
    static PageManager mgr;
    return mgr;
}
PageManager::PageManager() {}
void PageManager::push(Page* page, bool destroyPrev) {
    // 隐藏当前页面或销毁
    if (!pageStack.empty()) {
        if (destroyPrev) {
            Page* prev = pageStack.top();
            pageStack.pop();
            lv_async_call([](void* p){
                static_cast<Page*>(p)->onDestroy();
                delete static_cast<Page*>(p);
            }, prev);
        } else if (pageStack.top()->root) {
            lv_obj_add_flag(pageStack.top()->root, LV_OBJ_FLAG_HIDDEN);
        }
    }
    // 创建新页面
    page->root = page->onCreate();
    lv_obj_clear_flag(page->root, LV_OBJ_FLAG_HIDDEN);
    pageStack.push(page);
}
struct PopContext {
    PageManager* mgr;
    Page* page;
};

void PageManager::pop() {
    if (!pageStack.empty()) {
        Page* curr = pageStack.top();
        pageStack.pop();
        curr->onDestroy();
        delete curr;
        // 显示上一个页面 - 添加安全检查
        if (!pageStack.empty()) {
            Page* prevPage = pageStack.top();
            if (prevPage) {
                    if (!prevPage->root) {
                        prevPage->root = prevPage->onCreate();
                    }
                    if (prevPage->root && lv_obj_is_valid(prevPage->root)) {
                        lv_obj_clear_flag(prevPage->root, LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }

    }
}
void PageManager::replace(Page* page, bool destroyPrev) {
    if (!pageStack.empty()) {
        Page* prev = pageStack.top();
        pageStack.pop();
        lv_async_call([](void* p){
            static_cast<Page*>(p)->onDestroy();
            delete static_cast<Page*>(p);
        }, prev);
    }
    push(page);
}
Page* PageManager::current() {
    if (pageStack.empty()) return nullptr;
    return pageStack.top();
}
