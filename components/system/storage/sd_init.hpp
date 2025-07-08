// components/storage/sd_init.hpp
#pragma once
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#define SD_USE_MMC_HOST 1
//sd mmc 配置
#define SDMMC_CLK_GPIO GPIO_NUM_7
#define SDMMC_CMD_GPIO GPIO_NUM_6
#define SDMMC_DATA0_GPIO GPIO_NUM_8
#define SDMMC_DATA1_GPIO GPIO_NUM_9
#define SDMMC_DATA2_GPIO GPIO_NUM_4
#define SDMMC_DATA3_GPIO GPIO_NUM_5
#define SD_DET_PIN GPIO_NUM_10
//spi 配置
#define SD_MISO_PIN GPIO_NUM_16
#define SD_MOSI_PIN GPIO_NUM_6
#define SD_CLK_PIN GPIO_NUM_2
#define SD_CS_PIN GPIO_NUM_5

#define SDCARD_SPIHOST SPI2_HOST
// #define sdcard_mount_point "/sdcard"
#define sdcard_mount_point "/:"
static const char* SD_TAG = "SD";
TaskHandle_t GPIOtask_handle;
QueueHandle_t gpio_evt_queue = NULL;
sdmmc_card_t *card = NULL;
static SemaphoreHandle_t sd_mutex = nullptr;
void sd_init();
void mount_sd_card()
{
    if (sd_mutex) xSemaphoreTake(sd_mutex, portMAX_DELAY);
#if SD_USE_MMC_HOST
    esp_err_t ret;

    // 配置SD/MMC主机
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // 自定义SD/MMC插槽引脚配置
    sdmmc_slot_config_t slot_config = {
        .clk = SDMMC_CLK_GPIO,  // CLK信号引脚
        .cmd = SDMMC_CMD_GPIO,  // CMD信号引脚
        .d0 = SDMMC_DATA0_GPIO, // D0信号引脚
        .d1 = SDMMC_DATA1_GPIO, // D1信号引脚 (4线模式)
        .d2 = SDMMC_DATA2_GPIO, // D2信号引脚 (4线模式)
        .d3 = SDMMC_DATA3_GPIO, // D3信号引脚 (4线模式)
        .cd = SD_DET_PIN,       // 卡检测引脚
        .wp = SDMMC_SLOT_NO_WP, // 不使用写保护引脚
        .width = 4,             // 总线宽度 (1或4)
        .flags = 0,             // 额外标志
    };

    // 挂载文件系统
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
    #if ESP_IDF_VERSION_MAJOR >= 4
            .disk_status_check_enable = false
    #endif
        };

    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdmmc_mount(sdcard_mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(SD_TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        }
        else
        {
            ESP_LOGE(SD_TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }

    // 打印SD/MMC卡信息
    sdmmc_card_print_info(stdout, card);
#else
    if (!spi_initialized)
    {
        initialize_spi_bus();
    }

    // 配置 SDMMC 主机
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // 配置 SDSPI 设备
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;      // GPIO_NUM_5
    slot_config.host_id = SDCARD_SPIHOST; // 使用 SPI2 主机

    // 配置挂载参数
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // 挂载 SD 卡
    esp_err_t ret = esp_vfs_fat_sdspi_mount(sdcard_mount_point, &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK)
    {
        ESP_LOGI(SD_TAG, "SD card mounted successfully");

        if (card)
        {
            // 输出 SD 卡基本信息
            sdmmc_card_print_info(stdout, card);
        }
    }
    else
    {
        ESP_LOGE(SD_TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    }

#endif
    if (sd_mutex) xSemaphoreGive(sd_mutex);
}

void unmount_sd_card()
{
    if (sd_mutex) xSemaphoreTake(sd_mutex, portMAX_DELAY);
#if SD_USE_MMC_HOST
    // 卸载SD/MMC卡
    esp_vfs_fat_sdcard_unmount(sdcard_mount_point, card);
    ESP_LOGI(SD_TAG, "Card unmounted");
#else
    deinitialize_spi_bus();
    if (card)
    {
        esp_vfs_fat_sdcard_unmount(sdcard_mount_point, card);
        card = NULL;
        ESP_LOGI(SD_TAG, "SD card unmounted");
    }
#endif
    if (sd_mutex) xSemaphoreGive(sd_mutex);
}
// components/storage/sd_init.cpp

void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken) != pdPASS)
    {
        ESP_LOGW(SD_TAG, "Failed to send data to queue");
    }
    // if (xHigherPriorityTaskWoken) {
    //     portYIELD_FROM_ISR();
    // }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void SD_gpio_task(void *arg)
{
    uint32_t io_num;
    bool sd_inserted = false;

    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(1000)))
        {
            bool current_state = gpio_get_level(SD_DET_PIN) == 0;
            ESP_LOGI(SD_TAG, "GPIO event received, state: %d", current_state);

            if (current_state && !sd_inserted)
            {
                ESP_LOGI(SD_TAG, "SD card inserted");
                mount_sd_card();
                // 检查是否 mount 成功再设置 sd_inserted
                if (card != NULL) {
                    sd_inserted = true;
                }
            }
            else if (!current_state && sd_inserted)
            {
                ESP_LOGI(SD_TAG, "SD card removed");
                unmount_sd_card();
                sd_inserted = false;
            }
        }
        else
        {
            // ESP_LOGW(SD_TAG, "Timeout waiting for GPIO event");
        }
    }
}
void sd_init() {
     // 配置 SD_DET_PIN 检测引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << SD_DET_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE};
    gpio_config(&io_conf);

    // 创建 GPIO 事件队列
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (gpio_evt_queue == NULL)
    {
        ESP_LOGE(SD_TAG, "Failed to create queue");
        return;
    }

    // 创建 GPIO 任务
    ESP_LOGI(SD_TAG, "xTaskCreate SD_gpio_task");
    xTaskCreatePinnedToCore(SD_gpio_task, "SD DET task", 1024 * 3, NULL, 1, &GPIOtask_handle, 1);

    // 安装 GPIO 中断服务
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SD_DET_PIN, gpio_isr_handler, (void *)SD_DET_PIN);

    // 上电时检查 SD_DET_PIN 状态
    bool sd_detected = gpio_get_level(SD_DET_PIN) == 0; // 低电平表示 SD 卡插入
    if (sd_detected)
    {
        ESP_LOGI(SD_TAG, "SD card detected on boot");
        uint32_t gpio_num = SD_DET_PIN;
        xQueueSend(gpio_evt_queue, &gpio_num, 0); // 直接投递事件到队列
    }

    if (!sd_mutex) {
        sd_mutex = xSemaphoreCreateMutex();
    }
}