/*
 * QMC5883L三轴磁力计传感器服务实现文件
 * 提供QMC5883L数据读取和管理功能
 */

#include "qmc5883l_service.h"
#include "i2c_init.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include "i2cdev.h"
#include <string.h>
#include <math.h>

static const char* TAG = "qmc5883l_service";

// 静态变量
static qmc5883l_t s_qmc5883l_dev = {};
static qmc5883l_service_data_t s_current_data = {};
static qmc5883l_status_t s_status = {};
static qmc5883l_data_callback_t s_data_callback = NULL;
static TimerHandle_t s_update_timer = NULL;
static bool s_service_initialized = false;
static bool s_is_running = false;

// 校准参数
static float s_offset_x = 0.0f;
static float s_offset_y = 0.0f;
static float s_offset_z = 0.0f;
static float s_scale_x = 1.0f;
static float s_scale_y = 1.0f;
static float s_scale_z = 1.0f;

// 初始数据丢弃计数
static int s_discard_count = 0;
#define INITIAL_DISCARD_COUNT 20  // 丢弃前20个读数

// 前向声明
static void qmc5883l_update_timer_callback(TimerHandle_t xTimer);
static esp_err_t qmc5883l_read_sensor_data(void);
static float calculate_heading(float mag_x, float mag_y);

/**
 * @brief 初始化QMC5883L服务
 */
esp_err_t qmc5883l_service_init(void)
{
    if (s_service_initialized) {
        ESP_LOGW(TAG, "QMC5883L service already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing QMC5883L service...");

    // 初始化I2C设备描述符（使用正确的引脚配置）
    esp_err_t ret = qmc5883l_init_desc(&s_qmc5883l_dev, QMC5883L_I2C_ADDR_DEF, I2C_NUM_0, GPIO_NUM_17, GPIO_NUM_18);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize QMC5883L descriptor: %s", esp_err_to_name(ret));
        s_status.is_connected = false;
        s_status.error_count++;
        return ret;
    }

    // 重置设备
    ret = qmc5883l_reset(&s_qmc5883l_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset QMC5883L: %s", esp_err_to_name(ret));
        s_status.is_connected = false;
        s_status.error_count++;
        return ret;
    }

    // 等待重置完成并稳定
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "QMC5883L reset completed");

    // 检查芯片ID
    uint8_t chip_id = 0;
    ret = qmc5883l_get_chip_id(&s_qmc5883l_dev, &chip_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read chip ID: %s", esp_err_to_name(ret));
        s_status.is_connected = false;
        s_status.error_count++;
        return ret;
    }
    ESP_LOGI(TAG, "QMC5883L chip ID: 0x%02X", chip_id);

    // 配置QMC5883L - 使用更稳定的配置
    // 降低数据率到10Hz，增加过采样到512，使用±8G量程以减少饱和
    ret = qmc5883l_set_config(&s_qmc5883l_dev, QMC5883L_DR_10, QMC5883L_OSR_512, QMC5883L_RNG_8);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure QMC5883L: %s", esp_err_to_name(ret));
        s_status.is_connected = false;
        s_status.error_count++;
        return ret;
    }
    ESP_LOGI(TAG, "QMC5883L configured: 10Hz, 512 samples, ±8G range");

    // 设置为连续测量模式
    ret = qmc5883l_set_mode(&s_qmc5883l_dev, QMC5883L_MODE_CONTINUOUS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set continuous mode: %s", esp_err_to_name(ret));
        s_status.is_connected = false;
        s_status.error_count++;
        return ret;
    }

    // 初始化状态
    s_status.is_initialized = true;
    s_status.is_connected = true;
    s_status.error_count = 0;
    s_status.last_update = 0;

    // 初始化数据
    memset(&s_current_data, 0, sizeof(s_current_data));
    s_current_data.is_valid = false;

    s_service_initialized = true;
    ESP_LOGI(TAG, "QMC5883L service initialized successfully");

    return ESP_OK;
}

/**
 * @brief 反初始化QMC5883L服务
 */
void qmc5883l_service_deinit(void)
{
    if (!s_service_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing QMC5883L service...");

    // 停止服务
    qmc5883l_service_stop();

    // 设置为待机模式
    qmc5883l_set_mode(&s_qmc5883l_dev, QMC5883L_MODE_STANDBY);

    // 释放设备描述符
    qmc5883l_free_desc(&s_qmc5883l_dev);

    // 重置状态
    memset(&s_status, 0, sizeof(s_status));
    memset(&s_current_data, 0, sizeof(s_current_data));
    s_data_callback = NULL;
    s_service_initialized = false;

    ESP_LOGI(TAG, "QMC5883L service deinitialized");
}

/**
 * @brief 启动QMC5883L数据采集
 */
esp_err_t qmc5883l_service_start(uint32_t update_interval_ms)
{
    if (!s_service_initialized) {
        ESP_LOGE(TAG, "QMC5883L service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_is_running) {
        ESP_LOGW(TAG, "QMC5883L service already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting QMC5883L service with %ld ms update interval", update_interval_ms);

    // 重置丢弃计数器
    s_discard_count = 0;
    ESP_LOGI(TAG, "Reset discard counter, will discard first %d readings", INITIAL_DISCARD_COUNT);

    // 创建更新定时器
    s_update_timer = xTimerCreate(
        "qmc5883l_timer",
        pdMS_TO_TICKS(update_interval_ms),
        pdTRUE, // 自动重载
        NULL,
        qmc5883l_update_timer_callback
    );

    if (s_update_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create update timer");
        return ESP_ERR_NO_MEM;
    }

    // 启动定时器
    if (xTimerStart(s_update_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start update timer");
        xTimerDelete(s_update_timer, 0);
        s_update_timer = NULL;
        return ESP_FAIL;
    }

    s_is_running = true;
    ESP_LOGI(TAG, "QMC5883L service started");

    return ESP_OK;
}

/**
 * @brief 停止QMC5883L数据采集
 */
void qmc5883l_service_stop(void)
{
    if (!s_is_running) {
        return;
    }

    ESP_LOGI(TAG, "Stopping QMC5883L service...");

    // 停止并删除定时器
    if (s_update_timer != NULL) {
        xTimerStop(s_update_timer, 0);
        xTimerDelete(s_update_timer, 0);
        s_update_timer = NULL;
    }

    s_is_running = false;
    ESP_LOGI(TAG, "QMC5883L service stopped");
}

/**
 * @brief 获取最新的QMC5883L数据
 */
esp_err_t qmc5883l_service_get_data(qmc5883l_service_data_t* data)
{
    if (!s_service_initialized || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 复制当前数据
    memcpy(data, &s_current_data, sizeof(qmc5883l_service_data_t));
    return ESP_OK;
}

/**
 * @brief 获取QMC5883L服务状态
 */
esp_err_t qmc5883l_service_get_status(qmc5883l_status_t* status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 复制当前状态
    memcpy(status, &s_status, sizeof(qmc5883l_status_t));
    return ESP_OK;
}

/**
 * @brief 注册数据更新回调函数
 */
esp_err_t qmc5883l_service_register_callback(qmc5883l_data_callback_t callback)
{
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_data_callback = callback;
    ESP_LOGI(TAG, "Data callback registered");
    return ESP_OK;
}

/**
 * @brief 取消注册数据更新回调函数
 */
void qmc5883l_service_unregister_callback(void)
{
    s_data_callback = NULL;
    ESP_LOGI(TAG, "Data callback unregistered");
}

/**
 * @brief 校准QMC5883L传感器
 */
esp_err_t qmc5883l_service_calibrate(void)
{
    if (!s_service_initialized) {
        ESP_LOGE(TAG, "QMC5883L service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Calibrating QMC5883L...");
    
    // 简单的偏移校准 - 采集若干样本计算平均值作为偏移
    const int samples = 100;
    float sum_x = 0, sum_y = 0, sum_z = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < samples; i++) {
        qmc5883l_data_t data;
        if (qmc5883l_get_data(&s_qmc5883l_dev, &data) == ESP_OK) {
            sum_x += data.x;
            sum_y += data.y;
            sum_z += data.z;
            valid_samples++;
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 20ms间隔
    }
    
    if (valid_samples > samples / 2) {
        s_offset_x = sum_x / valid_samples;
        s_offset_y = sum_y / valid_samples;
        s_offset_z = sum_z / valid_samples;
        
        ESP_LOGI(TAG, "Calibration completed - Offsets: X=%.2f, Y=%.2f, Z=%.2f", 
                 s_offset_x, s_offset_y, s_offset_z);
    } else {
        ESP_LOGE(TAG, "Calibration failed - insufficient valid samples");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief 复位QMC5883L传感器
 */
esp_err_t qmc5883l_service_reset(void)
{
    if (!s_service_initialized) {
        ESP_LOGE(TAG, "QMC5883L service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Resetting QMC5883L...");

    // 停止服务
    bool was_running = s_is_running;
    if (s_is_running) {
        qmc5883l_service_stop();
    }

    // 重新初始化
    qmc5883l_service_deinit();
    esp_err_t ret = qmc5883l_service_init();
    
    if (ret == ESP_OK && was_running) {
        // 如果之前在运行，重新启动
        qmc5883l_service_start(100); // 默认100ms间隔
    }

    ESP_LOGI(TAG, "QMC5883L reset completed");
    return ret;
}

/**
 * @brief 定时器回调函数，用于更新QMC5883L数据
 */
static void qmc5883l_update_timer_callback(TimerHandle_t xTimer)
{
    if (qmc5883l_read_sensor_data() == ESP_OK) {
        // 调用用户回调函数
        if (s_data_callback != NULL) {
            s_data_callback(&s_current_data);
        }
    }
}

/**
 * @brief 读取传感器数据
 */
static esp_err_t qmc5883l_read_sensor_data(void)
{
    // 检查数据是否准备好
    bool ready = false;
    esp_err_t ret = qmc5883l_data_ready(&s_qmc5883l_dev, &ready);
    if (ret != ESP_OK || !ready) {
        return ESP_ERR_NOT_FOUND;
    }

    // 读取磁场数据
    qmc5883l_data_t raw_data;
    ret = qmc5883l_get_data(&s_qmc5883l_dev, &raw_data);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read magnetometer data: %s", esp_err_to_name(ret));
        s_status.error_count++;
        s_current_data.is_valid = false;
        return ret;
    }

    // 丢弃初始不稳定的数据
    if (s_discard_count < INITIAL_DISCARD_COUNT) {
        s_discard_count++;
        ESP_LOGD(TAG, "Discarding initial unstable data: %d/%d", s_discard_count, INITIAL_DISCARD_COUNT);
        s_current_data.is_valid = false;
        return ESP_ERR_NOT_FINISHED;
    }

    // 检查数据合理性
    float magnitude = sqrtf(raw_data.x * raw_data.x + raw_data.y * raw_data.y + raw_data.z * raw_data.z);
    if (magnitude < 0.1f || magnitude > 10000.0f) {
        ESP_LOGW(TAG, "Magnetic data out of range: X=%.2f, Y=%.2f, Z=%.2f, magnitude=%.2f", 
                 raw_data.x, raw_data.y, raw_data.z, magnitude);
        s_status.error_count++;
        s_current_data.is_valid = false;
        return ESP_ERR_INVALID_RESPONSE;
    }

    // 应用校准参数
    float cal_x = (raw_data.x - s_offset_x) * s_scale_x;
    float cal_y = (raw_data.y - s_offset_y) * s_scale_y;
    float cal_z = (raw_data.z - s_offset_z) * s_scale_z;

    // 更新数据
    s_current_data.mag_x = cal_x;
    s_current_data.mag_y = cal_y;
    s_current_data.mag_z = cal_z;
    s_current_data.heading = calculate_heading(cal_x, cal_y);
    s_current_data.magnitude = sqrtf(cal_x * cal_x + cal_y * cal_y + cal_z * cal_z);
    s_current_data.is_valid = true;
    s_current_data.timestamp = esp_timer_get_time() / 1000; // 转换为毫秒

    // 更新状态
    s_status.last_update = s_current_data.timestamp;

    return ESP_OK;
}

/**
 * @brief 计算航向角
 */
static float calculate_heading(float mag_x, float mag_y)
{
    float heading = atan2f(mag_y, mag_x) * 180.0f / M_PI;
    
    // 转换为0-360度范围
    if (heading < 0) {
        heading += 360.0f;
    }
    
    return heading;
}
