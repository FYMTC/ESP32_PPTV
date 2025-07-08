/**
 * @file thread_wrapper.cpp
 * @brief 封装ESP32线程操作，桥接C++ std::thread与FreeRTOS任务
 * 
 * 实现功能：
 * - 支持线程名称、优先级、核心亲和性设置
 * - 线程安全的创建、销毁、移动
 * - 提供线程休眠、让出等静态工具方法
 * 
 * @author
 * @date 2025-05-23
 */

#include "thread_wrapper.hpp"
#include "esp_log.h"
#include "esp_pthread.h"

// 定义日志标签
static const char* TAG = "ThreadWrapper";

// 构造函数，初始化线程属性，创建线程
ThreadWrapper::ThreadWrapper(const std::string& name, 
                           std::function<void()> task,
                           size_t stack_size,
                           Priority priority,
                           CoreAffinity affinity)
    : name_(name), priority_(priority), affinity_(affinity) {
    
    // 配置线程属性
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = name.c_str();
    cfg.stack_size = stack_size;
    cfg.prio = static_cast<int>(priority);
    cfg.pin_to_core = static_cast<int>(affinity);
    esp_pthread_set_cfg(&cfg);

    // 创建标准线程
    thread_ = std::make_unique<std::thread>([this, task]() {
        task_handle_ = xTaskGetCurrentTaskHandle();
        task();
    });

    ESP_LOGD(TAG, "Created thread %s (stack: %zu, priority: %d, core: %d)", 
             name.c_str(), stack_size, static_cast<int>(priority), static_cast<int>(affinity));
}

// 析构函数，如果线程未结束，则进行分离
ThreadWrapper::~ThreadWrapper() {
    if (thread_ && thread_->joinable()) {
        if (!joined_) {
            ESP_LOGW(TAG, "Thread %s destroyed without join/detach, detaching now", 
                     name_.c_str());
            thread_->detach();
        }
    }
}

// 移动构造函数，将其他线程的属性赋值给当前线程
ThreadWrapper::ThreadWrapper(ThreadWrapper&& other) noexcept
    : thread_(std::move(other.thread_)),
      task_handle_(other.task_handle_),
      name_(std::move(other.name_)),
      priority_(other.priority_),
      affinity_(other.affinity_),
      joined_(other.joined_) {
    other.task_handle_ = nullptr;
    other.joined_ = true;
}

// 移动赋值运算符，将其他线程的属性赋值给当前线程
ThreadWrapper& ThreadWrapper::operator=(ThreadWrapper&& other) noexcept {
    if (this != &other) {
        if (thread_ && thread_->joinable() && !joined_) {
            thread_->detach();
        }

        thread_ = std::move(other.thread_);
        task_handle_ = other.task_handle_;
        name_ = std::move(other.name_);
        priority_ = other.priority_;
        affinity_ = other.affinity_;
        joined_ = other.joined_;

        other.task_handle_ = nullptr;
        other.joined_ = true;
    }
    return *this;
}

// 等待线程结束
void ThreadWrapper::join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
        joined_ = true;
    }
}

// 分离线程
void ThreadWrapper::detach() {
    if (thread_ && thread_->joinable()) {
        thread_->detach();
        joined_ = true;
    }
}

// 判断线程是否可等待
bool ThreadWrapper::joinable() const {
    return thread_ && thread_->joinable() && !joined_;
}

// 线程函数包装器
void ThreadWrapper::thread_func_wrapper(void* arg) {
    auto* func = static_cast<std::function<void()>*>(arg);
    (*func)();
    delete func;
}

// 设置当前线程的名称
void ThreadWrapper::set_current_thread_name(const std::string& name) {
    //vTaskSetName(pdTASK_SELF, name.c_str());
}

// 设置当前线程的优先级
void ThreadWrapper::set_current_thread_priority(Priority priority) {
    vTaskPrioritySet(nullptr, static_cast<UBaseType_t>(priority));
}

// 线程休眠
void ThreadWrapper::sleep_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// 线程让出时间片
void ThreadWrapper::yield() {
    taskYIELD();
}