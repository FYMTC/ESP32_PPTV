// main/init_tasks.cpp
#include "init_tasks.hpp"
#include "init.hpp"
#include "nvs_init.hpp"
#include "sd_init.hpp"
#include "i2c_init.hpp"
#include "lvgl.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "wifi_service.h"
#include "sntp_service.h"

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
        auto devices = scan_i2c_devices();
        if (devices.empty()) {
            ESP_LOGW("I2C", "No I2C devices found");
        } });
    initializer.add_task(InitStage::DRIVERS, "LVGL", []()
                         {
                            lv_init();
                            lv_port_disp_init(); 
                            lv_port_indev_init();//绝不能在 lv_port_disp_init() 之前调用 lv_port_indev_init() 或创建 LVGL 对象

                            ESP_LOGI("LVGL", "LVGL initialized successfully");
                        }, true);

    initializer.add_task(InitStage::DRIVERS, "SD Card", sd_init);

    // 服务层
    initializer.add_task(InitStage::SERVICES, "Filesystem", []
                         {
                             //TODO: 挂载文件系统等
                         });
    initializer.add_task(InitStage::SERVICES, "WiFi Service", []
                         {
                             // 启动WiFi后台任务
                             wifi_manager_connect();
                             ESP_LOGI("WiFi Service", "WiFi service started");
                         }, true);
    initializer.add_task(InitStage::SERVICES, "SNTP", []
                         {
                             initialize_sntp();
                         });

    // 应用层
    initializer.add_task(InitStage::APPLICATION, "App Init", []
                         {
                             // 应用程序特定初始化
                         });
}