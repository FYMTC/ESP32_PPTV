#include "init_tasks.hpp"
#include "init.hpp"
#include "nvs_init.hpp"
#include "sd_init.hpp"
#include "i2c_init.h"
#include "lvgl.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "wifi_manager.h"
#include "time_service.h"
#include "battery_service.h"
#include "rtc_ds1302.h"

void register_init_tasks()
{
    auto &initializer = SystemInitializer::instance();

    // 早期初始化
    initializer.add_task(InitStage::EARLY, "CPU Cache", []
                         {
                             // 启用CPU缓存等底层设置
                         },
                         true);

    // 核心子系统
    initializer.add_task(InitStage::CORE, "NVS", nvs_init, true);

    // 驱动程序
    initializer.add_task(InitStage::DRIVERS, "I2C", []
                         {
        i2c_init();
        i2c_scan_result_t devices = scan_i2c_devices();
        if (devices.count == 0) {
            ESP_LOGW("I2C", "No I2C devices found");
        } else {
            ESP_LOGI("I2C", "Found %d I2C devices", devices.count);
        } });
    initializer.add_task(InitStage::DRIVERS, "LVGL", []()
                         {
                            lv_init();
                            lv_port_disp_init(); 
                            lv_port_indev_init();//绝不能在 lv_port_disp_init() 之前调用 lv_port_indev_init() 或创建 LVGL 对象

                            ESP_LOGI("LVGL", "LVGL initialized successfully");
                        }, true);

    initializer.add_task(InitStage::DRIVERS, "SD Card", sd_init);
    
    initializer.add_task(InitStage::DRIVERS, "RTC DS1302", []
                         {
                             esp_err_t ret = rtc_ds1302_init();//初始化RTC芯片
                             if (ret != ESP_OK) {
                                 ESP_LOGE("RTC", "DS1302 initialization failed: %s", esp_err_to_name(ret));
                             } else {
                                 ESP_LOGI("RTC", "DS1302 initialized successfully");
                             }
                         });

    // 服务层
    initializer.add_task(InitStage::SERVICES, "Filesystem", []
                         {
                             //TODO: 挂载文件系统等
                         });
    
    initializer.add_task(InitStage::SERVICES, "Time Service", [] //初始化时间服务
                         {
                             time_service::init();
                         }, true);
    
    initializer.add_task(InitStage::SERVICES, "Battery Service", [] //初始化电池服务
                         {
                             battery_service::init();
                         }, true);

    // 应用层
    initializer.add_task(InitStage::APPLICATION, "App Init", []
                         {
                             // 应用程序特定初始化
                         });
}