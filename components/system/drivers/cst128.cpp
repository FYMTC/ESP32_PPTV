#include "cst128.h"
#include "i2cdev.h"

static const char *TAG = "CST128";
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO               (gpio_num_t)17
#define I2C_MASTER_SCL_IO               (gpio_num_t)18
#define I2C_MASTER_FREQ_HZ              100000

static i2c_dev_t s_cst128_dev = {};

static esp_err_t cst128_read_register(cst128_dev_t *dev, uint8_t reg, uint8_t *data, size_t len)
{
    s_cst128_dev.port = dev->i2c_port;
    s_cst128_dev.addr = CST128_I2C_ADDR;
    s_cst128_dev.cfg.sda_io_num = I2C_MASTER_SDA_IO;
    s_cst128_dev.cfg.scl_io_num = I2C_MASTER_SCL_IO;
    s_cst128_dev.cfg.master.clk_speed = I2C_MASTER_FREQ_HZ;
    s_cst128_dev.cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    s_cst128_dev.cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    return i2c_dev_read_reg(&s_cst128_dev, reg, data, len);
}

esp_err_t cst128_init(cst128_dev_t *dev)
{
    esp_err_t ret;

    return ESP_OK;
}

esp_err_t cst128_read_touch(cst128_dev_t *dev, touch_result_t *result)
{
    cst128_reg_t reg_data;
    esp_err_t ret;

    ret = cst128_read_register(dev, 0x02, (uint8_t *)&reg_data, sizeof(reg_data));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read touch data");
        return ret;
    }

    int finger_num = reg_data.finger_num & 0x0F;
    if (finger_num > TOUCH_MAX_POINT_NUMBER)
    {
        ESP_LOGW(TAG, "Touch point %d > max %d", finger_num, TOUCH_MAX_POINT_NUMBER);
        finger_num = TOUCH_MAX_POINT_NUMBER;
    }
    result->point_num = finger_num;

    for (int i = 0; i < finger_num; i++)
    {
        uint16_t point_x = ((reg_data.pos[i].xh & 0x0F) << 8) | reg_data.pos[i].xl;
        uint16_t point_y = ((reg_data.pos[i].yh & 0x0F) << 8) | reg_data.pos[i].yl;

        if (point_x > dev->range_x || point_y > dev->range_y)
        {
            continue;
        }

        result->point[i].x_coordinate = point_x;
        result->point[i].y_coordinate = point_y;
        result->point[i].track_id = i;
        result->point[i].width = finger_num;
        result->point[i].event = 0; // RT_TOUCH_EVENT_NONE
        result->point[i].timestamp = xTaskGetTickCount();
    }

    return ESP_OK;
}