#include "system_low_power_design.hpp"
#include "lcd_brightness.hpp"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_clk_tree.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "LVGL_POWER";

// 全局变量
static lvgl_power_mode_t g_current_mode = LVGL_POWER_MODE_NORMAL;
static lvgl_power_config_t g_power_config = {
    .normal_refresh_ms = 5,      // 200Hz
    .low_power_refresh_ms = 20,  // 50Hz
    .sleep_refresh_ms = 100,     // 10Hz
    .enable_timer_pause = true,
    .enable_animation_pause = true
};
static SemaphoreHandle_t g_power_mutex = NULL;
static bool g_timers_paused = false;
static bool g_animations_paused = false;

esp_err_t lvgl_power_manager_init(void)
{
    if (g_power_mutex == NULL) {
        g_power_mutex = xSemaphoreCreateMutex();
        if (g_power_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create power manager mutex");
            return ESP_FAIL;
        }
    }
    
    g_current_mode = LVGL_POWER_MODE_NORMAL;
    g_timers_paused = false;
    g_animations_paused = false;
    
    ESP_LOGI(TAG, "LVGL Power Manager initialized");
    return ESP_OK;
}

esp_err_t lvgl_power_manager_set_mode(lvgl_power_mode_t mode)
{
    if (g_power_mutex == NULL) {
        ESP_LOGE(TAG, "Power manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_power_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire power manager mutex");
        return ESP_FAIL;
    }
    
    lvgl_power_mode_t old_mode = g_current_mode;
    g_current_mode = mode;
    
    // 根据模式调整LVGL设置
    switch (mode) {
        case LVGL_POWER_MODE_NORMAL:
            // 恢复正常模式
            if (g_timers_paused) {
                lvgl_power_manager_pause_timers(false);
            }
            if (g_animations_paused) {
                lvgl_power_manager_pause_animations(false);
            }
            ESP_LOGI(TAG, "Switched to NORMAL power mode (refresh: %lums)", g_power_config.normal_refresh_ms);
            break;
            
        case LVGL_POWER_MODE_LOW_POWER:
            // 低功耗模式
            if (g_power_config.enable_animation_pause && !g_animations_paused) {
                lvgl_power_manager_pause_animations(true);
            }
            ESP_LOGI(TAG, "Switched to LOW_POWER mode (refresh: %lums)", g_power_config.low_power_refresh_ms);
            break;
            
        case LVGL_POWER_MODE_SLEEP:
            // 睡眠模式
            if (g_power_config.enable_animation_pause && !g_animations_paused) {
                lvgl_power_manager_pause_animations(true);
            }
            ESP_LOGI(TAG, "Switched to SLEEP mode (refresh: %lums)", g_power_config.sleep_refresh_ms);
            break;
            
        case LVGL_POWER_MODE_DEEP_SLEEP:
            // 深度睡眠模式
            if (g_power_config.enable_timer_pause && !g_timers_paused) {
                lvgl_power_manager_pause_timers(true);
            }
            if (g_power_config.enable_animation_pause && !g_animations_paused) {
                lvgl_power_manager_pause_animations(true);
            }
            ESP_LOGI(TAG, "Switched to DEEP_SLEEP mode (timers paused)");
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown power mode: %d", mode);
            g_current_mode = old_mode;
            xSemaphoreGive(g_power_mutex);
            return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreGive(g_power_mutex);
    return ESP_OK;
}

lvgl_power_mode_t lvgl_power_manager_get_mode(void)
{
    return g_current_mode;
}

uint32_t lvgl_power_manager_get_refresh_interval(void)
{
    switch (g_current_mode) {
        case LVGL_POWER_MODE_NORMAL:
            return g_power_config.normal_refresh_ms;
        case LVGL_POWER_MODE_LOW_POWER:
            return g_power_config.low_power_refresh_ms;
        case LVGL_POWER_MODE_SLEEP:
            return g_power_config.sleep_refresh_ms;
        case LVGL_POWER_MODE_DEEP_SLEEP:
            return 1000; // 1秒，基本不刷新
        default:
            return g_power_config.normal_refresh_ms;
    }
}

esp_err_t lvgl_power_manager_set_config(const lvgl_power_config_t* config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_power_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_power_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_FAIL;
    }
    
    g_power_config = *config;
    
    xSemaphoreGive(g_power_mutex);

    ESP_LOGI(TAG, "Power config updated - Normal: %lums, Low: %lums, Sleep: %lums", 
             config->normal_refresh_ms, config->low_power_refresh_ms, config->sleep_refresh_ms);
    
    return ESP_OK;
}

esp_err_t lvgl_power_manager_get_config(lvgl_power_config_t* config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *config = g_power_config;
    return ESP_OK;
}

void lvgl_power_manager_auto_adjust(void)
{
    if (g_power_mutex == NULL) {
        return;
    }
    
    // 检查屏幕状态
    bool screen_sleeping = brightness_is_screen_sleeping();
    bool in_transition = brightness_is_in_sleep_transition();
    
    lvgl_power_mode_t target_mode;
    
    if (screen_sleeping) {
        // 屏幕完全睡眠
        target_mode = LVGL_POWER_MODE_SLEEP;
    } else if (in_transition) {
        // 正在进入睡眠状态
        target_mode = LVGL_POWER_MODE_LOW_POWER;
    } else {
        // 正常状态
        target_mode = LVGL_POWER_MODE_NORMAL;
    }
    
    // 只有当模式需要改变时才更新
    if (target_mode != g_current_mode) {
        lvgl_power_manager_set_mode(target_mode);
    }
}

void lvgl_power_manager_pause_timers(bool pause)
{
    if (pause && !g_timers_paused) {
        // 暂停所有LVGL定时器处理
        lv_timer_enable(false);
        g_timers_paused = true;
        ESP_LOGI(TAG, "LVGL timers paused");
    } else if (!pause && g_timers_paused) {
        // 恢复所有LVGL定时器处理
        lv_timer_enable(true);
        g_timers_paused = false;
        ESP_LOGI(TAG, "LVGL timers resumed");
    }
}

void lvgl_power_manager_pause_animations(bool pause)
{
    if (pause && !g_animations_paused) {
        // 记录动画暂停状态
        // 注意：LVGL没有直接暂停所有动画的API
        // 这里只是记录状态，实际的动画控制需要在应用层实现
        g_animations_paused = true;
        ESP_LOGI(TAG, "LVGL animations marked as paused (application should handle this)");
    } else if (!pause && g_animations_paused) {
        // 恢复动画状态
        g_animations_paused = false;
        ESP_LOGI(TAG, "LVGL animations marked as resumed");
    }
}

bool lvgl_power_manager_are_animations_paused(void)
{
    return g_animations_paused;
}

/**
 * 获取系统功耗统计信息（用于调试）
 */
void lvgl_power_manager_print_stats(void)
{
    ESP_LOGI(TAG, "=== LVGL Power Manager Stats ===");
    ESP_LOGI(TAG, "Current Mode: %d", g_current_mode);
    ESP_LOGI(TAG, "Refresh Interval: %lu ms", lvgl_power_manager_get_refresh_interval());
    ESP_LOGI(TAG, "Timers Paused: %s", g_timers_paused ? "Yes" : "No");
    ESP_LOGI(TAG, "Animations Paused: %s", g_animations_paused ? "Yes" : "No");
    ESP_LOGI(TAG, "Screen Sleeping: %s", brightness_is_screen_sleeping() ? "Yes" : "No");
    ESP_LOGI(TAG, "In Sleep Transition: %s", brightness_is_in_sleep_transition() ? "Yes" : "No");
    ESP_LOGI(TAG, "==============================");
}
