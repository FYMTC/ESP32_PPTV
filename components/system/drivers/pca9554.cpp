#include "pca9554.h"

static const char *TAG = "PCA9554";

esp_err_t pca9554_init(i2c_port_t i2c_port) {
    // Initialize PCA9554 (if needed)
    return ESP_OK;
}

esp_err_t pca9554_set_pin_mode(i2c_port_t i2c_port, pca9554_port_t pin, uint8_t mode) {
    uint8_t config_reg = 0x03; // Configuration register
    uint8_t current_config;
    esp_err_t ret;

    // Read current configuration
    ret = i2c_master_write_read_device(i2c_port, PCA9554_I2C_ADDR, &config_reg, 1, &current_config, 1, 1000 / portTICK_PERIOD_MS);
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
    uint8_t write_buf[2] = {config_reg, current_config};
    ret = i2c_master_write_to_device(i2c_port, PCA9554_I2C_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
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

    // Read current output
    ret = i2c_master_write_read_device(i2c_port, PCA9554_I2C_ADDR, &output_reg, 1, &current_output, 1, 1000 / portTICK_PERIOD_MS);
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
    uint8_t write_buf[2] = {output_reg, current_output};
    ret = i2c_master_write_to_device(i2c_port, PCA9554_I2C_ADDR, write_buf, sizeof(write_buf), 1000 / portTICK_PERIOD_MS);
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

    // Read input register
    ret = i2c_master_write_read_device(i2c_port, PCA9554_I2C_ADDR, &input_reg, 1, &input_value, 1, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read PCA9554 input");
        return ret;
    }

    *level = (input_value >> pin) & 0x01;
    return ESP_OK;
}