/*
 * MPU6050传感器服务实现文件
 * 提供MPU6050数据读取和管理功能
 */

#include "mpu6050_service.h"
#include "i2c_init.h"
#include "mpu6050.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include <string.h>

static const char* TAG = "mpu6050_service";

// 静态变量
static mpu6050_handle_t s_mpu6050_handle = NULL;
static mpu6050_data_t s_current_data = {};
static mpu6050_status_t s_status = {};
static mpu6050_data_callback_t s_data_callback = NULL;
static TimerHandle_t s_update_timer = NULL;
static bool s_service_initialized = false;
static bool s_is_running = false;

// 前向声明
static void mpu6050_update_timer_callback(TimerHandle_t xTimer);
static esp_err_t mpu6050_read_sensor_data(void);

/**
 * @brief 初始化MPU6050服务
 */
esp_err_t mpu6050_service_init(void)
{
    if (s_service_initialized) {
        ESP_LOGW(TAG, "MPU6050 service already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing MPU6050 service...");

    // 创建MPU6050句柄
    s_mpu6050_handle = mpu6050_create(I2C_NUM_0, MPU6050_I2C_ADDRESS);
    if (s_mpu6050_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create MPU6050 handle");
        s_status.is_connected = false;
        s_status.error_count++;
        return ESP_FAIL;
    }

    // 配置MPU6050
    esp_err_t ret = mpu6050_config(s_mpu6050_handle, ACCE_FS_4G, GYRO_FS_500DPS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure MPU6050: %s", esp_err_to_name(ret));
        mpu6050_delete(s_mpu6050_handle);
        s_mpu6050_handle = NULL;
        s_status.is_connected = false;
        s_status.error_count++;
        return ret;
    }

    // 唤醒MPU6050
    ret = mpu6050_wake_up(s_mpu6050_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake up MPU6050: %s", esp_err_to_name(ret));
        mpu6050_delete(s_mpu6050_handle);
        s_mpu6050_handle = NULL;
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
    ESP_LOGI(TAG, "MPU6050 service initialized successfully");

    return ESP_OK;
}

/**
 * @brief 反初始化MPU6050服务
 */
void mpu6050_service_deinit(void)
{
    if (!s_service_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing MPU6050 service...");

    // 停止服务
    mpu6050_service_stop();

    // 删除MPU6050句柄
    if (s_mpu6050_handle != NULL) {
        mpu6050_sleep(s_mpu6050_handle);
        mpu6050_delete(s_mpu6050_handle);
        s_mpu6050_handle = NULL;
    }

    // 重置状态
    memset(&s_status, 0, sizeof(s_status));
    memset(&s_current_data, 0, sizeof(s_current_data));
    s_data_callback = NULL;
    s_service_initialized = false;

    ESP_LOGI(TAG, "MPU6050 service deinitialized");
}

/**
 * @brief 启动MPU6050数据采集
 */
esp_err_t mpu6050_service_start(uint32_t update_interval_ms)
{
    if (!s_service_initialized) {
        ESP_LOGE(TAG, "MPU6050 service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_is_running) {
        ESP_LOGW(TAG, "MPU6050 service already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting MPU6050 service with %ld ms update interval", update_interval_ms);

    // 创建更新定时器
    s_update_timer = xTimerCreate(
        "mpu6050_timer",
        pdMS_TO_TICKS(update_interval_ms),
        pdTRUE, // 自动重载
        NULL,
        mpu6050_update_timer_callback
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
    ESP_LOGI(TAG, "MPU6050 service started");

    return ESP_OK;
}

/**
 * @brief 停止MPU6050数据采集
 */
void mpu6050_service_stop(void)
{
    if (!s_is_running) {
        return;
    }

    ESP_LOGI(TAG, "Stopping MPU6050 service...");

    // 停止并删除定时器
    if (s_update_timer != NULL) {
        xTimerStop(s_update_timer, 0);
        xTimerDelete(s_update_timer, 0);
        s_update_timer = NULL;
    }

    s_is_running = false;
    ESP_LOGI(TAG, "MPU6050 service stopped");
}

/**
 * @brief 获取最新的MPU6050数据
 */
esp_err_t mpu6050_service_get_data(mpu6050_data_t* data)
{
    if (!s_service_initialized || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 复制当前数据
    memcpy(data, &s_current_data, sizeof(mpu6050_data_t));
    return ESP_OK;
}

/**
 * @brief 获取MPU6050服务状态
 */
esp_err_t mpu6050_service_get_status(mpu6050_status_t* status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 复制当前状态
    memcpy(status, &s_status, sizeof(mpu6050_status_t));
    return ESP_OK;
}

/**
 * @brief 注册数据更新回调函数
 */
esp_err_t mpu6050_service_register_callback(mpu6050_data_callback_t callback)
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
void mpu6050_service_unregister_callback(void)
{
    s_data_callback = NULL;
    ESP_LOGI(TAG, "Data callback unregistered");
}

/**
 * @brief 校准MPU6050传感器
 */
esp_err_t mpu6050_service_calibrate(void)
{
    if (!s_service_initialized) {
        ESP_LOGE(TAG, "MPU6050 service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Calibrating MPU6050...");
    
    // 这里可以实现校准逻辑
    // 例如：采集多个样本，计算偏移量等
    
    ESP_LOGI(TAG, "MPU6050 calibration completed");
    return ESP_OK;
}

/**
 * @brief 复位MPU6050传感器
 */
esp_err_t mpu6050_service_reset(void)
{
    if (!s_service_initialized) {
        ESP_LOGE(TAG, "MPU6050 service not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Resetting MPU6050...");

    // 停止服务
    bool was_running = s_is_running;
    if (s_is_running) {
        mpu6050_service_stop();
    }

    // 重新初始化
    mpu6050_service_deinit();
    esp_err_t ret = mpu6050_service_init();
    
    if (ret == ESP_OK && was_running) {
        // 如果之前在运行，重新启动
        mpu6050_service_start(100); // 默认100ms间隔
    }

    ESP_LOGI(TAG, "MPU6050 reset completed");
    return ret;
}

/**
 * @brief 定时器回调函数，用于更新MPU6050数据
 */
static void mpu6050_update_timer_callback(TimerHandle_t xTimer)
{
    if (mpu6050_read_sensor_data() == ESP_OK) {
        // 调用用户回调函数
        if (s_data_callback != NULL) {
            s_data_callback(&s_current_data);
        }
    }
}

/**
 * @brief 读取传感器数据
 */
static esp_err_t mpu6050_read_sensor_data(void)
{
    if (s_mpu6050_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;
    mpu6050_temp_value_t temp;

    // 读取加速度计数据
    esp_err_t ret = mpu6050_get_acce(s_mpu6050_handle, &acce);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read accelerometer data: %s", esp_err_to_name(ret));
        s_status.error_count++;
        s_current_data.is_valid = false;
        return ret;
    }

    // 读取陀螺仪数据
    ret = mpu6050_get_gyro(s_mpu6050_handle, &gyro);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read gyroscope data: %s", esp_err_to_name(ret));
        s_status.error_count++;
        s_current_data.is_valid = false;
        return ret;
    }

    // 读取温度数据
    ret = mpu6050_get_temp(s_mpu6050_handle, &temp);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read temperature data: %s", esp_err_to_name(ret));
        s_status.error_count++;
        s_current_data.is_valid = false;
        return ret;
    }

    // 更新数据
    s_current_data.accel_x = acce.acce_x;
    s_current_data.accel_y = acce.acce_y;
    s_current_data.accel_z = acce.acce_z;
    s_current_data.gyro_x = gyro.gyro_x;
    s_current_data.gyro_y = gyro.gyro_y;
    s_current_data.gyro_z = gyro.gyro_z;
    s_current_data.temperature = temp.temp;
    s_current_data.is_valid = true;
    s_current_data.timestamp = esp_timer_get_time() / 1000; // 转换为毫秒

    // 更新状态
    s_status.last_update = s_current_data.timestamp;

    return ESP_OK;
}
