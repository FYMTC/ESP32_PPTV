#ifndef PCA9554_H
#define PCA9554_H

#include "driver/i2c.h"
#include "esp_log.h"

#define PCA9554_I2C_ADDR 0x38

typedef enum {
    PCA9554_PORT_NC = -1,
    PCA9554_PORT_0 = 0,
    PCA9554_PORT_1,
    PCA9554_PORT_2,
    PCA9554_PORT_3,
    PCA9554_PORT_4,
    PCA9554_PORT_5,
    PCA9554_PORT_6,
    PCA9554_PORT_7
} pca9554_port_t;

esp_err_t pca9554_init(i2c_port_t i2c_port);
esp_err_t pca9554_set_pin_mode(i2c_port_t i2c_port, pca9554_port_t pin, uint8_t mode);
esp_err_t pca9554_write_pin(i2c_port_t i2c_port, pca9554_port_t pin, uint8_t level);
esp_err_t pca9554_read_pin(i2c_port_t i2c_port, pca9554_port_t pin, uint8_t *level);

#endif // PCA9554_H