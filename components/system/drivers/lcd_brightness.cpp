
#include "lcd_brightness.hpp"
#include "nvs_init.hpp"
#include "esp_adc/adc_oneshot.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "LGFX_disp.hpp"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "brightness";

// NVS相关常量
#define BRIGHTNESS_NAMESPACE "brightness"
#define NVS_KEY_MODE "mode"
#define NVS_KEY_MANUAL_BRIGHTNESS "manual_val"
#define NVS_KEY_MIN_BRIGHTNESS "min_val"
#define NVS_KEY_MAX_BRIGHTNESS "max_val"
#define NVS_KEY_SMOOTH_TRANSITION "smooth"
#define NVS_KEY_AUTO_SLEEP_ENABLED "auto_sleep"
#define NVS_KEY_SLEEP_TIMEOUT "sleep_timeout"
#define NVS_KEY_SLEEP_DELAY "sleep_delay"
#define NVS_KEY_SLEEP_BRIGHTNESS "sleep_bright"

#define DEFAULT_VREF 3300 // 默认参考电压 (mV)

#define LED_PIN GPIO_NUM_48             // LED 连接的 GPIO 引脚
#define LEDC_CHANNEL LEDC_CHANNEL_0     // LEDC 通道
#define LEDC_TIMER LEDC_TIMER_0         // LEDC 定时器
#define LEDC_MODE LEDC_LOW_SPEED_MODE   // LEDC 模式
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // 13 位分辨率（0-8191）

// 定义滤波器参数
#define FILTER_WINDOW_SIZE 100                  // 滤波器窗口大小（历史值的数量）
uint8_t brightness_history[FILTER_WINDOW_SIZE]; // 环形缓冲区，存储历史亮度值
int history_index = 0;                          // 当前缓冲区索引
int MAX_BRIGHTNESS = 1;      // 历史最大亮度ADC值，作为动态亮度范围
int current_adc_reading = 0; // 当前ADC读数
// 全局变量
static brightness_config_t g_brightness_config;
static uint8_t g_current_brightness = 0;
static uint8_t g_target_brightness = 0;      // 目标亮度（用于平滑过渡）
static brightness_status_callback_t g_brightness_callback = NULL;
static SemaphoreHandle_t g_brightness_mutex = NULL;
static bool g_task_running = false;

// 自动息屏相关变量
static volatile uint32_t g_last_activity_time = 0;    // 最后一次用户活动时间（原子访问）
static volatile bool g_wakeup_requested = false;      // ISR请求唤醒标志（原子访问）
static bool g_screen_sleeping = false;       // 屏幕是否处于息屏状态
static bool g_in_sleep_transition = false;   // 是否正在进行息屏过渡
static uint8_t g_brightness_before_sleep = 0; // 息屏前的亮度值

// 内部函数前向声明
static void brightness_handle_wakeup_request(void);

// 辅助函数：映射数值范围
static long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    if (in_max == in_min)
        return out_min; // 避免除零
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void init_ledc(void)
{
    // 配置 LEDC 定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = 5000, // PWM 频率 5kHz
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 配置 LEDC 通道
    ledc_channel_config_t ledc_channel = {
        .gpio_num = LED_PIN,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0, // 初始占空比为 0（LED 关闭）
        .hpoint = 0,
        .flags = {
            .output_invert = 0,
        },
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// 移动平均滤波器函数
uint8_t apply_moving_average_filter(uint8_t new_brightness)
{
    // 将新值添加到环形缓冲区
    brightness_history[history_index] = new_brightness;
    history_index = (history_index + 1) % FILTER_WINDOW_SIZE;

    // 计算缓冲区中所有值的平均值
    uint16_t sum = 0;
    for (int i = 0; i < FILTER_WINDOW_SIZE; i++)
    {
        sum += brightness_history[i];
    }
    return (uint8_t)(sum / FILTER_WINDOW_SIZE);
}
void brightness_task(void *pvParameters)
{
    // ADC 配置
    adc_oneshot_unit_handle_t adc1_handle = NULL;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,              // 使用 ADC1
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT, // 默认时钟源
        .ulp_mode = ADC_ULP_MODE_DISABLE    // 禁用ULP模式
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "ADC1 is already in use by another component, trying to reuse");
            // 尝试直接配置通道而不创建新的ADC单元
            // 注意：这种情况下我们无法正确读取ADC，需要找到已存在的ADC句柄
            ESP_LOGE(TAG, "Cannot initialize ADC1: already in use. Brightness control will be disabled.");
            
            // 设置任务运行标志为false，表示任务无法正常工作
            if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
                g_task_running = false;
                xSemaphoreGive(g_brightness_mutex);
            }
            
            // 任务退出
            vTaskDelete(NULL);
            return;
        } else {
            ESP_LOGE(TAG, "Failed to initialize ADC1: %s", esp_err_to_name(ret));
            vTaskDelete(NULL);
            return;
        }
    }

    // ADC 通道配置
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_11,    // 设置衰减（11dB 适用于 0-3.3V 范围）
        .bitwidth = ADC_BITWIDTH_12, // 12 位分辨率
    };
    ret = adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(adc1_handle);
        vTaskDelete(NULL);
        return;
    }

    // 初始化 LEDC PWM
    //init_ledc();

    // 初始化亮度历史缓冲区
    for (int i = 0; i < FILTER_WINDOW_SIZE; i++)
    {
        brightness_history[i] = g_brightness_config.min_brightness;
    }

    ESP_LOGI(TAG, "Brightness control task started");

    while (1)
    {
        if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            // 处理来自ISR的唤醒请求
            if (g_wakeup_requested)
            {
                g_wakeup_requested = false;
                brightness_handle_wakeup_request();
            }
            
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // 检查自动息屏功能
            if (g_brightness_config.auto_sleep_enabled && !g_screen_sleeping && !g_in_sleep_transition)
            {
                uint32_t time_since_activity = current_time - g_last_activity_time;
                
                if (time_since_activity >= g_brightness_config.sleep_timeout_ms)
                {
                    // 开始息屏过渡
                    ESP_LOGI(TAG, "Starting auto sleep transition");
                    g_in_sleep_transition = true;
                    g_brightness_before_sleep = g_current_brightness;
                    g_target_brightness = g_brightness_config.sleep_brightness;
                }
            }
            
            // 处理息屏过渡
            if (g_in_sleep_transition && !g_screen_sleeping)
            {
                uint32_t time_since_activity = current_time - g_last_activity_time;
                uint32_t transition_time = time_since_activity - g_brightness_config.sleep_timeout_ms;
                
                if (transition_time >= g_brightness_config.sleep_delay_ms)
                {
                    // 完成息屏过渡，设置亮度为0
                    g_screen_sleeping = true;
                    g_in_sleep_transition = false;
                    g_current_brightness = 0;
                    
                    // 设置TFT显示亮度为0
                    auto tft = get_lgfx_tft();
                    tft->setBrightness(0);
                    
                    ESP_LOGI(TAG, "Screen entered sleep mode");
                    
                    // 触发回调
                    if (g_brightness_callback)
                    {
                        g_brightness_callback(0, g_brightness_config.mode);
                    }
                }
                else
                {
                    // 过渡阶段，保持最低亮度，暗示马上要息屏了。
                    g_current_brightness = g_brightness_config.sleep_brightness;
                    
                    // 设置TFT显示亮度
                    auto tft = get_lgfx_tft();
                    tft->setBrightness(g_current_brightness);
                }
                
                xSemaphoreGive(g_brightness_mutex);
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            
            // 如果屏幕正在睡眠，跳过正常的亮度调节
            if (g_screen_sleeping)
            {
                xSemaphoreGive(g_brightness_mutex);
                vTaskDelay(pdMS_TO_TICKS(100)); // 睡眠时减少检查频率
                continue;
            }

            if (g_brightness_config.mode == BRIGHTNESS_MODE_AUTO)
            {
                // 自动亮度模式
                int adc_reading = 0;
                // 只读取一次
                ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &adc_reading));

                // 动态调整最大亮度范围（保留原始设计）
                MAX_BRIGHTNESS = adc_reading > MAX_BRIGHTNESS ? adc_reading : MAX_BRIGHTNESS;
                current_adc_reading = adc_reading;

                // 根据 ADC 读数调整 LED 亮度
                // 暗光时 ADC 读数小，LED 亮度低；亮光时 ADC 读数大，LED 亮度高
                uint32_t duty = (adc_reading * 8191) / MAX_BRIGHTNESS; // 将 ADC 读数映射到 0-8191 的占空比范围
                duty = duty > 8191 ? 8191 : duty;
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

                uint8_t target_brightness = (uint8_t)(duty * 255 / 8191);                                                                            // 将占空比转换为 0-255 的亮度值
                target_brightness = target_brightness < g_brightness_config.min_brightness ? g_brightness_config.min_brightness : target_brightness; // 确保最小亮度
                target_brightness = target_brightness > g_brightness_config.max_brightness ? g_brightness_config.max_brightness : target_brightness; // 确保最大亮度限制

                // 应用移动平均滤波
                g_current_brightness = apply_moving_average_filter(target_brightness);

                // 设置TFT显示亮度
                auto tft = get_lgfx_tft();
                tft->setBrightness(g_current_brightness);

                // 触发回调
                if (g_brightness_callback)
                {
                    g_brightness_callback(g_current_brightness, BRIGHTNESS_MODE_AUTO);
                }

                // ESP_LOGD(TAG, "Auto brightness: ADC=%d, MAX=%d, Duty=%d, Brightness=%d",
                //          adc_reading, MAX_BRIGHTNESS, (int)duty, g_current_brightness);
            }
            // 手动模式下不需要在任务中做任何事情，亮度由set_brightness_manual函数控制

            xSemaphoreGive(g_brightness_mutex);
        }

        // 等待下一次执行（每10ms检查一次，保持原有的响应速度）
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 清理 ADC 资源
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}

void create_brightness_task(void)
{
    // 检查是否已经创建过任务
    if (g_brightness_mutex != NULL) {
        ESP_LOGW(TAG, "Brightness task already created, skipping");
        return;
    }
    
    // 创建互斥锁
    g_brightness_mutex = xSemaphoreCreateMutex();
    if (g_brightness_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create brightness mutex");
        return;
    }

    // 设置默认配置
    g_brightness_config.mode = BRIGHTNESS_MODE_AUTO;
    g_brightness_config.manual_brightness = 128;
    g_brightness_config.min_brightness = 12;
    g_brightness_config.max_brightness = 255;
    g_brightness_config.light_threshold_low = 100;
    g_brightness_config.light_threshold_high = 3000;
    g_brightness_config.auto_adjust_interval = 200;
    g_brightness_config.smooth_transition = true;
    
    // 自动息屏默认配置
    g_brightness_config.auto_sleep_enabled = true;
    g_brightness_config.sleep_timeout_ms = 30000;  // 30秒
    g_brightness_config.sleep_delay_ms = 3000;     // 3秒
    g_brightness_config.sleep_brightness = 1;      // 最低亮度
    
    g_current_brightness = g_brightness_config.min_brightness;
    g_target_brightness = g_brightness_config.min_brightness;
    
    // 初始化自动息屏相关变量
    g_last_activity_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    g_screen_sleeping = false;
    g_in_sleep_transition = false;
    g_brightness_before_sleep = 0;

    // 创建亮度控制任务
    BaseType_t result = xTaskCreate(brightness_task, "brightness_task", 4096, NULL, 10, NULL);
    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create brightness task");
        vSemaphoreDelete(g_brightness_mutex);
        g_brightness_mutex = NULL;
        return;
    }

    g_task_running = true;
    ESP_LOGI(TAG, "Brightness control system initialized");
    
    // 尝试从NVS加载保存的配置
    esp_err_t load_err = brightness_load_config_from_nvs();
    if (load_err == ESP_OK) {
        ESP_LOGI(TAG, "Brightness configuration restored from NVS");
    } else if (load_err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved brightness configuration, using defaults");
        // 保存默认配置到NVS
        brightness_save_config_to_nvs();
    } else {
        ESP_LOGW(TAG, "Failed to load brightness configuration from NVS: %s", esp_err_to_name(load_err));
    }
}

// 新增API函数实现
void set_brightness_mode(brightness_mode_t mode)
{
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        brightness_mode_t old_mode = g_brightness_config.mode;
        g_brightness_config.mode = mode;
        ESP_LOGI(TAG, "Brightness mode changed to: %s",
                 mode == BRIGHTNESS_MODE_AUTO ? "AUTO" : "MANUAL");
        xSemaphoreGive(g_brightness_mutex);
        
        // 如果模式发生了变化，保存到NVS
        if (old_mode != mode) {
            esp_err_t save_err = brightness_save_config_to_nvs();
            if (save_err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to save brightness mode to NVS: %s", esp_err_to_name(save_err));
            }
        }
    }
}

brightness_mode_t get_brightness_mode(void)
{
    brightness_mode_t mode = BRIGHTNESS_MODE_AUTO;
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        mode = g_brightness_config.mode;
        xSemaphoreGive(g_brightness_mutex);
    }
    return mode;
}

void set_brightness_manual(uint8_t brightness)
{
    if (!g_task_running)
    {
        ESP_LOGW(TAG, "Brightness task not running");
        return;
    }

    // 切换到手动模式并设置亮度
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        g_brightness_config.mode = BRIGHTNESS_MODE_MANUAL;

        // 限制亮度范围
        if (brightness < g_brightness_config.min_brightness)
        {
            brightness = g_brightness_config.min_brightness;
        }
        if (brightness > g_brightness_config.max_brightness)
        {
            brightness = g_brightness_config.max_brightness;
        }

        g_current_brightness = brightness;
        g_brightness_config.manual_brightness = brightness; // 更新配置中的手动亮度值

        // 设置TFT显示亮度
        auto tft = get_lgfx_tft();
        tft->setBrightness(g_current_brightness);

        // 设置LED亮度（用于指示）
        uint32_t duty = (g_current_brightness * 8191) / 255;
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

        // 触发回调
        if (g_brightness_callback)
        {
            g_brightness_callback(g_current_brightness, BRIGHTNESS_MODE_MANUAL);
        }

        ESP_LOGI(TAG, "Manual brightness set to: %d", g_current_brightness);
        xSemaphoreGive(g_brightness_mutex);
        
        // 异步保存到NVS（释放互斥锁后）
        esp_err_t save_err = brightness_save_config_to_nvs();
        if (save_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save brightness config to NVS: %s", esp_err_to_name(save_err));
        }
    }
}

uint8_t get_current_brightness(void)
{
    uint8_t brightness = 0;
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        brightness = g_current_brightness;
        xSemaphoreGive(g_brightness_mutex);
    }
    return brightness;
}

void set_brightness_config(const brightness_config_t *config)
{
    if (!config)
    {
        ESP_LOGW(TAG, "Invalid brightness config");
        return;
    }

    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        g_brightness_config = *config;
        ESP_LOGI(TAG, "Brightness config updated");
        xSemaphoreGive(g_brightness_mutex);
    }
}

brightness_config_t get_brightness_config(void)
{
    brightness_config_t config = {};
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        config = g_brightness_config;
        xSemaphoreGive(g_brightness_mutex);
    }
    return config;
}

void set_brightness_status_callback(brightness_status_callback_t callback)
{
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        g_brightness_callback = callback;
        xSemaphoreGive(g_brightness_mutex);
    }
}

// =============================================================================
// 自动息屏功能实现
// =============================================================================

void brightness_wake_up(void)
{
    if (!g_task_running)
    {
        return;
    }

    // ISR安全版本：只设置标志位，不执行阻塞操作
    if (xPortInIsrContext())
    {
        // 从ISR调用：只更新时间和设置唤醒标志
        g_last_activity_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
        g_wakeup_requested = true;
        return;
    }

    // 从普通任务调用：执行完整的唤醒处理
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        // 更新最后活动时间
        g_last_activity_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // 处理唤醒请求
        brightness_handle_wakeup_request();
        
        xSemaphoreGive(g_brightness_mutex);
    }
}

// 内部函数：处理唤醒请求（必须在持有互斥锁时调用）
static void brightness_handle_wakeup_request(void)
{
    // 如果屏幕正在睡眠或过渡中，立即唤醒
    if (g_screen_sleeping || g_in_sleep_transition)
    {
        g_screen_sleeping = false;
        g_in_sleep_transition = false;
        
        // 恢复之前的亮度或使用默认亮度
        uint8_t restore_brightness = g_brightness_before_sleep > 0 ? 
                                   g_brightness_before_sleep : 
                                   g_brightness_config.min_brightness;
        
        g_current_brightness = restore_brightness;
        
        // 设置TFT显示亮度
        auto tft = get_lgfx_tft();
        tft->setBrightness(g_current_brightness);
        
        // 触发回调
        if (g_brightness_callback)
        {
            g_brightness_callback(g_current_brightness, g_brightness_config.mode);
        }
    }
}

void brightness_set_auto_sleep_enabled(bool enabled)
{
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        bool old_enabled = g_brightness_config.auto_sleep_enabled;
        g_brightness_config.auto_sleep_enabled = enabled;
        
        // 如果禁用自动息屏，立即唤醒
        if (!enabled && (g_screen_sleeping || g_in_sleep_transition))
        {
            g_screen_sleeping = false;
            g_in_sleep_transition = false;
            
            uint8_t restore_brightness = g_brightness_before_sleep > 0 ? 
                                       g_brightness_before_sleep : 
                                       g_brightness_config.min_brightness;
            
            g_current_brightness = restore_brightness;
            
            auto tft = get_lgfx_tft();
            tft->setBrightness(g_current_brightness);
            
            if (g_brightness_callback)
            {
                g_brightness_callback(g_current_brightness, g_brightness_config.mode);
            }
        }
        
        ESP_LOGI(TAG, "Auto sleep %s", enabled ? "enabled" : "disabled");
        xSemaphoreGive(g_brightness_mutex);
        
        // 如果设置发生了变化，保存到NVS
        if (old_enabled != enabled) {
            esp_err_t save_err = brightness_save_config_to_nvs();
            if (save_err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to save auto sleep config to NVS: %s", esp_err_to_name(save_err));
            }
        }
    }
}

bool brightness_get_auto_sleep_enabled(void)
{
    bool enabled = false;
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        enabled = g_brightness_config.auto_sleep_enabled;
        xSemaphoreGive(g_brightness_mutex);
    }
    return enabled;
}

void brightness_set_sleep_timeout(uint32_t timeout_ms)
{
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        uint32_t old_timeout = g_brightness_config.sleep_timeout_ms;
        g_brightness_config.sleep_timeout_ms = timeout_ms;
        ESP_LOGI(TAG, "Sleep timeout set to: %lu ms", timeout_ms);
        xSemaphoreGive(g_brightness_mutex);
        
        // 如果设置发生了变化，保存到NVS
        if (old_timeout != timeout_ms) {
            esp_err_t save_err = brightness_save_config_to_nvs();
            if (save_err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to save sleep timeout to NVS: %s", esp_err_to_name(save_err));
            }
        }
    }
}

uint32_t brightness_get_sleep_timeout(void)
{
    uint32_t timeout = 0;
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        timeout = g_brightness_config.sleep_timeout_ms;
        xSemaphoreGive(g_brightness_mutex);
    }
    return timeout;
}

// 获取环境光传感器调试信息
int get_ambient_light_raw(void)
{
    return current_adc_reading;
}

int get_max_brightness_range(void)
{
    return MAX_BRIGHTNESS;
}

void reset_brightness_range(void)
{
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        MAX_BRIGHTNESS = 1; // 重置动态范围
        ESP_LOGI(TAG, "Brightness dynamic range reset");
        xSemaphoreGive(g_brightness_mutex);
    }
}

// =============================================================================
// NVS持久化功能实现
// =============================================================================

// 保存亮度配置到NVS
esp_err_t brightness_save_config_to_nvs(void)
{
    if (!g_task_running) {
        ESP_LOGW(TAG, "Brightness system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_OK;
    
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 保存亮度模式
        err = nvs_save_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_MODE, (uint8_t)g_brightness_config.mode);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save brightness mode to NVS");
            goto cleanup;
        }

        // 保存手动亮度值
        err = nvs_save_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_MANUAL_BRIGHTNESS, g_brightness_config.manual_brightness);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save manual brightness to NVS");
            goto cleanup;
        }

        // 保存最小亮度
        err = nvs_save_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_MIN_BRIGHTNESS, g_brightness_config.min_brightness);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save min brightness to NVS");
            goto cleanup;
        }

        // 保存最大亮度
        err = nvs_save_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_MAX_BRIGHTNESS, g_brightness_config.max_brightness);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save max brightness to NVS");
            goto cleanup;
        }

        // 保存平滑过渡设置
        err = nvs_save_bool(BRIGHTNESS_NAMESPACE, NVS_KEY_SMOOTH_TRANSITION, g_brightness_config.smooth_transition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save smooth transition to NVS");
            goto cleanup;
        }

        // 保存自动息屏设置
        err = nvs_save_bool(BRIGHTNESS_NAMESPACE, NVS_KEY_AUTO_SLEEP_ENABLED, g_brightness_config.auto_sleep_enabled);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save auto sleep enabled to NVS");
            goto cleanup;
        }

        // 保存息屏超时时间
        err = nvs_save_u32(BRIGHTNESS_NAMESPACE, NVS_KEY_SLEEP_TIMEOUT, g_brightness_config.sleep_timeout_ms);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save sleep timeout to NVS");
            goto cleanup;
        }

        // 保存息屏延迟时间
        err = nvs_save_u32(BRIGHTNESS_NAMESPACE, NVS_KEY_SLEEP_DELAY, g_brightness_config.sleep_delay_ms);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save sleep delay to NVS");
            goto cleanup;
        }

        // 保存息屏亮度
        err = nvs_save_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_SLEEP_BRIGHTNESS, g_brightness_config.sleep_brightness);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save sleep brightness to NVS");
            goto cleanup;
        }

        ESP_LOGI(TAG, "Brightness config saved to NVS - Mode: %s, Manual: %d, Range: %d-%d, AutoSleep: %s", 
                g_brightness_config.mode == BRIGHTNESS_MODE_AUTO ? "AUTO" : "MANUAL",
                g_brightness_config.manual_brightness,
                g_brightness_config.min_brightness,
                g_brightness_config.max_brightness,
                g_brightness_config.auto_sleep_enabled ? "ON" : "OFF");

    cleanup:
        xSemaphoreGive(g_brightness_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire brightness mutex for saving config");
        err = ESP_ERR_TIMEOUT;
    }

    return err;
}

// 从NVS加载亮度配置
esp_err_t brightness_load_config_from_nvs(void)
{
    if (!g_task_running) {
        ESP_LOGW(TAG, "Brightness system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_OK;
    bool config_loaded = false;
    
    if (xSemaphoreTake(g_brightness_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        brightness_config_t loaded_config = g_brightness_config; // 备份当前配置
        
        // 加载亮度模式
        uint8_t mode_val;
        if (nvs_load_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_MODE, &mode_val) == ESP_OK) {
            if (mode_val <= BRIGHTNESS_MODE_MANUAL) {
                loaded_config.mode = (brightness_mode_t)mode_val;
                config_loaded = true;
            }
        }

        // 加载手动亮度值
        uint8_t manual_brightness;
        if (nvs_load_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_MANUAL_BRIGHTNESS, &manual_brightness) == ESP_OK) {
            loaded_config.manual_brightness = manual_brightness;
        }

        // 加载最小亮度
        uint8_t min_brightness;
        if (nvs_load_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_MIN_BRIGHTNESS, &min_brightness) == ESP_OK) {
            loaded_config.min_brightness = min_brightness;
        }

        // 加载最大亮度
        uint8_t max_brightness;
        if (nvs_load_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_MAX_BRIGHTNESS, &max_brightness) == ESP_OK) {
            loaded_config.max_brightness = max_brightness;
        }

        // 加载平滑过渡设置
        bool smooth_transition;
        if (nvs_load_bool(BRIGHTNESS_NAMESPACE, NVS_KEY_SMOOTH_TRANSITION, &smooth_transition) == ESP_OK) {
            loaded_config.smooth_transition = smooth_transition;
        }

        // 加载自动息屏设置
        bool auto_sleep_enabled;
        if (nvs_load_bool(BRIGHTNESS_NAMESPACE, NVS_KEY_AUTO_SLEEP_ENABLED, &auto_sleep_enabled) == ESP_OK) {
            loaded_config.auto_sleep_enabled = auto_sleep_enabled;
        }

        // 加载息屏超时时间
        uint32_t sleep_timeout;
        if (nvs_load_u32(BRIGHTNESS_NAMESPACE, NVS_KEY_SLEEP_TIMEOUT, &sleep_timeout) == ESP_OK) {
            loaded_config.sleep_timeout_ms = sleep_timeout;
        }

        // 加载息屏延迟时间
        uint32_t sleep_delay;
        if (nvs_load_u32(BRIGHTNESS_NAMESPACE, NVS_KEY_SLEEP_DELAY, &sleep_delay) == ESP_OK) {
            loaded_config.sleep_delay_ms = sleep_delay;
        }

        // 加载息屏亮度
        uint8_t sleep_brightness;
        if (nvs_load_u8(BRIGHTNESS_NAMESPACE, NVS_KEY_SLEEP_BRIGHTNESS, &sleep_brightness) == ESP_OK) {
            loaded_config.sleep_brightness = sleep_brightness;
        }

        if (config_loaded) {
            // 应用加载的配置
            g_brightness_config = loaded_config;
            
            // 如果是手动模式，设置到保存的手动亮度值
            if (g_brightness_config.mode == BRIGHTNESS_MODE_MANUAL) {
                g_current_brightness = g_brightness_config.manual_brightness;
                
                // 立即应用亮度设置
                auto tft = get_lgfx_tft();
                tft->setBrightness(g_current_brightness);
                
                // 设置LED亮度
                uint32_t duty = (g_current_brightness * 8191) / 255;
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                
                ESP_LOGI(TAG, "Applied manual brightness from NVS: %d", g_current_brightness);
            }
            
            ESP_LOGI(TAG, "Brightness config loaded from NVS - Mode: %s, Manual: %d, Range: %d-%d, AutoSleep: %s", 
                    g_brightness_config.mode == BRIGHTNESS_MODE_AUTO ? "AUTO" : "MANUAL",
                    g_brightness_config.manual_brightness,
                    g_brightness_config.min_brightness,
                    g_brightness_config.max_brightness,
                    g_brightness_config.auto_sleep_enabled ? "ON" : "OFF");
        } else {
            ESP_LOGI(TAG, "No brightness config found in NVS, using defaults");
            err = ESP_ERR_NVS_NOT_FOUND;
        }

        xSemaphoreGive(g_brightness_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire brightness mutex for loading config");
        err = ESP_ERR_TIMEOUT;
    }

    return err;
}

bool brightness_is_screen_sleeping(void)
{
    return g_screen_sleeping || g_in_sleep_transition;
}

bool brightness_is_in_sleep_transition(void)
{
    return g_in_sleep_transition;
}