#include "pca9554.h"
#include "i2cdev.h"

static const char *TAG = "PCA9554";

#define I2C_MASTER_SDA_IO               (gpio_num_t)17
#define I2C_MASTER_SCL_IO               (gpio_num_t)18
#define I2C_MASTER_FREQ_HZ              100000

static i2c_dev_t s_pca9554_dev = {};

static i2c_dev_t *get_pca9554_dev(i2c_port_t i2c_port)
{
    s_pca9554_dev.port = i2c_port;
    s_pca9554_dev.addr = PCA9554_I2C_ADDR;
    s_pca9554_dev.cfg.sda_io_num = I2C_MASTER_SDA_IO;
    s_pca9554_dev.cfg.scl_io_num = I2C_MASTER_SCL_IO;
    s_pca9554_dev.cfg.master.clk_speed = I2C_MASTER_FREQ_HZ;
    s_pca9554_dev.cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    s_pca9554_dev.cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    return &s_pca9554_dev;
}

esp_err_t pca9554_init(i2c_port_t i2c_port) {
    // Initialize PCA9554 (if needed)
    return ESP_OK;
}

esp_err_t pca9554_set_pin_mode(i2c_port_t i2c_port, pca9554_port_t pin, uint8_t mode) {
    uint8_t config_reg = 0x03; // Configuration register
    uint8_t current_config;
    esp_err_t ret;

    i2c_dev_t *dev = get_pca9554_dev(i2c_port);

    // Read current configuration
    ret = i2c_dev_read_reg(dev, config_reg, &current_config, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PCA9554 configuration");
        return ret;
    }

    // Set pin mode
    if (mode == 0) { // Output
        current_config &= ~(1 << pin);
    } else { // Input
        current_config |= (1 << pin);
    }

    // Write new configuration
    ret = i2c_dev_write_reg(dev, config_reg, &current_config, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write PCA9554 configuration");
        return ret;
    }

    return ESP_OK;
}

esp_err_t pca9554_write_pin(i2c_port_t i2c_port, pca9554_port_t pin, uint8_t level) {
    uint8_t output_reg = 0x01; // Output port register
    uint8_t current_output;
    esp_err_t ret;

    i2c_dev_t *dev = get_pca9554_dev(i2c_port);

    // Read current output
    ret = i2c_dev_read_reg(dev, output_reg, &current_output, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PCA9554 output");
        return ret;
    }

    // Set pin level
    if (level == 0) {
        current_output &= ~(1 << pin);
    } else {
        current_output |= (1 << pin);
    }

    // Write new output
    ret = i2c_dev_write_reg(dev, output_reg, &current_output, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write PCA9554 output");
        return ret;
    }

    return ESP_OK;
}

esp_err_t pca9554_read_pin(i2c_port_t i2c_port, pca9554_port_t pin, uint8_t *level) {
    uint8_t input_reg = 0x00; // Input port register
    uint8_t input_value;
    esp_err_t ret;

    i2c_dev_t *dev = get_pca9554_dev(i2c_port);

    // Read input register
    ret = i2c_dev_read_reg(dev, input_reg, &input_value, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PCA9554 input");
        return ret;
    }

    *level = (input_value >> pin) & 0x01;
    return ESP_OK;
}
