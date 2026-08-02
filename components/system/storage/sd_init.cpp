#include "sd_init.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cstring>

// 日志标签
static const char* SD_TAG = "SD";

// 全局变量定义
TaskHandle_t GPIOtask_handle = nullptr;
QueueHandle_t gpio_evt_queue = nullptr;
sdmmc_card_t *card = nullptr;
static SemaphoreHandle_t sd_mutex = nullptr;

// SPI总线初始化（仅在SPI模式下使用）
#if !SD_USE_MMC_HOST
static bool spi_initialized = false;
static void initialize_spi_bus()
{
    if (spi_initialized) return;
    
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_PIN,
        .miso_io_num = SD_MISO_PIN,
        .sclk_io_num = SD_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    esp_err_t ret = spi_bus_initialize(SDCARD_SPIHOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(SD_TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }
    
    spi_initialized = true;
    ESP_LOGI(SD_TAG, "SPI bus initialized");
}

static void deinitialize_spi_bus()
{
    if (!spi_initialized) return;
    
    spi_bus_free(SDCARD_SPIHOST);
    spi_initialized = false;
    ESP_LOGI(SD_TAG, "SPI bus deinitialized");
}
#endif

void mount_sd_card()
{
    if (sd_mutex) xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
#if SD_USE_MMC_HOST
    esp_err_t ret;

    // 配置SD/MMC主机
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // 自定义SD/MMC插槽引脚配置
    sdmmc_slot_config_t slot_config = {
        .clk = SDMMC_CLK_GPIO,
        .cmd = SDMMC_CMD_GPIO,
        .d0 = SDMMC_DATA0_GPIO,
        .d1 = SDMMC_DATA1_GPIO,
        .d2 = SDMMC_DATA2_GPIO,
        .d3 = SDMMC_DATA3_GPIO,
        .d4 = GPIO_NUM_NC,
        .d5 = GPIO_NUM_NC,
        .d6 = GPIO_NUM_NC,
        .d7 = GPIO_NUM_NC,
        .cd = SD_DET_PIN,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };

    // 挂载文件系统
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024,
#if ESP_IDF_VERSION_MAJOR >= 4
        .disk_status_check_enable = false,
        .use_one_fat = false
#endif
    };

    ret = esp_vfs_fat_sdmmc_mount(sdcard_mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SD_TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(SD_TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        if (sd_mutex) xSemaphoreGive(sd_mutex);
        return;
    }

    ESP_LOGI(SD_TAG, "SD card mounted successfully");
    // 打印SD/MMC卡信息
    if (card) {
        sdmmc_card_print_info(stdout, card);
    }
#else
    if (!spi_initialized) {
        initialize_spi_bus();
    }

    // 配置 SDMMC 主机
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // 配置 SDSPI 设备
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;
    slot_config.host_id = SDCARD_SPIHOST;

    // 配置挂载参数
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };

    // 挂载 SD 卡
    esp_err_t ret = esp_vfs_fat_sdspi_mount(sdcard_mount_point, &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK) {
        ESP_LOGI(SD_TAG, "SD card mounted successfully");
        if (card) {
            sdmmc_card_print_info(stdout, card);
        }
    } else {
        ESP_LOGE(SD_TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    }
#endif
    
    if (sd_mutex) xSemaphoreGive(sd_mutex);
}

void unmount_sd_card()
{
    if (sd_mutex) xSemaphoreTake(sd_mutex, portMAX_DELAY);
    
#if SD_USE_MMC_HOST
    if (card) {
        esp_vfs_fat_sdcard_unmount(sdcard_mount_point, card);
        card = nullptr;
        ESP_LOGI(SD_TAG, "SD card unmounted");
    }
#else
    if (card) {
        esp_vfs_fat_sdcard_unmount(sdcard_mount_point, card);
        card = nullptr;
        ESP_LOGI(SD_TAG, "SD card unmounted");
    }
    deinitialize_spi_bus();
#endif
    
    if (sd_mutex) xSemaphoreGive(sd_mutex);
}

void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken) != pdPASS) {
        ESP_LOGW(SD_TAG, "Failed to send data to queue");
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void SD_gpio_task(void *arg)
{
    uint32_t io_num;
    bool sd_inserted = false;

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(1000))) {
            // 添加防抖延时
            vTaskDelay(pdMS_TO_TICKS(50));
            
            bool current_state = gpio_get_level(SD_DET_PIN) == 0;
            ESP_LOGI(SD_TAG, "GPIO event received, state: %d", current_state);

            if (current_state && !sd_inserted) {
                ESP_LOGI(SD_TAG, "SD card inserted");
                mount_sd_card();
                // 检查是否 mount 成功再设置 sd_inserted
                if (card != nullptr) {
                    sd_inserted = true;
                }
            } else if (!current_state && sd_inserted) {
                ESP_LOGI(SD_TAG, "SD card removed");
                unmount_sd_card();
                sd_inserted = false;
            }
        }
    }
}

void sd_init()
{
    // 创建互斥锁
    if (!sd_mutex) {
        sd_mutex = xSemaphoreCreateMutex();
        if (!sd_mutex) {
            ESP_LOGE(SD_TAG, "Failed to create SD mutex");
            return;
        }
    }

    // 配置 SD_DET_PIN 检测引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << SD_DET_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf);

    // 创建 GPIO 事件队列
    if (!gpio_evt_queue) {
        gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
        if (gpio_evt_queue == nullptr) {
            ESP_LOGE(SD_TAG, "Failed to create queue");
            return;
        }
    }

    // 创建 GPIO 任务
    if (!GPIOtask_handle) {
        ESP_LOGI(SD_TAG, "Creating SD detection task");
        xTaskCreatePinnedToCore(SD_gpio_task, "SD_DET_task", 4096, nullptr, 5, &GPIOtask_handle, 1);
    }

    // 安装 GPIO 中断服务
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SD_DET_PIN, gpio_isr_handler, (void *)SD_DET_PIN);

    // 上电时检查 SD_DET_PIN 状态
    bool sd_detected = gpio_get_level(SD_DET_PIN) == 0;
    if (sd_detected) {
        ESP_LOGI(SD_TAG, "SD card detected on boot");
        uint32_t gpio_num = SD_DET_PIN;
        xQueueSend(gpio_evt_queue, &gpio_num, 0);
    }

    ESP_LOGI(SD_TAG, "SD card detection initialized");
}

// SD卡状态查询API
bool is_sd_card_mounted()
{
    bool mounted = false;
    if (sd_mutex && xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        mounted = (card != nullptr);
        xSemaphoreGive(sd_mutex);
    }
    return mounted;
}

esp_err_t get_sd_card_info(sd_card_info_t *info)
{
    if (!info) return ESP_ERR_INVALID_ARG;
    
    if (sd_mutex && xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (card) {
            snprintf(info->name, sizeof(info->name), "%s", card->cid.name);
            info->capacity_mb = (uint32_t)((uint64_t)card->csd.capacity * card->csd.sector_size / 1024 / 1024);
            info->sector_size = card->csd.sector_size;
            info->is_mounted = true;
        } else {
            memset(info, 0, sizeof(sd_card_info_t));
            info->is_mounted = false;
        }
        xSemaphoreGive(sd_mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}
