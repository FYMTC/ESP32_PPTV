/**
 * @file task_scheduler.cpp
 * @brief 任务调度器实现，支持周期性与一次性任务调度
 * 
 * 功能：
 * - 支持周期性任务和一次性任务的调度
 * - 线程安全的任务队列管理
 * - 任务取消与清空
 * - 自动线程管理
 * 
 * @author
 * @date 2025-05-23
 */

// components/utilities/thread_manager/task_scheduler.cpp
#include "task_scheduler.hpp"
#include "esp_log.h"

// 定义日志标签
// static const char* TAG = "TaskScheduler";

// 构造函数，初始化任务调度器
TaskScheduler::TaskScheduler() {
    running_ = true;
    // 创建一个线程，用于执行任务调度
    scheduler_thread_ = std::make_unique<ThreadWrapper>(
        "TaskScheduler",
        [this] { scheduler_thread(); },
        4096,  // 4KB stack
        ThreadWrapper::Priority::HIGH
    );
}

// 析构函数，停止任务调度器
TaskScheduler::~TaskScheduler() {
    running_ = false;
    // 通知所有等待的线程
    condition_.notify_all();
    // 如果线程还在运行，等待线程结束
    if (scheduler_thread_ && scheduler_thread_->joinable()) {
        scheduler_thread_->join();
    }
}

// 调度一个周期性任务
TaskScheduler::TaskID TaskScheduler::schedule(const Task& task, Duration interval, bool immediate) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 生成任务ID
    TaskID id = ++next_id_;
    // 计算第一次运行的时间
    TimePoint first_run = immediate ? 
        std::chrono::system_clock::now() : 
        std::chrono::system_clock::now() + interval;
    
    // 将任务添加到任务队列中
    tasks_.emplace(first_run, ScheduledTask{
        .task = task,
        .next_run = first_run,
        .interval = interval,
        .recurring = true,
        .id = id
    });
    
    // 通知等待的线程
    condition_.notify_one();
    return id;
}

// 调度一个一次性任务
TaskScheduler::TaskID TaskScheduler::schedule_once(const Task& task, Duration delay) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 生成任务ID
    TaskID id = ++next_id_;
    // 计算运行时间
    TimePoint run_time = std::chrono::system_clock::now() + delay;
    
    // 将任务添加到任务队列中
    tasks_.emplace(run_time, ScheduledTask{
        .task = task,
        .next_run = run_time,
        .interval = Duration(0),
        .recurring = false,
        .id = id
    });
    
    // 通知等待的线程
    condition_.notify_one();
    return id;
}

// 取消一个任务
bool TaskScheduler::cancel(TaskID id) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 遍历任务队列，找到要取消的任务
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        if (it->second.id == id) {
            // 从任务队列中移除任务
            tasks_.erase(it);
            return true;
        }
    }
    return false;
}

// 清空任务队列
void TaskScheduler::clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    tasks_.clear();
}

// 任务调度线程
void TaskScheduler::scheduler_thread() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 如果任务队列为空，等待任务加入
        if (tasks_.empty()) {
            condition_.wait(lock, [this] { return !running_ || !tasks_.empty(); });
            if (!running_) break;
        }
        
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        // 获取下一个要运行的任务
        auto next_task = tasks_.begin();
        
        // 如果下一个任务的时间还未到，等待
        if (next_task->first > now) {
            condition_.wait_until(lock, next_task->first);
            continue;
        }
        
        // 获取要运行的任务
        ScheduledTask scheduled = std::move(next_task->second);
        // 从任务队列中移除任务
        tasks_.erase(next_task);
        
        lock.unlock();
        
        // try {
            // 执行任务
            scheduled.task();
        // } catch (const std::exception& e) {
        //     // 如果任务执行失败，记录错误日志
        //     ESP_LOGE(TAG, "Task %u failed: %s", scheduled.id, e.what());
        // }
        
        // 如果任务需要重复执行，计算下一次运行的时间，并将任务重新加入任务队列
        if (scheduled.recurring) {
            lock.lock();
            TimePoint next_run = scheduled.next_run + scheduled.interval;
            scheduled.next_run = next_run;
            tasks_.emplace(next_run, std::move(scheduled));
            lock.unlock();
        }
    }
}