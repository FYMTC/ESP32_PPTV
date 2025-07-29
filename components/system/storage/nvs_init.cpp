#include "nvs_init.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_timer.h"
#include <string.h>

static const char* nvsTAG = "NVS";

void nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(nvsTAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(nvsTAG, "NVS initialized successfully");
}

time_t load_time_from_nvs()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("time_storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(nvsTAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return 0;
    }

    int64_t saved_time = 0;
    err = nvs_get_i64(nvs_handle, "last_saved_time", &saved_time);
    if (err != ESP_OK)
    {
        ESP_LOGE(nvsTAG, "Failed to load time from NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(nvsTAG, "Time loaded from NVS: %lld", saved_time);
    }

    nvs_close(nvs_handle);
    return (time_t)saved_time;
}

void save_time_to_nvs(time_t time)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("time_storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(nvsTAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_i64(nvs_handle, "last_saved_time", (int64_t)time);
    if (err != ESP_OK)
    {
        ESP_LOGE(nvsTAG, "Failed to save time to NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(nvsTAG, "Time saved to NVS: %lld", (int64_t)time);
    }

    nvs_commit(nvs_handle); // 提交更改
    nvs_close(nvs_handle);
}

// =============================================================================
// 通用NVS操作API实现
// =============================================================================

// 字符串操作
esp_err_t nvs_save_string(const char* namespace_name, const char* key, const char* value)
{
    if (!namespace_name || !key || !value) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_save_string");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to save string to NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        nvs_commit(nvs_handle);
        ESP_LOGD(nvsTAG, "String saved to NVS [%s:%s]: %s", namespace_name, key, value);
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_string(const char* namespace_name, const char* key, char* value, size_t* length)
{
    if (!namespace_name || !key || !value || !length) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_load_string");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_str(nvs_handle, key, value, length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(nvsTAG, "String not found in NVS [%s:%s]", namespace_name, key);
    } else if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to load string from NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        ESP_LOGD(nvsTAG, "String loaded from NVS [%s:%s]: %s", namespace_name, key, value);
    }

    nvs_close(nvs_handle);
    return err;
}

// 8位无符号整数操作
esp_err_t nvs_save_u8(const char* namespace_name, const char* key, uint8_t value)
{
    if (!namespace_name || !key) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_save_u8");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u8(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to save u8 to NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        nvs_commit(nvs_handle);
        ESP_LOGD(nvsTAG, "u8 saved to NVS [%s:%s]: %d", namespace_name, key, value);
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_u8(const char* namespace_name, const char* key, uint8_t* value)
{
    if (!namespace_name || !key || !value) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_load_u8");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_u8(nvs_handle, key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(nvsTAG, "u8 not found in NVS [%s:%s]", namespace_name, key);
    } else if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to load u8 from NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        ESP_LOGD(nvsTAG, "u8 loaded from NVS [%s:%s]: %d", namespace_name, key, *value);
    }

    nvs_close(nvs_handle);
    return err;
}

// 16位无符号整数操作
esp_err_t nvs_save_u16(const char* namespace_name, const char* key, uint16_t value)
{
    if (!namespace_name || !key) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_save_u16");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u16(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to save u16 to NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        nvs_commit(nvs_handle);
        ESP_LOGD(nvsTAG, "u16 saved to NVS [%s:%s]: %d", namespace_name, key, value);
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_u16(const char* namespace_name, const char* key, uint16_t* value)
{
    if (!namespace_name || !key || !value) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_load_u16");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_u16(nvs_handle, key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(nvsTAG, "u16 not found in NVS [%s:%s]", namespace_name, key);
    } else if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to load u16 from NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        ESP_LOGD(nvsTAG, "u16 loaded from NVS [%s:%s]: %d", namespace_name, key, *value);
    }

    nvs_close(nvs_handle);
    return err;
}

// 32位无符号整数操作
esp_err_t nvs_save_u32(const char* namespace_name, const char* key, uint32_t value)
{
    if (!namespace_name || !key) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_save_u32");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to save u32 to NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        nvs_commit(nvs_handle);
        ESP_LOGD(nvsTAG, "u32 saved to NVS [%s:%s]: %lu", namespace_name, key, (unsigned long)value);
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_load_u32(const char* namespace_name, const char* key, uint32_t* value)
{
    if (!namespace_name || !key || !value) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_load_u32");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_get_u32(nvs_handle, key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGD(nvsTAG, "u32 not found in NVS [%s:%s]", namespace_name, key);
    } else if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to load u32 from NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        ESP_LOGD(nvsTAG, "u32 loaded from NVS [%s:%s]: %lu", namespace_name, key, (unsigned long)*value);
    }

    nvs_close(nvs_handle);
    return err;
}

// 布尔操作
esp_err_t nvs_save_bool(const char* namespace_name, const char* key, bool value)
{
    return nvs_save_u8(namespace_name, key, value ? 1 : 0);
}

esp_err_t nvs_load_bool(const char* namespace_name, const char* key, bool* value)
{
    if (!value) return ESP_ERR_INVALID_ARG;
    
    uint8_t val;
    esp_err_t err = nvs_load_u8(namespace_name, key, &val);
    if (err == ESP_OK) {
        *value = (val != 0);
    }
    return err;
}

// 删除操作
esp_err_t nvs_delete_key(const char* namespace_name, const char* key)
{
    if (!namespace_name || !key) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_delete_key");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_key(nvs_handle, key);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to delete key from NVS [%s:%s]: %s", namespace_name, key, esp_err_to_name(err));
    } else {
        nvs_commit(nvs_handle);
        ESP_LOGI(nvsTAG, "Key deleted from NVS [%s:%s]", namespace_name, key);
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_erase_namespace(const char* namespace_name)
{
    if (!namespace_name) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_erase_namespace");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        return err;
    }

    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to erase namespace '%s': %s", namespace_name, esp_err_to_name(err));
    } else {
        nvs_commit(nvs_handle);
        ESP_LOGI(nvsTAG, "Namespace erased: %s", namespace_name);
    }

    nvs_close(nvs_handle);
    return err;
}

// 查询操作
esp_err_t nvs_key_exists(const char* namespace_name, const char* key, bool* exists)
{
    if (!namespace_name || !key || !exists) {
        ESP_LOGE(nvsTAG, "Invalid parameters for nvs_key_exists");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(namespace_name, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(nvsTAG, "Failed to open NVS namespace '%s': %s", namespace_name, esp_err_to_name(err));
        *exists = false;
        return err;
    }

    // 尝试读取一个u8值来检查key是否存在
    // 如果key不存在，会返回ESP_ERR_NVS_NOT_FOUND
    uint8_t dummy_val;
    err = nvs_get_u8(nvs_handle, key, &dummy_val);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *exists = false;
        err = ESP_OK; // 这不是真正的错误，只是key不存在
    } else if (err == ESP_ERR_NVS_INVALID_LENGTH) {
        // key存在但不是u8类型，尝试其他类型
        uint32_t dummy_u32;
        err = nvs_get_u32(nvs_handle, key, &dummy_u32);
        if (err == ESP_OK || err == ESP_ERR_NVS_INVALID_LENGTH) {
            *exists = true;
            err = ESP_OK;
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            *exists = false;
            err = ESP_OK;
        }
    } else if (err == ESP_OK) {
        *exists = true;
    } else {
        *exists = false;
    }
    
    nvs_close(nvs_handle);
    return err;
}