// components/system/drivers/i2c_init.hpp
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "mpu6050.h"
#include "driver/i2c.h"

#define MPU6050_ADDR 0x68  // MPU6050的I2C地址
#define AXP2101_ADDR 0x34  // AXP2101的I2C地址
#define QMC5883_ADDR 0x0D  // QMC5883的I2C地址
#define NAU88C22_ADDR 0x1A // NAU88C22的I2C地址
#define PCA9554_ADDR 0x38  // PCA9554的I2C地址
#define CST128_ADDR 0x2A   // CST128的I2C地址

#define MAX_I2C_DEVICES 20

typedef struct {
    uint8_t address;
    const char *name;
} i2c_device_t;

typedef struct {
    i2c_device_t devices[MAX_I2C_DEVICES];
    int count;
} i2c_scan_result_t;

// 函数声明
void i2c_init(void);

i2c_scan_result_t scan_i2c_devices(void);

#ifdef __cplusplus
}
#endif