#include "esp_adc/adc_oneshot.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "LGFX_disp.hpp"

#define DEFAULT_VREF 3300 // 默认参考电压 (mV)
#define NO_OF_SAMPLES 10  // 采样次数

#define LED_PIN GPIO_NUM_48             // LED 连接的 GPIO 引脚
#define LEDC_CHANNEL LEDC_CHANNEL_0     // LEDC 通道
#define LEDC_TIMER LEDC_TIMER_0         // LEDC 定时器
#define LEDC_MODE LEDC_LOW_SPEED_MODE   // LEDC 模式
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // 13 位分辨率（0-8191）

int MAX_BRIGHTNESS = 0; // 最大亮度ADC值

// 定义滤波器参数
#define FILTER_WINDOW_SIZE 100 // 滤波器窗口大小（历史值的数量）
uint8_t brightness_history[FILTER_WINDOW_SIZE]; // 环形缓冲区，存储历史亮度值
int history_index = 0; // 当前缓冲区索引
uint8_t filtered_brightness = 0; // 滤波后的亮度值

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
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1, // 使用 ADC1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    // ADC 通道配置
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_11,    // 设置衰减（11dB 适用于 0-3.3V 范围）
        .bitwidth = ADC_BITWIDTH_12, // 12 位分辨率
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &channel_config)); // GPIO1 对应 ADC1_CHANNEL_0

    // 初始化 LEDC PWM
    init_ledc();

    // 初始化亮度历史缓冲区
    for (int i = 0; i < FILTER_WINDOW_SIZE; i++)
    {
        brightness_history[i] = 0;
    }

    while (1)
    {
        int adc_reading = 0;
        // 多次采样取平均
        for (int i = 0; i < NO_OF_SAMPLES; i++)
        {
            int raw_value;
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw_value));
            adc_reading += raw_value;
        }
        adc_reading /= NO_OF_SAMPLES;

        if (adc_reading > MAX_BRIGHTNESS)
        {
            MAX_BRIGHTNESS = adc_reading;
        }

        // 将 ADC 读数转换为电压（单位：mV）
        //uint32_t voltage = adc_reading * DEFAULT_VREF / 4095; // 12 位 ADC，最大值为 4095
        //ESP_LOGI(TAG, "Raw: %d\tVoltage: %lumV", adc_reading, voltage);

        // 根据 ADC 读数调整 LED 亮度
        // 暗光时 ADC 读数小，LED 亮度低；亮光时 ADC 读数大，LED 亮度高
        uint32_t duty = (adc_reading * 8191) / MAX_BRIGHTNESS; // 将 ADC 读数映射到 0-8191 的占空比范围
        if (duty > 8191)
        {
            duty = 8191;
        }
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

        uint8_t brightness = (uint8_t)(duty * 255 / 8191); // 将占空比转换为 0-255 的亮度值
        if(brightness < 12)
        {
            brightness = 12;
        }

        // 应用移动平均滤波器
        filtered_brightness = apply_moving_average_filter(brightness);

        // 使用滤波后的亮度值设置 TFT 显示亮度
        auto tft = get_lgfx_tft();
        tft->setBrightness(filtered_brightness);

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 清理 ADC 资源
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}