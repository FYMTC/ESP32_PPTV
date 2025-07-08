/**
 * @file task_scheduler.hpp
 * @brief 任务调度器，支持周期性与一次性任务调度
 * 
 * 功能：
 * - 单次定时任务
 * - 周期性定时任务
 * - 任务取消
 * - 线程安全操作
 * 
 * @author
 * @date 2025-05-23
 */

// components/utilities/thread_manager/task_scheduler.hpp
#pragma once

#include <map>
#include <chrono>
#include <functional>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "thread_wrapper.hpp"

/**
 * @brief 定时任务调度器
 * 
 * 功能：
 * - 单次定时任务
 * - 周期性定时任务
 * - 任务取消
 * - 线程安全操作
 */
class TaskScheduler {
public:
    using TaskID = uint32_t;                      ///< 任务ID类型
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>; ///< 时间点
    using Duration = std::chrono::milliseconds;   ///< 持续时间(毫秒)
    using Task = std::function<void()>;           ///< 任务函数类型

    TaskScheduler();
    ~TaskScheduler();

    /**
     * @brief 调度周期性任务
     * @param task 任务函数
     * @param interval 执行间隔
     * @param immediate 是否立即执行第一次
     * @return TaskID 任务ID(用于取消)
     */
    TaskID schedule(const Task& task, Duration interval, bool immediate = false);
    
    /**
     * @brief 调度单次任务
     * @param task 任务函数
     * @param delay 延迟时间
     * @return TaskID 任务ID
     */
    TaskID schedule_once(const Task& task, Duration delay);
    
    /**
     * @brief 取消任务
     * @param id 任务ID
     * @return bool 是否取消成功
     */
    bool cancel(TaskID id);
    
    /// 清除所有任务
    void clear();

private:
    /// 定时任务结构
    struct ScheduledTask {
        Task task;               ///< 任务函数
        TimePoint next_run;      ///< 下次运行时间
        Duration interval;       ///< 运行间隔(0表示单次)
        bool recurring;          ///< 是否周期性任务
        TaskID id;              ///< 任务ID
    };

    /// 调度器线程函数
    void scheduler_thread();

    std::map<TimePoint, ScheduledTask> tasks_; ///< 任务队列(按时间排序)
    std::mutex mutex_;                         ///< 互斥锁
    std::condition_variable condition_;        ///< 条件变量
    std::unique_ptr<ThreadWrapper> scheduler_thread_; ///< 调度线程
    std::atomic<bool> running_{false};        ///< 运行标志
    std::atomic<TaskID> next_id_{0};          ///< 下一个任务ID
};