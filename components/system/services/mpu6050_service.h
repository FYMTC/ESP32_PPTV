/*
 * MPU6050传感器服务头文件
 * 提供MPU6050数据读取和管理功能
 */

#ifndef MPU6050_SERVICE_H
#define MPU6050_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// MPU6050数据结构
typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float temperature;
    bool is_valid;
    uint32_t timestamp;
} mpu6050_data_t;

// MPU6050状态结构
typedef struct {
    bool is_initialized;
    bool is_connected;
    uint32_t error_count;
    uint32_t last_update;
} mpu6050_status_t;

// 数据更新回调函数类型
typedef void (*mpu6050_data_callback_t)(const mpu6050_data_t* data);

/**
 * @brief 初始化MPU6050服务
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t mpu6050_service_init(void);

/**
 * @brief 反初始化MPU6050服务
 */
void mpu6050_service_deinit(void);

/**
 * @brief 启动MPU6050数据采集
 * @param update_interval_ms 数据更新间隔（毫秒）
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t mpu6050_service_start(uint32_t update_interval_ms);

/**
 * @brief 停止MPU6050数据采集
 */
void mpu6050_service_stop(void);

/**
 * @brief 获取最新的MPU6050数据
 * @param data 输出数据结构指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t mpu6050_service_get_data(mpu6050_data_t* data);

/**
 * @brief 获取MPU6050服务状态
 * @param status 输出状态结构指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t mpu6050_service_get_status(mpu6050_status_t* status);

/**
 * @brief 注册数据更新回调函数
 * @param callback 回调函数指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t mpu6050_service_register_callback(mpu6050_data_callback_t callback);

/**
 * @brief 取消注册数据更新回调函数
 */
void mpu6050_service_unregister_callback(void);

/**
 * @brief 校准MPU6050传感器
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t mpu6050_service_calibrate(void);

/**
 * @brief 复位MPU6050传感器
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t mpu6050_service_reset(void);

#ifdef __cplusplus
}
#endif

#endif // MPU6050_SERVICE_H
