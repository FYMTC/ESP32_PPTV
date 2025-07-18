/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <cstdio>
#include "esp_heap_caps.h"
#include "conf.h"
#include "LGFX_disp.hpp"
#include "lcd_brightness.hpp"
#include "esp_log.h"
/*********************
 *      DEFINES
 *********************/

#ifndef MY_DISP_HOR_RES
#warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
#define MY_DISP_HOR_RES 320
#endif

#ifndef MY_DISP_VER_RES
#warning Please define or replace the macro MY_DISP_VER_RES with the actual screen height, default value 240 is used for now.
#define MY_DISP_VER_RES 240
#endif

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

static const char *LV_TAG = "lvgl_port";
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t *disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    /* Example 1
     * One buffer for partial rendering*/
    #if use_buf_dsc_1
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_1_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];            /*A buffer for 10 rows*/
    lv_display_set_buffers(disp, buf_1_1, NULL, sizeof(buf_1_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    #endif
    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    #if use_buf_dsc_2
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_2[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    #endif
    /* Example 3
     * Two buffers screen sized buffer for double buffering.
     * Both LV_DISPLAY_RENDER_MODE_DIRECT and LV_DISPLAY_RENDER_MODE_FULL works, see their comments*/
    #if use_buf_dsc_3
     printf("Display buffer malloc\n");

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t *buf_3_1 = (uint8_t *)heap_caps_malloc(MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t *buf_3_2 = (uint8_t *)heap_caps_malloc(MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf_3_1 || !buf_3_2)
    {
        // 处理分配失败
        printf("Display buffer malloc failed!\n");
        abort();
    }
    else
    {
        printf("Display buffer malloc success!\n");
    }
    lv_display_set_buffers(disp, buf_3_1, buf_3_2, MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL, LV_DISPLAY_RENDER_MODE_PARTIAL);
    #endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    ESP_LOGI(LV_TAG, "[LVGL] LGFX lcd init...");
    auto tft = get_lgfx_tft();
    tft->init();
    // tft->setRotation(1);
    // tft->setBrightness(255);
    // tft->fillScreen(TFT_RED);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // tft->fillScreen(TFT_GREEN);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // tft->fillScreen(TFT_BLUE);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // tft->fillScreen(TFT_BLACK);

    xTaskCreate(brightness_task, "brightness_task", 1048, NULL, 10, NULL);
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    auto tft = get_lgfx_tft();
    if (disp_flush_enabled)
    {
        /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

        // int32_t x;
        // int32_t y;
        // for (y = area->y1; y <= area->y2; y++)
        // {
        //     for (x = area->x1; x <= area->x2; x++)
        //     {
        //         /*Put a pixel to the display. For example:*/
        //         /*put_px(x, y, *px_map)*/
        //         px_map++;
        //     }
        // }
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);
        if (tft->getStartCount() == 0)
        {
            tft->startWrite();

            tft->setAddrWindow(area->x1, area->y1, w, h);
            tft->pushPixelsDMA((uint16_t *)px_map, w * h, true);
            //tft->pushColors((uint16_t *)px_map, w * h, true); // 非 DMA 传输
            tft->waitDMA();
            tft->endWrite();
        }
    }

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_display_flush_ready(disp_drv);
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
