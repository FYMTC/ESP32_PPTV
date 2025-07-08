#pragma once
//竖屏
#define MY_DISP_HOR_RES 240 // width
#define MY_DISP_VER_RES 320 // height
#define OFFSET_ROTATION 0
//横屏
// #define MY_DISP_HOR_RES 320 // width
// #define MY_DISP_VER_RES 240 // height
// #define OFFSET_ROTATION 1

#define USE_LGFX 1
#define USE_eTFT 0

#define USE_PSRAM_FOR_BUFFER 1
#define use_buf_dsc_1 0 
#define use_buf_dsc_2 0 // fps
#define use_buf_dsc_3 1 // fps23
#define cfgspi_mode 3
#define disp_sclk 13
#define disp_mosi 15
#define disp_dc 14
#define disp_rst 11
#define disp_cs 12
#define disp_offset_y 0
#define DISP_HOR_RES 240
#define DISP_VER_RES 320
#define DISP_bl_pin 21

#define cfgreadable false;
#define cfginvert true;
#define cfgrgb_order false;
#define cfgdlen_16bit false;
#define cfgbus_shared false;

#define USE_TOUCHPAD 1
#define USE_MOUSE 0
#define USE_ENCODER 1

#define BRIGHTNESS_KEY "brightness"
#define WIFI_STATUS_KEY "WIFI_STATUS"
#define MY_FONT_MOUSE  "\xEF\x89\x85"
#define MY_FONT_TIME  "\xEF\x8B\xB2"

#define TOUCH_PAD_NUM GPIO_NUM_14

#define LED_STRIP_GPIO_PIN  48
#define LED_STRIP_LED_COUNT 1

//#define USE_I2S_AUDIO 0
#define USE_UAC_AUDIO 1
