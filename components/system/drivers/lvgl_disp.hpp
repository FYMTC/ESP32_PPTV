#if 0

#include "lvgl.h"
#include "conf.h"
//#include "bt.hpp"
#include "driver/gpio.h"
#include "cst128.h"
#include "LGFX_disp.hpp"
// 编码器引脚定义
#define ENCODER_A_PIN GPIO_NUM_16 // A 相连接到 GPIO16
#define ENCODER_B_PIN GPIO_NUM_2  // B 相连接到 GPIO2
#define ENCODER_BT_PIN GPIO_NUM_3 // 按钮连接到 GPIO3
// 去抖动时间（单位：毫秒）
#define DEBOUNCE_TIME_MS 100
// 编码器状态变量
static volatile int16_t encoder_count = 0;
static volatile bool button_state = false;
// 去抖动相关变量
static uint32_t last_a_change_time = 0;
static uint32_t last_b_change_time = 0;
static uint32_t last_bt_change_time = 0;
static uint8_t last_a_state = 0;
static uint8_t last_b_state = 0;
static uint8_t last_bt_state = 0;

static const char *LV_TAG = "lvgl_port";
/* Display */
LGFX_tft tft;
static void disp_init(void);
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

mouse_t bt_mouse_indev;
uint8_t notifyCallback_statue = 0;
/* Touch pad */
cst128_dev_t dev = {
    .i2c_port = I2C_NUM_0,
    .rst_pin = PCA9554_PORT_6, // RST connected to PCA9554 P6
    .int_pin = PCA9554_PORT_7, // INT connected to PCA9554 P7
    .rst_valid = 0,
    .range_x = MY_DISP_HOR_RES,
    .range_y = MY_DISP_VER_RES,
    .gpio_rst = GPIO_NUM_4, // 使用GPIO4作为RST
    .gpio_int = GPIO_NUM_5, // 使用GPIO5作为INT
    .use_gpio = true        // 使用ESP32的GPIO
};
static void touchpad_init(void);
bool cst128_init_flag = false;
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
lv_indev_t *indev_touchpad;
lv_indev_t *indev_mouse;
lv_indev_t *indev_keypad;
lv_indev_t *indev_encoder;
lv_indev_t *indev_button;
lv_indev_t *encoder_indev;
void encoder_read_task(void *pvParameter);

static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t *x, lv_coord_t *y);

static void mouse_init(void);
static void mouse_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static bool mouse_is_pressed(void);
static void mouse_get_xy(lv_coord_t *x, lv_coord_t *y);

static void keypad_init(void);
static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static uint32_t keypad_get_key(void);

static void encoder_init(void);
static void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static void encoder_handler(void);

static void button_init(void);
static void button_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static int8_t button_get_pressed_id(void);
static bool button_is_pressed(uint8_t id);
/***********************************************************/
/**********************    lvgl ↓   *********************/
void lv_port_indev_init(void)
{
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

/*------------------
 * Touchpad
 * -----------------*/
#if USE_TOUCHPAD
    static lv_indev_drv_t touchpad_indev_drv;
    /*Initialize your touchpad if you have*/
    touchpad_init();
    if (cst128_init_flag)
    { /*Register a touchpad input device*/
        lv_indev_drv_init(&touchpad_indev_drv);
        touchpad_indev_drv.type = LV_INDEV_TYPE_POINTER;
        touchpad_indev_drv.read_cb = touchpad_read;
        indev_touchpad = lv_indev_drv_register(&touchpad_indev_drv);
    }
#endif

/*------------------
 * Mouse
 * -----------------*/

/*Initialize your mouse if you have*/
#if USE_MOUSE
    static lv_indev_drv_t mouse_indev_drv;
    mouse_init();

    /*Register a mouse input device*/
    lv_indev_drv_init(&mouse_indev_drv);
    mouse_indev_drv.type = LV_INDEV_TYPE_POINTER;
    mouse_indev_drv.read_cb = mouse_read;
    indev_mouse = lv_indev_drv_register(&mouse_indev_drv);

    /*Set cursor. For simplicity set a HOME symbol now.*/
    lv_obj_t *mouse_cursor = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(mouse_cursor, &NotoSansSC_Medium_3500, 0);
    lv_img_set_src(mouse_cursor, MY_FONT_MOUSE); // 鼠标字体符号
    lv_indev_set_cursor(indev_mouse, mouse_cursor);
#endif

    // /*------------------
    //  * Encoder
    //  * -----------------*/
#if USE_ENCODER
    // /*Initialize your encoder if you have*/
    encoder_init();

    // /*Register a encoder input device*/
    static lv_indev_drv_t indev_drv_encoder;
    lv_indev_drv_init(&indev_drv_encoder);
    indev_drv_encoder.type = LV_INDEV_TYPE_ENCODER;
    indev_drv_encoder.read_cb = encoder_read;
    encoder_indev = lv_indev_drv_register(&indev_drv_encoder);
    // 创建任务读取编码器状态
    xTaskCreate(encoder_read_task, "encoder_read_task", 2048, NULL, 1, NULL);

    // /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
    //  *add objects to the group with `lv_group_add_obj(group, obj)`
    //  *and assign this input device to group to navigate in it:
    //  *`lv_indev_set_group(indev_encoder, group);`*/
#endif
    /*------------------
     * Button
     * -----------------*/

    // /*Initialize your button if you have*/
    // button_init();

    // /*Register a button input device*/
    // 初始化按钮输入设备
    // static lv_indev_drv_t indev_drv_button;
    // lv_indev_drv_init(&indev_drv_button);
    // indev_drv_button.type = LV_INDEV_TYPE_BUTTON;
    // indev_drv_button.read_cb = button_read;
    // lv_indev_t *button_indev = lv_indev_drv_register(&indev_drv_button);

    // /*Assign buttons to points on the screen*/
    // static const lv_point_t btn_points[2] = {
    //     {10, 10},   /*Button 0 -> x:10; y:10*/
    //     {40, 100},  /*Button 1 -> x:40; y:100*/
    // };
    // lv_indev_set_button_points(indev_button, btn_points);

    // 将按钮与编码器输入设备关联
    // static lv_point_t points_array[] = {{0, 0}};
    // lv_indev_set_button_points(button_indev, points_array);
}

/*------------------
 * 触摸设备
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    esp_err_t ret = cst128_init(&dev);
    if (ret != ESP_OK)
    {
        ESP_LOGE("MAIN", "Failed to initialize CST128");
        return;
    }
    cst128_init_flag = true;
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    touch_result_t result;
    esp_err_t ret = cst128_read_touch(&dev, &result);
    if (ret == ESP_OK)
    {
        // for (int i = 0; i < result.point_num; i++) {
        //     ESP_LOGI("MAIN", "Touch %d: X=%d, Y=%d", i, result.point[i].x_coordinate, result.point[i].y_coordinate);
        // }
        if (result.point_num > 0)
        {
            data->point.x = MY_DISP_HOR_RES - result.point[0].x_coordinate;
            data->point.y = result.point[0].y_coordinate;
            // ESP_LOGI("lv_touch_pont","X=%d, Y=%d",data->point.x, data->point.y);
            data->state = LV_INDEV_STATE_PR;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    // data->point.x = last_x;
    // data->point.y = last_y;
}
/*------------------
 * Mouse
 * -----------------*/

/*Initialize your mouse*/
static void mouse_init(void)
{
    /*Your code comes here*/
    bt_mouse_indev.left_button_pressed = false;
    bt_mouse_indev.right_button_pressed = false;
    bt_mouse_indev.x_movement = 0;
    bt_mouse_indev.y_movement = 0;
    bt_mouse_indev.data_frame = 0;
}

/*Will be called by the library to read the mouse*/
static void mouse_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    /*Get the current x and y coordinates*/
    mouse_get_xy(&data->point.x, &data->point.y);

    /*Get whether the mouse button is pressed or released*/
    if (mouse_is_pressed())
    {
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

/*Return true is the mouse button is pressed*/
static bool mouse_is_pressed(void)
{
    /*Your code comes here*/

    return bt_mouse_indev.left_button_pressed;
}

/*Get the x and y coordinates if the mouse is pressed*/
static void mouse_get_xy(lv_coord_t *x, lv_coord_t *y)
{
    /*Your code comes here*/
    static lv_point_t last_pos = {0, 0}; // 保存鼠标的最后位置
    if (notifyCallback_statue != bt_mouse_indev.data_frame)
    { // 说明鼠标数据有更新
        last_pos.x += bt_mouse_indev.x_movement;
        last_pos.y += bt_mouse_indev.y_movement;
        if (last_pos.y > MY_DISP_VER_RES)
        {
            last_pos.y = MY_DISP_VER_RES;
        }
        if (last_pos.x > MY_DISP_HOR_RES)
        {
            last_pos.x = MY_DISP_HOR_RES;
        }
        if (last_pos.x < 0)
        {
            last_pos.x = 0;
        }
        if (last_pos.y < 0)
        {
            last_pos.y = 0;
        }
        (*x) = last_pos.x;
        (*y) = last_pos.y;
        notifyCallback_statue = bt_mouse_indev.data_frame;
    }
}
/*------------------
 * Encoder
 * -----------------*/
// 初始化编码器
void encoder_init()
{
    // 配置GPIO引脚为输入模式
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ENCODER_A_PIN) | (1ULL << ENCODER_B_PIN) | (1ULL << ENCODER_BT_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // 初始化编码器计数
    encoder_count = 0;
    button_state = false;

    // 初始化去抖动状态
    last_a_state = gpio_get_level(ENCODER_A_PIN);
    last_b_state = gpio_get_level(ENCODER_B_PIN);
    last_bt_state = gpio_get_level(ENCODER_BT_PIN);
}
// 读取编码器状态（带去抖动）
void encoder_read_task(void *pvParameter)
{
    float i = 0;
    while (1)
    {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 读取A相信号并去抖动
        uint8_t a_state = gpio_get_level(ENCODER_A_PIN);
        if (a_state != last_a_state)
        {
            if ((current_time - last_a_change_time) > DEBOUNCE_TIME_MS)
            {
                last_a_state = a_state;
                last_a_change_time = current_time;
            }
        }

        // 读取B相信号并去抖动
        uint8_t b_state = gpio_get_level(ENCODER_B_PIN);
        if (b_state != last_b_state)
        {
            if ((current_time - last_b_change_time) > DEBOUNCE_TIME_MS)
            {
                last_b_state = b_state;
                last_b_change_time = current_time;
            }
        }

        // 读取按钮信号并去抖动
        uint8_t bt_state = gpio_get_level(ENCODER_BT_PIN);
        if (bt_state != last_bt_state)
        {
            if ((current_time - last_bt_change_time) > DEBOUNCE_TIME_MS)
            {
                last_bt_state = bt_state;
                last_bt_change_time = current_time;
                button_state = (bt_state == 0); // 按钮按下时为低电平
            }
        }

        // 更新编码器计数
        static uint8_t last_encoder_state = 0;
        uint8_t current_encoder_state = (last_a_state << 1) | last_b_state;
        static uint32_t last_encoder_change_time = 0;

        if (current_encoder_state != last_encoder_state)
        {
            if ((current_time - last_encoder_change_time) > 20)
            { // 20ms 间隔
                if ((last_encoder_state == 0b00 && current_encoder_state == 0b01) ||
                    (last_encoder_state == 0b01 && current_encoder_state == 0b11) ||
                    (last_encoder_state == 0b11 && current_encoder_state == 0b10) ||
                    (last_encoder_state == 0b10 && current_encoder_state == 0b00))
                {
                    i += 0.5;
                    if (i >= 1)
                    {
                        i = 0;
                        encoder_count++;
                    }
                }
                else
                {
                    i -= 0.5;
                    if (i <= -1)
                    {
                        i = 0;
                        encoder_count--;
                    }
                }
                last_encoder_state = current_encoder_state;
                last_encoder_change_time = current_time;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // 5ms 延时
    }
}
// LVGL输入设备回调函数
static void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static int16_t last_encoder_count = 0;
    static bool last_button_state = false;

    data->enc_diff = encoder_count - last_encoder_count;
    last_encoder_count = encoder_count;

    data->state = button_state ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    if (button_state != last_button_state)
    {
        last_button_state = button_state;
    }
}
/*Call this function in an interrupt to process encoder events (turn, press)*/

/*------------------
 * Button
 * -----------------*/

/*Initialize your buttons*/
static void button_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the button*/
static void button_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
}

/*Get ID  (0, 1, 2 ..) of the pressed button*/
static int8_t button_get_pressed_id(void)
{
    uint8_t i;

    /*Check to buttons see which is being pressed (assume there are 2 buttons)*/
    for (i = 0; i < 2; i++)
    {
        /*Return the pressed button's ID*/
        if (button_is_pressed(i))
        {
            return i;
        }
    }

    /*No button pressed*/
    return -1;
}

/*Test if `id` button is pressed or not*/
static bool button_is_pressed(uint8_t id)
{

    /*Your code comes here*/

    return false;
}

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/

    /**
     * LVGL requires a buffer where it internally draws the widgets.
     * Later this buffer will passed to your display driver's `flush_cb` to copy its content to your display.
     * The buffer has to be greater than 1 display row
     *
     * There are 3 buffering configurations:
     * 1. Create ONE buffer:
     *      LVGL will draw the display's content here and writes it to your display
     *
     * 2. Create TWO buffer:
     *      LVGL will draw the display's content to a buffer and writes it your display.
     *      You should use DMA to write the buffer's content to the display.
     *      It will enable LVGL to draw the next part of the screen to the other buffer while
     *      the data is being sent form the first buffer. It makes rendering and flushing parallel.
     *
     * 3. Double buffering
     *      Set 2 screens sized buffers and set disp_drv.full_refresh = 1.
     *      This way LVGL will always provide the whole rendered screen in `flush_cb`
     *      and you only need to change the frame buffer's address.
     */

    /* Example for 1) */
    // static lv_disp_draw_buf_t draw_buf_dsc_1;
    // static lv_color_t buf_1[MY_DISP_HOR_RES * 120];                          /*A buffer for 10 rows*/
    // lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * MY_DISP_VER_RES);   /*Initialize the display buffer*/

#if use_buf_dsc_2

    /* Example for 2) */
    static lv_disp_draw_buf_t draw_buf_dsc_2;
    static lv_color_t buf_2_1[MY_DISP_HOR_RES * 24];                                /*A buffer for 10 rows*/
    static lv_color_t buf_2_2[MY_DISP_HOR_RES * 24];                                /*An other buffer for 10 rows*/
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 24); /*Initialize the display buffer*/

/* Example for 3) also set disp_drv.full_refresh = 1 below*/
#endif

#if use_buf_dsc_3
    static lv_disp_draw_buf_t draw_buf_dsc_3;
#if USE_PSRAM_FOR_BUFFER                                                                                                                                      /* Try to get buffer from PSRAM */
    static lv_color_t *buf_3_1 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); /*A screen sized buffer*/
    static lv_color_t *buf_3_2 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); /*Another screen sized buffer*/

    if ((buf_3_1 == NULL) || (buf_3_2 == NULL))
    {
        ESP_LOGE(LV_TAG, "malloc buffer from PSRAM fialed");
        while (1)
            ;
    }
    else
    {
        ESP_LOGI(LV_TAG, "malloc buffer from PSRAM successful");
    }
    lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,
                          MY_DISP_HOR_RES * MY_DISP_VER_RES); /*Initialize the display buffer*/
#else
    static lv_color_t *buf_3_1 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t) / 7, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    static lv_color_t *buf_3_2 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t) / 7, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    // static lv_color_t *buf_3_1 = (lv_color_t *)malloc(MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t)/2);
    // static lv_color_t *buf_3_2 = (lv_color_t *)malloc(MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t)/2);

    lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,
                          MY_DISP_HOR_RES * MY_DISP_VER_RES / 7); /*Initialize the display buffer*/
#endif
#endif
    // static lv_disp_draw_buf_t draw_buf_dsc_2;
    // static lv_color_t buf_2_1[MY_DISP_HOR_RES * 24];                        /*A buffer for 10 rows*/
    // static lv_color_t buf_2_2[MY_DISP_HOR_RES * 24];                        /*An other buffer for 10 rows*/
    // lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 24);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv; /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);   /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = MY_DISP_HOR_RES; // width
    disp_drv.ver_res = MY_DISP_VER_RES; // height

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

/*Set a display buffer*/
// disp_drv.draw_buf = &draw_buf_dsc_1;
#if use_buf_dsc_2
    disp_drv.draw_buf = &draw_buf_dsc_2;
#endif
#if use_buf_dsc_3
    disp_drv.draw_buf = &draw_buf_dsc_3;
#endif
    /*Required for Example 3)*/

    disp_drv.full_refresh = 0; // 全屏幕刷新
    /* Set LVGL software rotation */
    // disp_drv.sw_rotate = 1;//软件旋转屏幕
    //  disp_drv.rotated = LV_DISP_ROT_90;

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    // disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
    ESP_LOGI(LV_TAG, "register the driver");
}

static void disp_init(void)
{
#if USE_LGFX == 1
    ESP_LOGI(LV_TAG, "[LVGL] LGFX lcd init...");
    tft.init();
    // tft.setRotation(1);
    tft.setBrightness(get_brightness_from_nvs());
    // tft.fillScreen(TFT_RED);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // tft.fillScreen(TFT_GREEN);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // tft.fillScreen(TFT_BLUE);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    tft.fillScreen(TFT_BLACK);
#endif

#if USE_eTFT == 1
    Serial.println("[LVGL] eTFT lcd init...");
    tft = TFT_eSPI();
    tft.init();
    // tft.setRotation(2);
    tft.fillScreen(0xAD75);
#endif
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

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    // if(disp_flush_enabled) {
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

    // int32_t x;
    // int32_t y;
    // for(y = area->y1; y <= area->y2; y++) {
    //     for(x = area->x1; x <= area->x2; x++) {
    //         /*Put a pixel to the display. For example:*/
    //         /*put_px(x, y, *color_p)*/
    //         color_p++;
    //     }
    // }

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if USE_LGFX == 1
    if (tft.getStartCount() == 0)
    {
        tft.startWrite();

        tft.setAddrWindow(area->x1, area->y1, w, h);
        tft.pushPixelsDMA((uint16_t *)&color_p->full, w * h, true);
        // tft.pushColors((uint16_t *)&color_p->full, w * h, true);  // 非 DMA 传输
        // tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
        //  tft.waitDMA();
        tft.endWrite();
    }

#endif

#if USE_eTFT == 1
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)&color_p->full, w * h);
    tft.endWrite();
#endif

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
}

/**********************    lvgl ↑  *********************/
/**********************************************************/


#endif /*Disable/Enable content*/   