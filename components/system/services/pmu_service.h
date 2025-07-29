/*
 * PMU电源管理服务头文件
 * 提供AXP2101电源管理功能
 */

#ifndef PMU_SERVICE_H
#define PMU_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// PMU状态枚举
typedef enum {
    PMU_STATUS_DISCONNECTED = 0,
    PMU_STATUS_CONNECTED,
    PMU_STATUS_ERROR
} pmu_status_t;

// 电池状态枚举
typedef enum {
    BATTERY_STATUS_UNKNOWN = 0,
    BATTERY_STATUS_CHARGING,
    BATTERY_STATUS_DISCHARGING,
    BATTERY_STATUS_FULL,
    BATTERY_STATUS_NOT_PRESENT
} battery_status_t;

// 充电状态枚举
typedef enum {
    CHARGE_STATUS_NOT_CHARGING = 0,
    CHARGE_STATUS_PRECHARGE,
    CHARGE_STATUS_CONSTANT_CURRENT,
    CHARGE_STATUS_CONSTANT_VOLTAGE,
    CHARGE_STATUS_CHARGE_DONE
} charge_status_t;

// PMU数据结构
typedef struct {
    // 电池信息
    uint16_t battery_voltage;      // 电池电压 (mV)
    int16_t battery_current;       // 电池电流 (mA, 正值为充电，负值为放电)
    uint8_t battery_percentage;    // 电池电量百分比 (0-100)
    battery_status_t battery_status;
    
    // USB/VBUS信息
    uint16_t vbus_voltage;         // USB电压 (mV)
    uint16_t vbus_current;         // USB电流 (mA)
    bool vbus_present;             // USB是否插入
    
    // 系统信息
    uint16_t system_voltage;       // 系统电压 (mV)
    int16_t temperature;           // 芯片温度 (°C * 10)
    
    // 电源通道状态
    bool dc1_enabled;              // 外部3.3V
    bool dc3_enabled;              // ESP32核心电压
    bool aldo1_enabled;            // 摄像头数字电源
    bool aldo2_enabled;            // 摄像头模拟电源
    bool aldo3_enabled;            // PIR电源
    bool aldo4_enabled;            // 摄像头AVDD
    bool bldo1_enabled;            // OLED电源
    bool bldo2_enabled;            // MIC电源
    
    // 充电信息
    charge_status_t charge_status;
    uint16_t charge_current;       // 充电电流设置 (mA)
    uint16_t charge_voltage;       // 充电电压设置 (mV)
    
    // 状态标志
    pmu_status_t pmu_status;
    uint32_t timestamp;            // 数据时间戳
} pmu_data_t;

// PMU事件回调函数类型
typedef void (*pmu_data_callback_t)(const pmu_data_t *data);
typedef void (*pmu_event_callback_t)(const char *event, uint32_t value);

/**
 * @brief 初始化PMU服务
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_init(void);

/**
 * @brief 启动PMU数据采集
 * @param interval_ms 数据采集间隔 (毫秒)
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_start(uint32_t interval_ms);

/**
 * @brief 停止PMU数据采集
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_stop(void);

/**
 * @brief 注册PMU数据回调函数
 * @param callback 回调函数指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_register_data_callback(pmu_data_callback_t callback);

/**
 * @brief 注册PMU事件回调函数
 * @param callback 回调函数指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_register_event_callback(pmu_event_callback_t callback);

/**
 * @brief 获取当前PMU数据
 * @param data 数据结构指针
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_get_data(pmu_data_t *data);

/**
 * @brief 获取PMU服务状态
 * @return PMU服务状态
 */
pmu_status_t pmu_service_get_status(void);

/**
 * @brief 设置充电电流
 * @param current_ma 充电电流 (mA)
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_set_charge_current(uint16_t current_ma);

/**
 * @brief 设置充电电压
 * @param voltage_mv 充电电压 (mV)  
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_set_charge_voltage(uint16_t voltage_mv);

/**
 * @brief 控制电源通道
 * @param channel 通道名称
 * @param enable 是否使能
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_set_power_channel(const char *channel, bool enable);

/**
 * @brief 重置PMU服务
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t pmu_service_reset(void);

#ifdef __cplusplus
}
#endif

#endif // PMU_SERVICE_H
