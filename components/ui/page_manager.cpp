// page_manager.cpp
// 页面管理器实现，负责页面注册、跳转、返回、销毁等功能
#include "page_manager.h"


// 注册页面创建函数，name为页面标识，func为页面创建回调
void PageManager::registerPage(const std::string& name, PageCreateFunc func) {
    pageFactories[name] = func;
}



// 跳转到指定页面，自动保留当前页面（无动画）
void PageManager::gotoPage(const std::string& name) {
    if(pageFactories.count(name)) {
        lv_obj_t* page = pageFactories[name]();
        if(!pageStack.empty()) {
            pageStack.top().keep = true;
        }
        pageStack.push({name, page, false});
        lv_screen_load(page);
    }
}

// 跳转到指定页面，带动画
void PageManager::gotoPage(const std::string& name, lv_screen_load_anim_t anim_type, uint32_t time) {
    if(pageFactories.count(name)) {
        lv_obj_t* page = pageFactories[name]();
        if(!pageStack.empty()) {
            pageStack.top().keep = true;
        }
        pageStack.push({name, page, false});
        lv_screen_load_anim(page, anim_type, time, 0, false);
    }
}

// 跳转到指定页面，销毁当前页面（无动画）
void PageManager::gotoPageAndDestroy(const std::string& name) {
    if(pageFactories.count(name)) {
        lv_obj_t* page = pageFactories[name]();
        if(!pageStack.empty()) {
            auto cur = pageStack.top();
            pageStack.pop();
            // 异步安全销毁页面
            lv_async_call([](void* obj){
                lv_obj_t* page = static_cast<lv_obj_t*>(obj);
                lv_timer_t* timer = (lv_timer_t*)lv_obj_get_user_data(page);
                if (timer) lv_timer_del(timer);
                lv_obj_clean(page);
                lv_obj_del(page);
            }, cur.page);
        }
        pageStack.push({name, page, false});
        lv_screen_load(page);
    }
}

// 跳转到指定页面，销毁当前页面（有动画）
void PageManager::gotoPageAndDestroy(const std::string& name, lv_screen_load_anim_t anim_type, uint32_t time) {
    if(pageFactories.count(name)) {
        lv_obj_t* page = pageFactories[name]();
        if(!pageStack.empty()) {
            auto cur = pageStack.top();
            pageStack.pop();
            lv_timer_t* timer = (lv_timer_t*)lv_obj_get_user_data(cur.page);
                if (timer) lv_timer_del(timer);
        }
        pageStack.push({name, page, false});
        lv_screen_load_anim(page, anim_type, time, 0, true);
    }
}


// 返回上一页面，销毁当前页面（如未保留），无动画
void PageManager::back() {
    if(pageStack.size() > 1) {
        auto cur = pageStack.top();
        pageStack.pop();
        if(!pageStack.empty() && pageStack.top().page){
        lv_screen_load(pageStack.top().page);
        if(!cur.keep && cur.page) {
            // 异步安全销毁页面
            lv_async_call([](void* obj){
                lv_obj_t* page = static_cast<lv_obj_t*>(obj);
                // 在此处添加自定义清理逻辑，如移除子控件、解绑事件等
                lv_timer_t* timer = (lv_timer_t*)lv_obj_get_user_data(page);
                if (timer) lv_timer_del(timer); // 删除定时器
                lv_obj_clean(page); // 清理内容
                lv_obj_del(page);  // 删除页面
            }, cur.page);
        }}

    }
}


// 返回上一页面，带动画，增加空指针保护
void PageManager::back(lv_screen_load_anim_t anim_type, uint32_t time) {
    if(pageStack.size() > 1) {
        auto cur = pageStack.top();
        pageStack.pop();

        if(!pageStack.empty() && pageStack.top().page){
            lv_timer_t* timer = (lv_timer_t*)lv_obj_get_user_data(cur.page);
            if (timer) lv_timer_del(timer);
            lv_screen_load_anim(pageStack.top().page, anim_type, time, 0, !cur.keep);
        }
    }
}


// 销毁所有页面，释放资源
void PageManager::clear() {
    while(!pageStack.empty()) {
        auto cur = pageStack.top();
        pageStack.pop();
        if(cur.page) lv_obj_del(cur.page);
    }
}


// 获取当前页面名称
std::string PageManager::currentPage() const {
    return pageStack.empty() ? "" : pageStack.top().name;
}
