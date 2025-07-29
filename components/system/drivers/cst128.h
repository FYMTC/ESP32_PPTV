#ifndef CST128_H
#define CST128_H

#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pca9554.h"
#include "driver/gpio.h"

#define TP_RST PCA9554_PORT_7
#define TP_INT PCA9554_PORT_6

#define CST128_I2C_ADDR 0x38

#define TOUCH_MAX_POINT_NUMBER 5
#define TOUCH_READ_REG_MAX_SIZE 32

typedef struct {
    uint8_t xh;
    uint8_t xl;
    uint8_t yh;
    uint8_t yl;
    uint8_t resv[2];
} cst128_pos_t;

typedef struct {
    uint8_t finger_num;
    cst128_pos_t pos[TOUCH_MAX_POINT_NUMBER];
} cst128_reg_t;

typedef struct {
    uint16_t x_coordinate;
    uint16_t y_coordinate;
    uint8_t track_id;
    uint8_t width;
    uint8_t event;
    uint32_t timestamp;
} touch_point_t;

typedef struct {
    touch_point_t point[TOUCH_MAX_POINT_NUMBER];
    uint8_t point_num;
} touch_result_t;

typedef struct {
    i2c_port_t i2c_port;
    pca9554_port_t rst_pin; // PCA9554 pin for RST
    pca9554_port_t int_pin; // PCA9554 pin for INT
    int rst_valid;
    uint16_t range_x;
    uint16_t range_y;
    gpio_num_t gpio_rst;    // ESP32 GPIO for RST
    gpio_num_t gpio_int;    // ESP32 GPIO for INT
    bool use_gpio;          // Flag to indicate whether to use GPIO or PCA9554
} cst128_dev_t;

esp_err_t cst128_init(cst128_dev_t *dev);
esp_err_t cst128_read_touch(cst128_dev_t *dev, touch_result_t *result);

#endif // CST128_H