#include <memory>
#include <string>
#include <esp_log.h>

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


    // 使用FreeRTOS任务代替ThreadWrapper避免std::thread问题
    xTaskCreatePinnedToCore(
        [](void* param) {
            ESP_LOGI("BOOT", "UI initialization task started");
            my_ui_init();
            ESP_LOGI("BOOT", "UI initialization completed");
            vTaskDelete(NULL); // 任务完成后删除自己
        },
        "ui_init",
        8 * 1024,    // 8KB栈
        nullptr,
        5,           // 高优先级
        nullptr,
        1            // 固定到核心1
    );

    ESP_LOGI("BOOT", "LVGL demos started");

    // 使用FreeRTOS任务代替ThreadWrapper
    xTaskCreatePinnedToCore(
        [](void* param) {
            ESP_LOGI("LVGL", "LVGL timer task started");
            while (true) {
                uint32_t time_till_next = lv_timer_handler();
                if (time_till_next == LV_NO_TIMER_READY)
                    time_till_next = LV_DEF_REFR_PERIOD;
                
                vTaskDelay(time_till_next);
            }
        },
        "lvgl_timer",
        24 * 1024,   // 16KB栈
        nullptr,
        2,           // 低优先级
        nullptr,
        1            // 固定到核心1
    );

    lvgl_tick_timer_init();
    // 4. 主循环
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
