// components/system/drivers/i2c_init.cpp
#include "i2c_init.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "axp2101.hpp"
#include "mpu6050.h"

static const char *I2C_TAG = "I2C";
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO (gpio_num_t)17
#define I2C_MASTER_SCL_IO (gpio_num_t)18
#define I2C_MASTER_FREQ_HZ 100000

extern "C"
{

    void i2c_init()
    {
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master = {.clk_speed = I2C_MASTER_FREQ_HZ},
        };

        ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
        ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
        ESP_LOGI(I2C_TAG, "I2C initialized on SDA:%d, SCL:%d", (int)I2C_MASTER_SDA_IO, (int)I2C_MASTER_SCL_IO);
    }

    i2c_scan_result_t scan_i2c_devices()
    {
        i2c_scan_result_t result = {.count = 0};
        esp_err_t ret;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();

        for (uint8_t addr = 0x08; addr < 0x78 && result.count < MAX_I2C_DEVICES; ++addr)
        {
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);

            ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
            if (ret == ESP_OK)
            {
                const char *device_name = "Unknown";
                // 可以根据地址识别常见设备
                if (addr == MPU6050_ADDR)
                {
                    device_name = "MPU6050";
                    // mpu6050_service_init();
                }
                else if (addr == AXP2101_ADDR)
                {
                    device_name = "AXP2101";
                    pmu_init();
                }
                else if (addr == PCA9554_ADDR)
                {
                    device_name = "PCA9554";
                    // init_external_gpio();
                }
                else if (addr == CST128_ADDR)
                {
                    device_name = "CST128";
                }
                else if (addr == NAU88C22_ADDR)
                {
                    device_name = "NAU88C22";
                }
                else if (addr == QMC5883_ADDR)
                {
                    device_name = "QMC5883";
                }

                result.devices[result.count].address = addr;
                result.devices[result.count].name = device_name;
                result.count++;

                ESP_LOGI(I2C_TAG, "Found I2C device at 0x%02X (%s)", (unsigned int)addr, device_name);
            }

            i2c_cmd_link_delete(cmd);
            cmd = i2c_cmd_link_create();
        }

        i2c_cmd_link_delete(cmd);
        return result;
    }

} // extern "C"