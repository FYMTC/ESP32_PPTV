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

#include "lcd_brightness.hpp"
#include "my_ui.h"
#include "conf.h"

#include "task_manager.hpp"
#include "thread_wrapper.hpp"

#include "lcd_brightness.hpp"          // 添加亮度控制头文件
#include "system_low_power_design.hpp" // 添加功耗管理头文件

void task_manager()
{
    auto &task_manager = TaskManager::instance();

    // 每5秒打印一次系统状态
    while (true)
    {
        task_manager.print_top_like_output();
        ThreadWrapper::sleep_ms(10000);
    }
}

void register_init_tasks()
{
#if USE_TASK_MANAGER
    // 创建任务管理器线程
    ThreadWrapper task_manager_thread(
        "TaskManager",
        task_manager,
        4096,
        ThreadWrapper::Priority::LOW);
    task_manager_thread.detach();
#endif

    ESP_LOGI("BOOT", "Starting system initialization...");


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

#if USE_LVGL
    initializer.add_task(InitStage::DRIVERS, "LVGL", []()
                         {
                            lv_init();
                            lv_port_disp_init(); 
                            lv_port_indev_init();//绝不能在 lv_port_disp_init() 之前调用 lv_port_indev_init() 或创建 LVGL 对象

                            ESP_LOGI("LVGL", "LVGL initialized successfully"); }, true);
    // // 初始化LVGL功耗管理器，息屏的时候降低频率
    // initializer.add_task(InitStage::DRIVERS, "LVGL Power Manager", []()
    //                      {  esp_err_t err = lvgl_power_manager_init();
    //                          if (err != ESP_OK) {
    //                              ESP_LOGE("LVGL Power Manager", "Failed to initialize: %s", esp_err_to_name(err));
    //                          }
    //                      }, true);
    // 创建亮度控制任务，自动息屏，自动亮度调节
    initializer.add_task(InitStage::DRIVERS, "Brightness Control", create_brightness_task, true);
#endif
    // 文件系统依赖
    initializer.add_task(InitStage::DRIVERS, "SD Card", sd_init);

    // 初始化RTC芯片
    initializer.add_task(InitStage::DRIVERS, "RTC DS1302", rtc_ds1302_init);

    // 服务层
    initializer.add_task(InitStage::SERVICES, "Filesystem", []
                         {
                             // TODO: 挂载文件系统等
                         });
    // 初始化时间服务
    initializer.add_task(InitStage::SERVICES, "Time Service", []
                         { time_service::init(); }, true);
    // 初始化电池服务
    initializer.add_task(InitStage::SERVICES, "Battery Service", []
                         { battery_service::init(); }, true);

    // 应用层
    initializer.add_task(InitStage::APPLICATION, "App Init", []
                         {
                             // 应用程序特定初始化
                         });

        // 执行初始化
    if (!SystemInitializer::instance().execute())
    {
        ESP_LOGE("BOOT", "System initialization failed");
        while (true)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI("BOOT", "System initialized successfully");
}

void lvgl_tick_timer_init(void)
{
    esp_timer_handle_t lvgl_tick_timer = NULL;
    const esp_timer_create_args_t timer_args = {
        .callback = [](void *arg)
        { lv_tick_inc(1); },
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "LVGL Tick Timer",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 1000);
}

void init_lvgl()
{
    // 使用FreeRTOS任务代替ThreadWrapper避免std::thread问题
    xTaskCreatePinnedToCore(
        [](void *param)
        {
            ESP_LOGI("BOOT", "UI initialization task started");
            my_ui_init();
            ESP_LOGI("BOOT", "UI initialization completed");
            vTaskDelete(NULL); // 任务完成后删除自己
        },
        "ui_init",
        8 * 1024, // 8KB栈
        nullptr,
        5, // 高优先级
        nullptr,
        1 // 固定到核心1
    );

    ESP_LOGI("BOOT", "LVGL demos started");

    // 使用FreeRTOS任务代替ThreadWrapper
    xTaskCreatePinnedToCore(
        [](void *param)
        {
            ESP_LOGI("LVGL", "LVGL timer task started");

            // 初始化LVGL功耗管理器
            if (lvgl_power_manager_init() != ESP_OK)
            {
                ESP_LOGE("LVGL", "Failed to initialize LVGL power manager");
                vTaskDelete(NULL);
                return;
            }

            while (true)
            {
                // 自动调整功耗模式
                lvgl_power_manager_auto_adjust();

                // 获取当前刷新间隔
                uint32_t refresh_interval = lvgl_power_manager_get_refresh_interval();

                uint32_t time_till_next = lv_timer_handler();
                if (time_till_next == LV_NO_TIMER_READY)
                {
                    time_till_next = refresh_interval;
                }

                // 确保不会超过当前功耗模式的最大刷新间隔
                if (time_till_next < refresh_interval)
                {
                    time_till_next = refresh_interval;
                }

                vTaskDelay(pdMS_TO_TICKS(time_till_next));
            }
        },
        "lvgl_timer",
        24 * 1024, // 24KB栈
        nullptr,
        2, // 低优先级
        nullptr,
        1 // 固定到核心1
    );

    lvgl_tick_timer_init();
}