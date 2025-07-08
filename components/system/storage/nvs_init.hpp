// components/storage/nvs_init.hpp
#pragma once

void nvs_init();
// components/storage/nvs_init.cpp
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"

static const char* NVSTAG = "NVS";

void nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(NVSTAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(NVSTAG, "NVS initialized successfully");
}