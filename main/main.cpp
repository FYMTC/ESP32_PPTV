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
/**
 * @brief 打印当前线程信息
 * @param extra 额外信息(可选)
 */
void print_thread_info(const char *extra = nullptr)
{
    std::string info;
    if (extra)
    {
        info += extra;
    }
    info += "Core id: " + std::to_string(xPortGetCoreID()) +
            ", prio: " + std::to_string(uxTaskPriorityGet(nullptr)) +
            ", minimum free stack: " + std::to_string(uxTaskGetStackHighWaterMark(nullptr)) + " bytes.";
    ESP_LOGI(pcTaskGetName(nullptr), "%s", info.c_str());
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
        "LvDemos",
        []()
        {
            // 启动 LVGL 演示
            // lv_demo_stress();
            // lv_demo_benchmark();
            lv_demo_music();
        },
        8192,
        ThreadWrapper::Priority::MEDIUM,
        ThreadWrapper::CoreAffinity::CORE_1);
    lv_demos_thread.detach();

    ESP_LOGI("BOOT", "LVGL demos started");

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
                vTaskDelay(time_till_next);              /* delay to avoid unnecessary polling */
            }
        },
        12 * 1024,
        ThreadWrapper::Priority::LOW,
        ThreadWrapper::CoreAffinity::CORE_1);
    lv_timer_thread.detach();

    print_thread_info("Main Thread: ");

    lvgl_tick_timer_init();
    // 4. 主循环
    while (true)
    {

        // lv_timer_handler();
        // lv_timer_periodic_handler();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
