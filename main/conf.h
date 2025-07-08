#pragma once

#define USE_BORAD_1 0

#if USE_BORAD_1

/******************************************************************************/
/***************************  I2C ↓ *******************************************/

#define I2C_MASTER_SCL_IO_0 GPIO_NUM_1  // GPIO for SCL (Bus 0)
#define I2C_MASTER_SDA_IO_0 GPIO_NUM_2  // GPIO for SDA (Bus 0)
#define I2C_MASTER_SCL_IO_1 GPIO_NUM_12 // GPIO for SCL (Bus 1)
#define I2C_MASTER_SDA_IO_1 GPIO_NUM_11 // GPIO for SDA (Bus 1)
#define I2C_MASTER_NUM_0 I2C_NUM_0
#define I2C_MASTER_NUM_1 I2C_NUM_1
#define I2C_MASTER_FREQ_HZ 100000

/***********************************************************/
/**********************    SD卡 ↓   *********************/
#define SD_DET_PIN GPIO_NUM_17
#define SD_MISO_PIN GPIO_NUM_16
#define SD_MOSI_PIN GPIO_NUM_6
#define SD_CLK_PIN GPIO_NUM_15
#define SD_CS_PIN GPIO_NUM_5

#define SDCARD_SPIHOST SPI2_HOST

/******************************************************************************/
/***************************  LVGL gamed ↓ *************************************/
#define cube_pre_x 20
#define cube_pre_y 50
#define cube_win_x 100
#define cube_win_y 9
#define cube_size 12

/******************************************************************************/
/***************************  scream ↓ ************************************/
#define MY_DISP_HOR_RES 240 // width
#define MY_DISP_VER_RES 280 // height

#define USE_LGFX 1
#define USE_eTFT 0

#define USE_PSRAM_FOR_BUFFER 0
#define use_buf_dsc_2 0 // fps34
#define use_buf_dsc_3 1 // fps45

#define disp_sclk 14
#define disp_mosi 13
#define disp_dc 21
#define disp_rst 10
#define disp_offset_y 20
#define DISP_HOR_RES 240
#define DISP_VER_RES 320
#define DISP_bl_pin 48

#else

#define I2C_MASTER_SCL_IO_0 GPIO_NUM_1  // GPIO for SCL (Bus 0)
#define I2C_MASTER_SDA_IO_0 GPIO_NUM_2  // GPIO for SDA (Bus 0)
#define I2C_MASTER_SCL_IO_1 GPIO_NUM_41 // GPIO for SCL (Bus 1)
#define I2C_MASTER_SDA_IO_1 GPIO_NUM_40 // GPIO for SDA (Bus 1)
#define I2C_MASTER_NUM_0 I2C_NUM_0
#define I2C_MASTER_NUM_1 I2C_NUM_1
#define I2C_MASTER_FREQ_HZ 100000

#define MAX_TASK_NAME_LEN 32

/***********************************************************/
/**********************    SD卡 ↓   *********************/
#define SD_USE_MMC_HOST 1
#define SDMMC_CLK_GPIO GPIO_NUM_2
#define SDMMC_CMD_GPIO GPIO_NUM_6
#define SDMMC_DATA0_GPIO GPIO_NUM_16
#define SDMMC_DATA1_GPIO GPIO_NUM_17
#define SDMMC_DATA2_GPIO GPIO_NUM_4
#define SDMMC_DATA3_GPIO GPIO_NUM_5

#define SD_DET_PIN GPIO_NUM_18

#define SD_MISO_PIN GPIO_NUM_16
#define SD_MOSI_PIN GPIO_NUM_6
#define SD_CLK_PIN GPIO_NUM_2
#define SD_CS_PIN GPIO_NUM_5

#define SDCARD_SPIHOST SPI2_HOST
// #define sdcard_mount_point "/sdcard"
#define sdcard_mount_point "/:"

/******************************************************************************/
/***************************  LVGL gamed ↓ *************************************/


/******************************************************************************/
/***************************  scream ↓ ************************************/
#define MY_DISP_HOR_RES 240 // width
#define MY_DISP_VER_RES 320 // height

#define USE_LGFX 1
#define USE_eTFT 0

#define USE_PSRAM_FOR_BUFFER 1
#define use_buf_dsc_2 0 // fps34
#define use_buf_dsc_3 1 // fps45
#define cfgspi_mode 3
#define disp_sclk 1
#define disp_mosi 8
#define disp_dc 3
#define disp_rst 7 
#define disp_cs 9  //不可少
#define disp_offset_y 0
#define DISP_HOR_RES 240
#define DISP_VER_RES 320
#define DISP_bl_pin 46

#define cfgreadable false;
#define cfginvert true;
#define cfgrgb_order false;
#define cfgdlen_16bit false;
#define cfgbus_shared false;

#define USE_TOUCHPAD 0
#define USE_MOUSE 1

#define BRIGHTNESS_KEY "brightness"
#define WIFI_STATUS_KEY "WIFI_STATUS"
#define MY_FONT_MOUSE  "\xEF\x89\x85"
#define MY_FONT_TIME  "\xEF\x8B\xB2"

#define TOUCH_PAD_NUM GPIO_NUM_14

#define LED_STRIP_GPIO_PIN  48
#define LED_STRIP_LED_COUNT 1

//#define USE_I2S_AUDIO 0
#define USE_UAC_AUDIO 1


#endif