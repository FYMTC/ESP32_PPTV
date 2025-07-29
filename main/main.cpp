#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "thread_pool.hpp"
// #include "task_scheduler.hpp"

#include "init_tasks.hpp"
#include "tests.h"
#include "conf.h"
static const char *TAG = "Main";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Application started");
    ESP_LOGI("BUILD", "Build date: %s, time: %s", __DATE__, __TIME__);
    ESP_LOGI("BUILD", "This file: H:/esp32_code/pthread/%s", __FILE__);

    // 注册所有初始化任务 //
    register_init_tasks();

#if USE_LVGL
    init_lvgl();  //产生10mA功耗
#endif // LVGL

    //test001();// 测试函数调用

    // 4. 主循环
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
