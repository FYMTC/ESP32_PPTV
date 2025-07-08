// components/drivers/i2c_init.hpp
#pragma once

#include <vector>
#include <cstdint>
#include <axp2101.hpp>
#include "mpu6050.h"

#define MPU6050_ADDR 0x68  // MPU6050的I2C地址
#define AXP2101_ADDR 0x34  // AXP2101的I2C地址
#define QMC5883_ADDR 0x0D  // QMC5883的I2C地址
#define NAU88C22_ADDR 0x1A // NAU88C22的I2C地址
#define PCA9554_ADDR 0x38  // PCA9554的I2C地址
#define CST128_ADDR 0x2A   // CST128的I2C地址

static mpu6050_handle_t mpu6050 = NULL;

static const char *I2C_TAG = "I2C";
void read_mpu6050_data()
{
    // while (1) {
    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;
    mpu6050_temp_value_t temp;

    if (mpu6050_get_acce(mpu6050, &acce) == ESP_OK)
    {
        ESP_LOGI(I2C_TAG, "Accel: X=%.2f Y=%.2f Z=%.2f", acce.acce_x, acce.acce_y, acce.acce_z);
    }
    else
    {
        ESP_LOGE(I2C_TAG, "Failed to read accelerometer data");
    }

    if (mpu6050_get_gyro(mpu6050, &gyro) == ESP_OK)
    {
        ESP_LOGI(I2C_TAG, "Gyro: X=%.2f Y=%.2f Z=%.2f", gyro.gyro_x, gyro.gyro_y, gyro.gyro_z);
    }
    else
    {
        ESP_LOGE(I2C_TAG, "Failed to read gyroscope data");
    }

    if (mpu6050_get_temp(mpu6050, &temp) == ESP_OK)
    {
        ESP_LOGI(I2C_TAG, "Temp: %.2f°C", temp.temp);
    }
    else
    {
        ESP_LOGE(I2C_TAG, "Failed to read temperature data");
    }

    // vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}
void i2c_sensor_mpu6050_test(i2c_port_t i2c_num)
{
    mpu6050 = mpu6050_create(i2c_num, MPU6050_I2C_ADDRESS);
    if (mpu6050 == NULL)
    {
        ESP_LOGE(I2C_TAG, "MPU6050 create failed");
        return;
    }
    esp_err_t ret = mpu6050_config(mpu6050, ACCE_FS_4G, GYRO_FS_500DPS);
    if (ret != ESP_OK)
    {
        ESP_LOGE(I2C_TAG, "MPU6050 config failed");
        return;
    }
    ret = mpu6050_wake_up(mpu6050);
    if (ret != ESP_OK)
    {
        ESP_LOGE(I2C_TAG, "MPU6050 wake-up failed");
        return;
    }
    ESP_LOGI(I2C_TAG, "MPU6050 initialized successfully");
    read_mpu6050_data();
    mpu6050_sleep(mpu6050);
    mpu6050_delete(mpu6050);
}
struct I2CDevice
{
    uint8_t address;
    const char *name;
};

void i2c_init();
std::vector<I2CDevice> scan_i2c_devices();

// components/drivers/i2c_init.cpp
#include "i2c_init.hpp"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *I2CTAG = "I2C";

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO (gpio_num_t)17
#define I2C_MASTER_SCL_IO (gpio_num_t)18
#define I2C_MASTER_FREQ_HZ 100000
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
    ESP_LOGI(I2CTAG, "I2C initialized on SDA:%d, SCL:%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
}

std::vector<I2CDevice> scan_i2c_devices()
{
    std::vector<I2CDevice> devices;
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    for (uint8_t addr = 0x08; addr < 0x78; ++addr)
    {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS);
        if (ret == ESP_OK)
        {
            const char *device_name = "Unknown";
            // 可以根据地址识别常见设备
            if (addr == MPU6050_ADDR)
            {
                device_name = "MPU6050";
                i2c_sensor_mpu6050_test(I2C_MASTER_NUM);
            }
            else if (addr == AXP2101_ADDR)
            {
                device_name = "AXP2101";
                pmu_init(); // 初始化电源管理芯片
            }
            else if (addr == PCA9554_ADDR)
            {
                device_name = "PCA9554";
                //init_external_gpio();
            }
            else if(addr == CST128_ADDR)
            {
                device_name = "CST128";
                // cst128_dev_t cst128_dev = {
                //     .i2c_port = i2c_num,
                //     .rst_pin = PCA9554_PORT_7,
                //     .int_pin = PCA9554_PORT_6,
                //     .rst_valid = 0,
                //     .range_x = 240,
                //     .range_y = 320,
                // };
                // cst128_init(&cst128_dev);
            }
            else if (addr == NAU88C22_ADDR)
            {
                device_name = "NAU88C22";
                // nau88c22_init(i2c_num);
            }
            else if (addr == QMC5883_ADDR)
            {
                device_name = "QMC5883";
                // qmc5883_init(i2c_num);
            }

            devices.push_back({addr, device_name});
            ESP_LOGI(I2CTAG, "Found I2C device at 0x%02X (%s)", addr, device_name);
        }

        i2c_cmd_link_delete(cmd);
        cmd = i2c_cmd_link_create();
    }

    i2c_cmd_link_delete(cmd);
    return devices;
}