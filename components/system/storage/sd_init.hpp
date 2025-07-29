#pragma once

#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// 配置选择：1=使用MMC主机模式，0=使用SPI模式
#define SD_USE_MMC_HOST 1

// SD MMC 配置引脚
#define SDMMC_CLK_GPIO GPIO_NUM_7
#define SDMMC_CMD_GPIO GPIO_NUM_6
#define SDMMC_DATA0_GPIO GPIO_NUM_8
#define SDMMC_DATA1_GPIO GPIO_NUM_9
#define SDMMC_DATA2_GPIO GPIO_NUM_4
#define SDMMC_DATA3_GPIO GPIO_NUM_5
#define SD_DET_PIN GPIO_NUM_10

// SPI 配置引脚
#define SD_MISO_PIN GPIO_NUM_16
#define SD_MOSI_PIN GPIO_NUM_6
#define SD_CLK_PIN GPIO_NUM_2
#define SD_CS_PIN GPIO_NUM_5
#define SDCARD_SPIHOST SPI2_HOST

// 挂载点
#define sdcard_mount_point "/:"

// SD卡信息结构体
typedef struct {
    char name[16];
    uint32_t capacity_mb;
    uint32_t sector_size;
    bool is_mounted;
} sd_card_info_t;

// 外部变量声明
extern TaskHandle_t GPIOtask_handle;
extern QueueHandle_t gpio_evt_queue;
extern sdmmc_card_t *card;

// 函数声明
void sd_init();
void mount_sd_card();
void unmount_sd_card();
bool is_sd_card_mounted();
esp_err_t get_sd_card_info(sd_card_info_t *info);

// 内部任务和中断处理函数声明
void IRAM_ATTR gpio_isr_handler(void *arg);
void SD_gpio_task(void *arg);