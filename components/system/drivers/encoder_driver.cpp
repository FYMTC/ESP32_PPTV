#include "encoder_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

// 包含亮度控制包装头文件
#include "lcd_brightness.hpp"
static const char *TAG = "ENCODER_DRIVER";

// 去抖动和灵敏度设置
#define ENCODER_DEBOUNCE_TIME_MS    20    // 编码器去抖动时间（毫秒）
#define BUTTON_DEBOUNCE_TIME_MS     50    // 按钮去抖动时间（毫秒）
#define ENCODER_MIN_PULSE_WIDTH_US  1000  // 最小脉冲宽度（微秒）

// 编码器状态变量
static volatile int32_t encoder_count = 0;
static volatile int32_t encoder_diff = 0;
static volatile bool encoder_button_pressed = false;
static volatile encoder_event_t last_event = ENCODER_EVENT_NONE;

// 编码器引脚状态
static uint8_t last_a_state = 0;
static uint8_t last_b_state = 0;

// 去抖动相关变量
static volatile uint32_t last_encoder_time = 0;
static volatile uint32_t last_button_time = 0;
static volatile uint8_t encoder_step_count = 0;  // 编码器步数计数器
static uint8_t encoder_sensitivity = 4;          // 编码器灵敏度（默认4步一个计数）

// 回调函数指针
static encoder_callback_t event_callback = NULL;

// 初始化标志
static bool is_initialized = false;

/**
 * @brief GPIO中断处理函数
 */
static void IRAM_ATTR encoder_gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    uint32_t current_time_us = (uint32_t)(esp_timer_get_time());
    
    if (gpio_num == ENCODER_BT_PIN) {
        // 按钮防抖动处理
        uint32_t time_diff_ms = (current_time_us - last_button_time) / 1000;
        if (time_diff_ms < BUTTON_DEBOUNCE_TIME_MS) {
            return; // 忽略抖动
        }
        
        last_button_time = current_time_us;
        encoder_button_pressed = true;
        last_event = ENCODER_EVENT_BUTTON;
        
        // 唤醒屏幕
        brightness_wake_up();
        
        // 调用回调函数
        if (event_callback) {
            event_callback(ENCODER_EVENT_BUTTON, encoder_count);
        }
    } else if (gpio_num == ENCODER_A_PIN || gpio_num == ENCODER_B_PIN) {
        // 编码器防抖动处理
        uint32_t time_diff_us = current_time_us - last_encoder_time;
        if (time_diff_us < ENCODER_MIN_PULSE_WIDTH_US) {
            return; // 忽略过快的脉冲
        }
        
        // 读取当前状态
        uint8_t a_state = gpio_get_level(ENCODER_A_PIN);
        uint8_t b_state = gpio_get_level(ENCODER_B_PIN);
        
        // 只有当A相发生变化时才处理
        if (a_state != last_a_state) {
            // 四分频处理：只在A相下降沿和B相状态稳定时计数
            if (a_state == 0) { // A相下降沿
                // 增加步数计数器
                encoder_step_count = encoder_step_count + 1;
                
                // 使用可配置的灵敏度进行分频
                if (encoder_step_count >= encoder_sensitivity) {
                    encoder_step_count = 0;
                    last_encoder_time = current_time_us;
                    
                    if (b_state == 1) {
                        encoder_count = encoder_count + 1;
                        encoder_diff = encoder_diff + 1;
                        last_event = ENCODER_EVENT_CW;
                        
                        // 唤醒屏幕
                        brightness_wake_up();
                        
                        // 调用回调函数
                        if (event_callback) {
                            event_callback(ENCODER_EVENT_CW, encoder_count);
                        }
                    } else {
                        encoder_count = encoder_count - 1;
                        encoder_diff = encoder_diff - 1;
                        last_event = ENCODER_EVENT_CCW;
                        
                        // 唤醒屏幕
                        brightness_wake_up();
                        
                        // 调用回调函数
                        if (event_callback) {
                            event_callback(ENCODER_EVENT_CCW, encoder_count);
                        }
                    }
                }
            }
        }
        
        // 更新状态
        last_a_state = a_state;
        last_b_state = b_state;
    }
}

esp_err_t encoder_driver_init(void)
{
    if (is_initialized) {
        ESP_LOGW(TAG, "Encoder driver already initialized");
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;

    // 配置编码器A相引脚
    gpio_config_t io_conf_a = {};
    io_conf_a.pin_bit_mask = 1ULL << ENCODER_A_PIN;
    io_conf_a.mode = GPIO_MODE_INPUT;
    io_conf_a.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf_a.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf_a.intr_type = GPIO_INTR_ANYEDGE;
    ret = gpio_config(&io_conf_a);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", ENCODER_A_PIN, esp_err_to_name(ret));
        return ret;
    }

    // 配置编码器B相引脚
    gpio_config_t io_conf_b = {};
    io_conf_b.pin_bit_mask = 1ULL << ENCODER_B_PIN;
    io_conf_b.mode = GPIO_MODE_INPUT;
    io_conf_b.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf_b.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf_b.intr_type = GPIO_INTR_ANYEDGE;
    ret = gpio_config(&io_conf_b);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", ENCODER_B_PIN, esp_err_to_name(ret));
        return ret;
    }

    // 配置编码器按钮引脚
    gpio_config_t io_conf_bt = {};
    io_conf_bt.pin_bit_mask = 1ULL << ENCODER_BT_PIN;
    io_conf_bt.mode = GPIO_MODE_INPUT;
    io_conf_bt.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf_bt.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf_bt.intr_type = GPIO_INTR_NEGEDGE;
    ret = gpio_config(&io_conf_bt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", ENCODER_BT_PIN, esp_err_to_name(ret));
        return ret;
    }

    // 安装GPIO中断服务（如果尚未安装）
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 为编码器引脚添加中断处理函数
    ret = gpio_isr_handler_add(ENCODER_A_PIN, encoder_gpio_isr_handler, (void*)ENCODER_A_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d: %s", ENCODER_A_PIN, esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_isr_handler_add(ENCODER_B_PIN, encoder_gpio_isr_handler, (void*)ENCODER_B_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d: %s", ENCODER_B_PIN, esp_err_to_name(ret));
        gpio_isr_handler_remove(ENCODER_A_PIN);
        return ret;
    }

    ret = gpio_isr_handler_add(ENCODER_BT_PIN, encoder_gpio_isr_handler, (void*)ENCODER_BT_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d: %s", ENCODER_BT_PIN, esp_err_to_name(ret));
        gpio_isr_handler_remove(ENCODER_A_PIN);
        gpio_isr_handler_remove(ENCODER_B_PIN);
        return ret;
    }

    // 读取初始状态
    last_a_state = gpio_get_level(ENCODER_A_PIN);
    last_b_state = gpio_get_level(ENCODER_B_PIN);
    
    // 初始化变量
    encoder_count = 0;
    encoder_diff = 0;
    encoder_button_pressed = false;
    last_event = ENCODER_EVENT_NONE;
    
    // 初始化时间变量
    last_encoder_time = (uint32_t)(esp_timer_get_time());
    last_button_time = last_encoder_time;
    encoder_step_count = 0;
    
    is_initialized = true;
    
    ESP_LOGI(TAG, "Encoder driver initialized successfully");
    ESP_LOGI(TAG, "Pins - A: %d, B: %d, Button: %d", ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_BT_PIN);
    
    return ESP_OK;
}

esp_err_t encoder_driver_deinit(void)
{
    if (!is_initialized) {
        ESP_LOGW(TAG, "Encoder driver not initialized");
        return ESP_OK;
    }

    // 移除中断处理函数
    gpio_isr_handler_remove(ENCODER_A_PIN);
    gpio_isr_handler_remove(ENCODER_B_PIN);
    gpio_isr_handler_remove(ENCODER_BT_PIN);

    // 重置所有变量
    encoder_count = 0;
    encoder_diff = 0;
    encoder_button_pressed = false;
    last_event = ENCODER_EVENT_NONE;
    last_encoder_time = 0;
    last_button_time = 0;
    encoder_step_count = 0;
    event_callback = NULL;
    
    is_initialized = false;
    
    ESP_LOGI(TAG, "Encoder driver deinitialized");
    
    return ESP_OK;
}

esp_err_t encoder_get_state(encoder_state_t *state)
{
    if (!is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    // 临时禁用中断以确保数据一致性
    portDISABLE_INTERRUPTS();
    state->count = encoder_count;
    state->button_pressed = encoder_button_pressed;
    state->last_event = last_event;
    portENABLE_INTERRUPTS();

    return ESP_OK;
}

int32_t encoder_get_diff(void)
{
    if (!is_initialized) {
        return 0;
    }

    // 原子读取并重置差值
    portDISABLE_INTERRUPTS();
    int32_t diff = encoder_diff;
    encoder_diff = 0;
    portENABLE_INTERRUPTS();

    return diff;
}

bool encoder_is_button_pressed(void)
{
    if (!is_initialized) {
        return false;
    }

    // 原子读取并重置按钮状态
    portDISABLE_INTERRUPTS();
    bool pressed = encoder_button_pressed;
    encoder_button_pressed = false;
    portENABLE_INTERRUPTS();

    return pressed;
}

void encoder_reset_count(void)
{
    if (!is_initialized) {
        return;
    }

    portDISABLE_INTERRUPTS();
    encoder_count = 0;
    encoder_diff = 0;
    portENABLE_INTERRUPTS();

    ESP_LOGI(TAG, "Encoder count reset");
}

void encoder_set_callback(encoder_callback_t callback)
{
    event_callback = callback;
    ESP_LOGI(TAG, "Encoder callback %s", callback ? "set" : "cleared");
}

esp_err_t encoder_set_sensitivity(uint8_t steps_per_count)
{
    if (steps_per_count < 1 || steps_per_count > 16) {
        ESP_LOGE(TAG, "Invalid sensitivity value: %d (must be 1-16)", steps_per_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    portDISABLE_INTERRUPTS();
    encoder_sensitivity = steps_per_count;
    encoder_step_count = 0; // 重置步数计数器
    portENABLE_INTERRUPTS();
    
    ESP_LOGI(TAG, "Encoder sensitivity set to %d steps per count", steps_per_count);
    return ESP_OK;
}

uint8_t encoder_get_sensitivity(void)
{
    return encoder_sensitivity;
}
