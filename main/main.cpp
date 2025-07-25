#include <memory>
#include <string>
#include <esp_log.h>
#include <driver/i2c.h>
#include "i2c_scanner.h"
#include "thread_wrapper.hpp"
#include "thread_pool.hpp"
#include "task_scheduler.hpp"
#include "task_manager.hpp"

#include "init.hpp"
#include "init_tasks.hpp"

#include "lvgl.h"
#include "lv_demos.h"
#include "esp_timer.h"
#include "my_ui.h"

static const char *TAG = "Main";

void task_manager_demo()
{
    auto &task_manager = TaskManager::instance();

    // 每5秒打印一次系统状态
    while (true)
    {
        task_manager.print_top_like_output();
        ThreadWrapper::sleep_ms(10000);
    }
}
void lvgl_tick_timer_init(void)
{   
    esp_timer_handle_t lvgl_tick_timer = NULL;
    const esp_timer_create_args_t timer_args = {
        .callback = [](void *arg) { lv_tick_inc(1);},
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "LVGL Tick Timer",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 1000);
}



extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Application started");
    ESP_LOGI("BUILD", "Build date: %s, time: %s", __DATE__, __TIME__);
    ESP_LOGI("BUILD", "This file: H:/esp32_code/pthread/%s", __FILE__);
    // 创建任务管理器线程
    ThreadWrapper task_manager_thread(
        "TaskManager",
        task_manager_demo,
        4096,
        ThreadWrapper::Priority::LOW);
    task_manager_thread.detach();

    ESP_LOGI("BOOT", "Starting system initialization...");
    // 注册所有初始化任务
    register_init_tasks();

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


    ThreadWrapper lv_demos_thread(
        "LVGL",
        []()
        {
            // 启动 LVGL 演示
            // lv_demo_stress();
            // lv_demo_benchmark();
            // lv_demo_music();
            
            // 启动自定义LVGL应用页面
            //PageManager::instance().push(new MainPage());

            my_ui_init();
        },
        8192,
        ThreadWrapper::Priority::HIGH,
        ThreadWrapper::CoreAffinity::CORE_1);
    lv_demos_thread.detach();

    ESP_LOGI("BOOT", "LVGL demos started");

   // my_ui_init();


    ThreadWrapper lv_timer_thread(
        "LvTimer",
        []()
        {
            // 启动 LVGL 定时器处理
            while (true)
            {
                uint32_t time_till_next = lv_timer_handler();
                if (time_till_next == LV_NO_TIMER_READY)
                    time_till_next = LV_DEF_REFR_PERIOD; /*handle LV_NO_TIMER_READY. Another option is to `sleep` for longer*/
                
                // // 定期检查栈使用情况（每10秒检查一次）
                // static uint32_t last_stack_check = 0;
                // uint32_t current_time = xTaskGetTickCount();
                // if (current_time - last_stack_check > pdMS_TO_TICKS(10000)) {
                //     UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
                //     if (stack_remaining < 1024) { // 如果剩余栈小于1KB
                //         ESP_LOGW("LvTimer", "Low stack warning: %d bytes remaining", stack_remaining * sizeof(StackType_t));
                //     } else {
                //         ESP_LOGI("LvTimer", "Stack usage OK: %d bytes remaining", stack_remaining * sizeof(StackType_t));
                //     }
                //     last_stack_check = current_time;
                // }
                
                vTaskDelay(time_till_next);              /* delay to avoid unnecessary polling */
            }
        },
        48 * 1024,  // 增加栈大小从32KB到48KB
        ThreadWrapper::Priority::LOW,
        ThreadWrapper::CoreAffinity::CORE_1);
    lv_timer_thread.detach();

    lvgl_tick_timer_init();
    // 4. 主循环
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
