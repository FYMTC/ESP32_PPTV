#pragma once
#include <vector>
#include <cstdint>
#include <esp_log.h>
#include <driver/i2c.h>

class I2CScanner {
public:
    I2CScanner(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint32_t freq = 100000)
        : port_(port), sda_(sda), scl_(scl), freq_(freq) {}

    void init() {
        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = sda_;
        conf.scl_io_num = scl_;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = freq_;
        i2c_param_config(port_, &conf);
        i2c_driver_install(port_, conf.mode, 0, 0, 0);
    }

    std::vector<uint8_t> scan() {
        std::vector<uint8_t> found;
        ESP_LOGI("I2C_SCAN", "Scanning I2C bus...");
        for (uint8_t addr = 1; addr < 127; ++addr) {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(port_, cmd, 50 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                ESP_LOGI("I2C_SCAN", "Found device at 0x%02X", addr);
                found.push_back(addr);
            }
        }
        return found;
    }

private:
    i2c_port_t port_;
    gpio_num_t sda_;
    gpio_num_t scl_;
    uint32_t freq_;
};