#pragma once
#include "lvgl/lvgl.h"

#define MY_SYMBOL_CONNECTED "\xEF\x83\x81"
#define MY_SYMBOL_DISCONNECTED "\xEF\x84\xA7"
#define MY_SYMBOL_TOOLS "\xEF\x9F\x99"
#define MY_SYMBOL_SLIDERS "\xEF\x87\x9E"
#define MY_SYMBOL_MOUSE "\xEF\x89\x85"
#define MY_SYMBOL_FILES "\xEF\x81\xBC"
#define MY_SYMBOL_TIME "\xEF\x80\x97"
#define MY_SYMBOL_CHIP "\xEF\x8B\x9B"
#define MY_SYMBOL_GPS "\xEF\x84\xA4"
#define MY_SYMBOL_COMPASS "\xEF\x85\x8E"
#define MY_SYMBOL_HEART "\xEF\x88\x9E"


lv_obj_t* createPage1();
lv_obj_t* createPage2();
lv_obj_t* createPage_settings();
lv_obj_t* createPage_menu();
lv_obj_t* createPage_mpu6050();
lv_obj_t* createPage_qmc5883l();
lv_obj_t* createPage_prepage();
lv_obj_t* createPage_time();
lv_obj_t* createPage_wifi();
lv_obj_t* createPage_pmu();
lv_obj_t* createPage_sd_files();
