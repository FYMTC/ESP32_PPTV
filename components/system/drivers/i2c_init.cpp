// components/system/drivers/i2c_init.cpp
#include "i2c_init.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "axp2101.hpp"
#include "mpu6050.h"
#include "i2cdev.h"

static const char *I2C_TAG = "I2C";
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO (gpio_num_t)17
#define I2C_MASTER_SCL_IO (gpio_num_t)18
#define I2C_MASTER_FREQ_HZ 100000

extern "C"
{

    void i2c_init()
    {
        // 使用i2cdev库的线程安全初始化
        ESP_ERROR_CHECK(i2cdev_init());
        ESP_LOGI(I2C_TAG, "I2C initialized using i2cdev library on SDA:%d, SCL:%d", 
                 (int)I2C_MASTER_SDA_IO, (int)I2C_MASTER_SCL_IO);
    }

    i2c_scan_result_t scan_i2c_devices()
    {
        i2c_scan_result_t result = {};
        
        // 创建I2C设备描述符用于扫描
        i2c_dev_t scan_dev = {};
        scan_dev.port = I2C_MASTER_NUM;
        scan_dev.addr = 0; // 初始化地址，稍后在循环中设置
        scan_dev.cfg.sda_io_num = I2C_MASTER_SDA_IO;
        scan_dev.cfg.scl_io_num = I2C_MASTER_SCL_IO;
        scan_dev.cfg.master.clk_speed = I2C_MASTER_FREQ_HZ;
        scan_dev.cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
        scan_dev.cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;

        for (uint8_t addr = 0x08; addr < 0x78 && result.count < MAX_I2C_DEVICES; ++addr)
        {
            scan_dev.addr = addr;
            esp_err_t ret = i2c_dev_probe(&scan_dev, I2C_DEV_WRITE);
            
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
                    device_name = "QMC5883L";
                    // qmc5883l_service_init();
                }

                result.devices[result.count].address = addr;
                result.devices[result.count].name = device_name;
                result.count++;

                ESP_LOGI(I2C_TAG, "Found I2C device at 0x%02X (%s)", (unsigned int)addr, device_name);
            }
        }

        return result;
    }

} // extern "C"