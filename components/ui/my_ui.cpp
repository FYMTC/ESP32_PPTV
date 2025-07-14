
// my_ui.cpp
// LVGL 页面示例，依赖 PageManager
#include "my_ui.h"
#include "lvgl/lvgl.h"

#include "page_manager.h"
#include "pages_common.h"


// 全局页面管理器
PageManager g_pageManager;


extern "C" void my_ui_init(void)
{
    // 注册页面（直接绑定事件）
    g_pageManager.registerPage("pre_page", createPage_prepage);
    g_pageManager.registerPage("page_menu", createPage_menu);
    g_pageManager.registerPage("page_settings", createPage_settings);
    g_pageManager.registerPage("page_mpu6050", createPage_mpu6050);
    g_pageManager.registerPage("page1", createPage1);
    g_pageManager.registerPage("page2", createPage2);
    // 启动时加载页面1
    g_pageManager.gotoPage("pre_page");
}
