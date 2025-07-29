#include "cst128.h"

static const char *TAG = "CST128";
#define I2C_MASTER_NUM I2C_NUM_0

static esp_err_t cst128_read_register(cst128_dev_t *dev, uint8_t reg, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST128_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST128_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1)
    {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
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