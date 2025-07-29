#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LVGL功耗管理器
 * 根据屏幕状态动态调整LVGL刷新频率和其他功耗相关设置
 */

// LVGL功耗模式
typedef enum {
    LVGL_POWER_MODE_NORMAL = 0,      // 正常模式 - 全速刷新
    LVGL_POWER_MODE_LOW_POWER,       // 低功耗模式 - 降低刷新频率
    LVGL_POWER_MODE_SLEEP,           // 睡眠模式 - 最低刷新频率
    LVGL_POWER_MODE_DEEP_SLEEP       // 深度睡眠模式 - 停止刷新
} lvgl_power_mode_t;

// LVGL功耗配置
typedef struct {
    uint32_t normal_refresh_ms;      // 正常模式刷新间隔 (默认5ms)
    uint32_t low_power_refresh_ms;   // 低功耗模式刷新间隔 (默认20ms)
    uint32_t sleep_refresh_ms;       // 睡眠模式刷新间隔 (默认100ms)
    bool enable_timer_pause;         // 是否在深度睡眠时暂停定时器
    bool enable_animation_pause;     // 是否在低功耗模式下暂停动画
} lvgl_power_config_t;

/**
 * 初始化LVGL功耗管理器
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t lvgl_power_manager_init(void);

/**
 * 设置LVGL功耗模式
 * @param mode 功耗模式
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t lvgl_power_manager_set_mode(lvgl_power_mode_t mode);

/**
 * 获取当前LVGL功耗模式
 * @return 当前功耗模式
 */
lvgl_power_mode_t lvgl_power_manager_get_mode(void);

/**
 * 获取当前刷新间隔
 * @return 刷新间隔（毫秒）
 */
uint32_t lvgl_power_manager_get_refresh_interval(void);

/**
 * 设置功耗配置
 * @param config 功耗配置
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t lvgl_power_manager_set_config(const lvgl_power_config_t* config);

/**
 * 获取功耗配置
 * @param config 输出参数，用于接收当前配置
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t lvgl_power_manager_get_config(lvgl_power_config_t* config);

/**
 * 自动调整功耗模式（基于屏幕状态）
 * 这个函数会自动检查屏幕状态并调整功耗模式
 */
void lvgl_power_manager_auto_adjust(void);

/**
 * 暂停/恢复LVGL定时器
 * @param pause true暂停，false恢复
 */
void lvgl_power_manager_pause_timers(bool pause);

/**
 * 暂停/恢复LVGL动画
 * @param pause true暂停，false恢复
 */
void lvgl_power_manager_pause_animations(bool pause);

/**
 * 检查动画是否被暂停
 * @return true动画被暂停，false动画正常
 */
bool lvgl_power_manager_are_animations_paused(void);

#ifdef __cplusplus
}
#endif
