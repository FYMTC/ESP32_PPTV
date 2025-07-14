#include "lvgl/lvgl.h"
#include "page_manager.h"
#include "pages_common.h"
#include <iostream>
#include <cstring>
extern PageManager g_pageManager;

// 定义菜单项结构体
typedef struct {
    const char *icon;
    const char *title;
    void (*action)(lv_event_t *e); // 使用函数指针实现多态
} MenuItem;

// 菜单项数组
static const MenuItem menu_items[] = {
    {LV_SYMBOL_SETTINGS, "Settings", [](lv_event_t *e){
        g_pageManager.gotoPage("page_settings", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Settings clicked!"<<std::endl; }},
    {LV_SYMBOL_WIFI, "WIFI", [](lv_event_t *e){
        g_pageManager.gotoPage("page_wifi", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"WIFI clicked!"<<std::endl; }},
    {"\xEF\x80\x97", "TIME", [](lv_event_t *e){
        g_pageManager.gotoPage("page_time", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"TIME clicked!"<<std::endl; }},
    {"\xEF\x84\xA4", "MPU6050", [](lv_event_t *e){
        g_pageManager.gotoPage("page_mpu6050", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"MPU6050 clicked!"<<std::endl; }},
    {"\xEF\x80\x84", "MAX30105", [](lv_event_t *e){
        g_pageManager.gotoPage("page_max30105", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"MAX30105 clicked!"<<std::endl; }},
    {LV_SYMBOL_FILE, "cube game", [](lv_event_t *e){
        g_pageManager.gotoPage("page_cube_game", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Cube Game clicked!"<<std::endl; }},
    {LV_SYMBOL_FILE, "ball game", [](lv_event_t *e){
        g_pageManager.gotoPage("page_ball_game", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Ball Game clicked!"<<std::endl; }},
    {LV_SYMBOL_FILE, "pvz", [](lv_event_t *e){
        g_pageManager.gotoPage("page_pvz", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"PVZ clicked!"<<std::endl; }},
    {LV_SYMBOL_FILE, "fly game", [](lv_event_t *e){
        g_pageManager.gotoPage("page_fly_game", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Fly Game clicked!"<<std::endl; }},
    {LV_SYMBOL_SD_CARD, "SD files", [](lv_event_t *e){
        g_pageManager.gotoPage("page_sd_files", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"SD Files clicked!"<<std::endl; }},
    {LV_SYMBOL_REFRESH, "Restart", [](lv_event_t *e){
        g_pageManager.gotoPage("page_restart", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"Restart clicked!"<<std::endl; }},
    {LV_SYMBOL_POWER, "OFF", [](lv_event_t *e){
        g_pageManager.gotoPage("page_off", LV_SCR_LOAD_ANIM_FADE_OUT, 300);
     std::cout<<"OFF clicked!"<<std::endl; }},
};

// 初始化样式
static void init_styles(lv_style_t *style) {
    lv_style_init(style);
    lv_style_set_text_font(style, &NotoSansSC_Medium_3500);
}

// 通用事件回调函数
static void menu_event_handler(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    const MenuItem *item = (const MenuItem *)lv_event_get_user_data(e);
    if (item && item->action) {
        item->action(e); // 调用对应的函数指针
    }
}

lv_obj_t* createPage_menu(){

	lv_obj_t *main_screen = lv_obj_create(NULL);
    lv_obj_t *list = lv_list_create(main_screen);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));

    static lv_style_t style;
    init_styles(&style);

    for (size_t i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); i++) {
        lv_obj_t *btn = lv_list_add_btn(list, menu_items[i].icon, menu_items[i].title);
        lv_obj_add_event_cb(btn, menu_event_handler, LV_EVENT_CLICKED, (void *)&menu_items[i]);
        lv_obj_add_style(btn, &style, 0);

        // 特殊样式处理
        if (strcmp(menu_items[i].title, "Restart") == 0) {
            lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (strcmp(menu_items[i].title, "OFF") == 0) {
            lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }
	return main_screen;
}
