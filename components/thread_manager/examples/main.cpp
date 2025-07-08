#include <memory>
#include <string>
#include <esp_log.h>
#include "thread_wrapper.hpp"
#include "thread_pool.hpp"
#include "task_scheduler.hpp"
#include "task_manager.hpp"

// I2C 配置
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO (gpio_num_t)17
#define I2C_MASTER_SCL_IO (gpio_num_t)18
#define I2C_MASTER_FREQ_HZ 100000

static const char* TAG = "Main";

void task_manager_demo() {
    auto& task_manager = TaskManager::instance();
    
    // 每5秒打印一次系统状态
    while (true) {
        task_manager.print_top_like_output();
        ThreadWrapper::sleep_ms(5000);
    }
}
/**
 * @brief 打印当前线程信息
 * @param extra 额外信息(可选)
 */
void print_thread_info(const char* extra = nullptr) {
    std::string info;
    if (extra) {
        info += extra;
    }
    info += "Core id: " + std::to_string(xPortGetCoreID()) + 
            ", prio: " + std::to_string(uxTaskPriorityGet(nullptr)) + 
            ", minimum free stack: " + std::to_string(uxTaskGetStackHighWaterMark(nullptr)) + " bytes.";
    ESP_LOGI(pcTaskGetName(nullptr), "%s", info.c_str());
}


/**
 * @brief 周期性打印信息的任务
 */
void print_info_task() {
    print_thread_info("Info Task: ");
}



extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Application started");
     // 创建任务管理器线程
    ThreadWrapper task_manager_thread(
        "TaskManager",
        task_manager_demo,
        4096,
        ThreadWrapper::Priority::LOW
    );
    task_manager_thread.detach();
    

    // 2. 创建线程池处理其他任务
    ThreadPool pool(2, 4);   // 2-4个工作线程
    
    // 向线程池提交任务
    pool.enqueue([] {
        ESP_LOGI(TAG, "Task 1 started in thread pool");
        ThreadWrapper::sleep_ms(2000);
        ESP_LOGI(TAG, "Task 1 completed");
    });
    
    pool.enqueue([] {
        ESP_LOGI(TAG, "Task 2 started in thread pool");
        ThreadWrapper::sleep_ms(1000);
        ESP_LOGI(TAG, "Task 2 completed");
    });

    // 3. 使用任务调度器定时执行任务
    TaskScheduler scheduler;
    
    // 每3秒执行一次信息打印
    scheduler.schedule(print_info_task, std::chrono::seconds(3));
    
    // 5秒后执行一次延迟任务
    scheduler.schedule_once([] {
        ESP_LOGI(TAG, "This is a one-time delayed task");
    }, std::chrono::seconds(5));

    // 4. 主循环
    while (true) {
        print_thread_info("Main Thread: ");
        ThreadWrapper::sleep_ms(5000);
        
        // 可以在这里添加其他主线程逻辑
        // 例如检查线程池状态
        ESP_LOGD(TAG, "ThreadPool status - active: %zu, pending: %zu", 
                pool.active_threads(), pool.pending_tasks());
    }
}