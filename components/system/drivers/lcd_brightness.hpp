#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// 亮度控制模式
typedef enum {
    BRIGHTNESS_MODE_AUTO = 0,    // 自动模式（根据环境光调节）
    BRIGHTNESS_MODE_MANUAL       // 手动模式（固定亮度）
} brightness_mode_t;

// 亮度配置结构体
typedef struct {
    brightness_mode_t mode;         // 当前模式
    uint8_t manual_brightness;      // 手动模式下的亮度值 (0-255)
    uint8_t min_brightness;         // 最小亮度限制 (0-255, 默认12)
    uint8_t max_brightness;         // 最大亮度限制 (0-255, 默认255)
    uint16_t light_threshold_low;   // 自动模式低光阈值 (默认100)
    uint16_t light_threshold_high;  // 自动模式高光阈值 (默认3000)
    uint16_t auto_adjust_interval;  // 自动调节间隔ms (默认200)
    bool smooth_transition;         // 是否启用平滑过渡 (默认true)
    
    // 自动息屏功能配置
    bool auto_sleep_enabled;        // 是否启用自动息屏 (默认true)
    uint32_t sleep_timeout_ms;      // 息屏超时时间 (默认30000ms = 30秒)
    uint32_t sleep_delay_ms;        // 降到最低亮度后的延迟时间 (默认3000ms = 3秒)
    uint8_t sleep_brightness;       // 息屏时的最低亮度 (默认1)
} brightness_config_t;

// 亮度状态回调函数类型
typedef void (*brightness_status_callback_t)(uint8_t current_brightness, brightness_mode_t mode);

// 创建亮度控制任务
void create_brightness_task(void);

// 设置亮度控制模式
void set_brightness_mode(brightness_mode_t mode);

// 获取当前亮度控制模式
brightness_mode_t get_brightness_mode(void);

// 设置手动亮度值 (会自动切换到手动模式)
void set_brightness_manual(uint8_t brightness);

// 获取当前亮度值
uint8_t get_current_brightness(void);

// 设置亮度配置
void set_brightness_config(const brightness_config_t* config);

// 获取亮度配置
brightness_config_t get_brightness_config(void);

// 注册亮度状态变化回调
void set_brightness_status_callback(brightness_status_callback_t callback);

// 自动息屏功能
void brightness_wake_up(void);          // 唤醒屏幕（用户活动时调用）
void brightness_set_auto_sleep_enabled(bool enabled);  // 开关自动息屏功能
bool brightness_get_auto_sleep_enabled(void);          // 获取自动息屏功能状态
void brightness_set_sleep_timeout(uint32_t timeout_ms); // 设置息屏超时时间
uint32_t brightness_get_sleep_timeout(void);           // 获取息屏超时时间
bool brightness_is_screen_sleeping(void);              // 检查屏幕是否处于睡眠状态
bool brightness_is_in_sleep_transition(void);          // 检查是否正在进入睡眠状态

// 获取环境光传感器当前ADC值 (调试用)
int get_ambient_light_raw(void);

// 获取当前动态亮度范围最大值 (调试用)
int get_max_brightness_range(void);

// 重置动态亮度范围 (调试用)
void reset_brightness_range(void);

// NVS持久化功能
esp_err_t brightness_save_config_to_nvs(void);
esp_err_t brightness_load_config_from_nvs(void);

// 重置历史最大亮度值
void brightness_reset_max_history(void);