#include "nvs_init.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_timer.h"

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