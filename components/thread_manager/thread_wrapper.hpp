/**
 * @file thread_wrapper.hpp
 * @brief 封装ESP32的线程操作，桥接C++ std::thread和FreeRTOS任务
 * 
 * 提供线程创建、优先级设置、核心绑定等功能，并确保资源正确释放
 * 
 * @author
 * @date 2025-05-23
 */

// components/utilities/thread_manager/thread_wrapper.hpp
#pragma once

#include <functional>
#include <string>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <thread>

/**
 * @brief 封装ESP32的线程操作，桥接C++ std::thread和FreeRTOS任务
 * 
 * 提供线程创建、优先级设置、核心绑定等功能，并确保资源正确释放
 */
class ThreadWrapper {
public:
    /// 线程优先级枚举，映射到FreeRTOS优先级
    enum class Priority {
        IDLE = tskIDLE_PRIORITY,       ///< 空闲优先级
        LOW = tskIDLE_PRIORITY + 1,    ///< 低优先级
        MEDIUM = configMAX_PRIORITIES - 3, ///< 中等优先级
        HIGH = configMAX_PRIORITIES - 2,   ///< 高优先级
        REALTIME = configMAX_PRIORITIES - 1 ///< 实时优先级(最高)
    };

    /// 线程核心亲和性设置
    enum class CoreAffinity {
        CORE_0 = 0,     ///< 绑定到核心0
        CORE_1 = 1,     ///< 绑定到核心1
        ANY = PRO_CPU_NUM ///< 不绑定，由调度器决定
    };

    /**
     * @brief 构造函数，创建并启动线程
     * @param name 线程名称(用于调试)
     * @param task 线程执行函数
     * @param stack_size 栈大小(字节)
     * @param priority 线程优先级
     * @param affinity 线程核心亲和性
     */
    ThreadWrapper(const std::string& name, 
                std::function<void()> task,
                size_t stack_size = 4096,
                Priority priority = Priority::MEDIUM,
                CoreAffinity affinity = CoreAffinity::ANY);
    
    /// 析构函数，自动处理线程资源
    ~ThreadWrapper();

    // 禁止拷贝
    ThreadWrapper(const ThreadWrapper&) = delete;
    ThreadWrapper& operator=(const ThreadWrapper&) = delete;

    // 允许移动
    ThreadWrapper(ThreadWrapper&& other) noexcept;
    ThreadWrapper& operator=(ThreadWrapper&& other) noexcept;

    /// 等待线程结束
    void join();
    
    /// 分离线程(不再管理)
    void detach();
    
    /// 检查线程是否可join
    bool joinable() const;

    // Getter方法
    const std::string& name() const { return name_; }
    Priority priority() const { return priority_; }
    CoreAffinity affinity() const { return affinity_; }

    /**
     * @brief 设置当前线程名称
     * @param name 线程名称
     */
    static void set_current_thread_name(const std::string& name);
    
    /**
     * @brief 设置当前线程优先级
     * @param priority 优先级
     */
    static void set_current_thread_priority(Priority priority);
    
    /**
     * @brief 当前线程休眠
     * @param ms 休眠时间(毫秒)
     */
    static void sleep_ms(uint32_t ms);
    
    /// 当前线程主动让出CPU
    static void yield();

private:
    /// C线程函数包装器，用于适配C++可调用对象
    static void thread_func_wrapper(void* arg);

    std::unique_ptr<std::thread> thread_;  ///< 封装的std::thread对象
    TaskHandle_t task_handle_ = nullptr;   ///< FreeRTOS任务句柄
    std::string name_;                     ///< 线程名称
    Priority priority_;                    ///< 线程优先级
    CoreAffinity affinity_;                ///< 核心亲和性
    bool joined_ = false;                  ///< 是否已join标志
};