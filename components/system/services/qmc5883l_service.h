/*
 * QMC5883L三轴磁力计传感器服务头文件
 * 提供QMC5883L数据读取和管理功能
 */

#ifndef QMC5883L_SERVICE_H
#define QMC5883L_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "qmc5883l.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * QMC5883L扩展数据结构
 */
typedef struct {
    float mag_x;        // X轴磁场强度 (mG)
    float mag_y;        // Y轴磁场强度 (mG)
    float mag_z;        // Z轴磁场强度 (mG)
    float heading;      // 航向角度 (度，0-360)
    float magnitude;    // 磁场强度大小 (mG)
    bool is_valid;      // 数据是否有效
    uint64_t timestamp; // 时间戳 (毫秒)
} qmc5883l_service_data_t;

/**
 * QMC5883L状态结构
 */
typedef struct {
    bool is_initialized;    // 是否已初始化
    bool is_connected;      // 是否连接正常
    uint32_t error_count;   // 错误计数
    uint64_t last_update;   // 最后更新时间 (毫秒)
} qmc5883l_status_t;

/**
 * QMC5883L数据更新回调函数类型
 */
typedef void (*qmc5883l_data_callback_t)(const qmc5883l_service_data_t* data);

/**
 * @brief 初始化QMC5883L服务
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t qmc5883l_service_init(void);

/**
 * @brief 反初始化QMC5883L服务
 */
void qmc5883l_service_deinit(void);

/**
 * @brief 启动QMC5883L数据采集
 * @param update_interval_ms 数据更新间隔 (毫秒)
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t qmc5883l_service_start(uint32_t update_interval_ms);

/**
 * @brief 停止QMC5883L数据采集
 */
void qmc5883l_service_stop(void);

/**
 * @brief 获取最新的QMC5883L数据
 * @param data 数据输出缓冲区
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t qmc5883l_service_get_data(qmc5883l_service_data_t* data);

/**
 * @brief 获取QMC5883L服务状态
 * @param status 状态输出缓冲区
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t qmc5883l_service_get_status(qmc5883l_status_t* status);

/**
 * @brief 注册数据更新回调函数
 * @param callback 回调函数指针
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t qmc5883l_service_register_callback(qmc5883l_data_callback_t callback);

/**
 * @brief 取消注册数据更新回调函数
 */
void qmc5883l_service_unregister_callback(void);

/**
 * @brief 校准QMC5883L传感器
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t qmc5883l_service_calibrate(void);

/**
 * @brief 复位QMC5883L传感器
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t qmc5883l_service_reset(void);

#ifdef __cplusplus
}
#endif

#endif // QMC5883L_SERVICE_H
