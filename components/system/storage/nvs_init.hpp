#pragma once
#include <time.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// NVS初始化
void nvs_init();

// 时间相关API（保持向后兼容）
time_t load_time_from_nvs();
void save_time_to_nvs(time_t time);

// 通用NVS操作API
// 字符串操作
esp_err_t nvs_save_string(const char* namespace_name, const char* key, const char* value);
esp_err_t nvs_load_string(const char* namespace_name, const char* key, char* value, size_t* length);
esp_err_t nvs_delete_string(const char* namespace_name, const char* key);

// 整数操作
esp_err_t nvs_save_u8(const char* namespace_name, const char* key, uint8_t value);
esp_err_t nvs_load_u8(const char* namespace_name, const char* key, uint8_t* value);
esp_err_t nvs_save_u16(const char* namespace_name, const char* key, uint16_t value);
esp_err_t nvs_load_u16(const char* namespace_name, const char* key, uint16_t* value);
esp_err_t nvs_save_u32(const char* namespace_name, const char* key, uint32_t value);
esp_err_t nvs_load_u32(const char* namespace_name, const char* key, uint32_t* value);
esp_err_t nvs_save_i32(const char* namespace_name, const char* key, int32_t value);
esp_err_t nvs_load_i32(const char* namespace_name, const char* key, int32_t* value);

// 布尔操作
esp_err_t nvs_save_bool(const char* namespace_name, const char* key, bool value);
esp_err_t nvs_load_bool(const char* namespace_name, const char* key, bool* value);

// 删除操作
esp_err_t nvs_delete_key(const char* namespace_name, const char* key);
esp_err_t nvs_erase_namespace(const char* namespace_name);

// 查询操作
esp_err_t nvs_key_exists(const char* namespace_name, const char* key, bool* exists);
esp_err_t nvs_get_namespace_stats(const char* namespace_name, size_t* used_entries, size_t* total_entries);