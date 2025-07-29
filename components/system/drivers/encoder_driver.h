#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 编码器引脚定义
#define ENCODER_A_PIN GPIO_NUM_16 // A 相连接到 GPIO16
#define ENCODER_B_PIN GPIO_NUM_2  // B 相连接到 GPIO2
#define ENCODER_BT_PIN GPIO_NUM_3 // 按钮连接到 GPIO3

// 编码器事件类型
typedef enum {
    ENCODER_EVENT_NONE = 0,
    ENCODER_EVENT_CW,      // 顺时针旋转
    ENCODER_EVENT_CCW,     // 逆时针旋转
    ENCODER_EVENT_BUTTON   // 按钮按下
} encoder_event_t;

// 编码器状态结构体
typedef struct {
    int32_t count;          // 编码器计数值
    bool button_pressed;    // 按钮状态
    encoder_event_t last_event; // 最后一次事件
} encoder_state_t;

// 编码器回调函数类型
typedef void (*encoder_callback_t)(encoder_event_t event, int32_t count);

/**
 * @brief 初始化编码器驱动
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t encoder_driver_init(void);

/**
 * @brief 反初始化编码器驱动
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t encoder_driver_deinit(void);

/**
 * @brief 获取编码器当前状态
 * @param state 输出编码器状态
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t encoder_get_state(encoder_state_t *state);

/**
 * @brief 获取编码器计数差值（相对于上次读取）
 * @return 编码器计数差值
 */
int32_t encoder_get_diff(void);

/**
 * @brief 检查按钮是否被按下
 * @return true 按钮被按下，false 按钮未被按下
 */
bool encoder_is_button_pressed(void);

/**
 * @brief 重置编码器计数
 */
void encoder_reset_count(void);

/**
 * @brief 设置编码器事件回调函数
 * @param callback 回调函数指针
 */
void encoder_set_callback(encoder_callback_t callback);

/**
 * @brief 设置编码器灵敏度（步数分频）
 * @param steps_per_count 每多少步产生一个计数（1-16）
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t encoder_set_sensitivity(uint8_t steps_per_count);

/**
 * @brief 获取当前编码器灵敏度设置
 * @return 当前的步数分频值
 */
uint8_t encoder_get_sensitivity(void);

#ifdef __cplusplus
}
#endif
