/**
 * @file thread_pool.cpp
 * @brief 动态线程池实现，可根据负载自动调整线程数量
 * 
 * 实现功能：
 * - 支持最小/最大线程数限制
 * - 空闲线程超时回收
 * - 线程安全的任务队列
 * - 线程池自动扩缩容
 * 
 * @author 
 * @date 2025-05-23
 */

#include "thread_pool.hpp"
#include "esp_log.h"

static const char* TAG = "ThreadPool";

ThreadPool::ThreadPool(size_t min_threads, size_t max_threads, size_t idle_timeout_sec)
    : min_threads_(min_threads), 
      max_threads_(max_threads),
      idle_timeout_sec_(idle_timeout_sec) {
    
    ESP_LOGI(TAG, "Creating thread pool (min: %zu, max: %zu, timeout: %zus)",
             min_threads_, max_threads_, idle_timeout_sec_);
    
    for (size_t i = 0; i < min_threads_; ++i) {
        workers_.emplace_back(std::make_unique<ThreadWrapper>(
            "PoolWorker" + std::to_string(i),
            [this] { worker_thread(); },
            3072,  // 3KB stack
            ThreadWrapper::Priority::MEDIUM
        ));
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    
    condition_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker->joinable()) {
            worker->join();
        }
    }
}

void ThreadPool::worker_thread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            condition_.wait_for(lock, 
                std::chrono::seconds(idle_timeout_sec_),
                [this] { return stop_ || !tasks_.empty(); });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            if (tasks_.empty()) {
                // 超时且没有任务，检查是否需要退出
                if (workers_.size() > min_threads_) {
                    // 找到当前线程并移除
                    auto self = std::find_if(workers_.begin(), workers_.end(),
                        [](const auto& w) {
                            return w->name() == "PoolWorker" + std::to_string(reinterpret_cast<uintptr_t>(xTaskGetCurrentTaskHandle()));
                        });
                    
                    if (self != workers_.end()) {
                        workers_.erase(self);
                        ESP_LOGD(TAG, "Idle worker thread exited");
                        return;
                    }
                }
                continue;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
            ++active_count_;
        }
        
        task();
        --active_count_;
    }
}

size_t ThreadPool::active_threads() const {
    return active_count_;
}

size_t ThreadPool::pending_tasks() const {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::wait_all() {
    while (pending_tasks() > 0 || active_threads() > 0) {
        ThreadWrapper::sleep_ms(100);
    }
}

void ThreadPool::adjust_thread_count() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    size_t pending = tasks_.size();
    size_t total_threads = workers_.size();
    size_t active = active_count_;
    
    // 需要更多线程的条件：
    // 1. 有未处理任务
    // 2. 活跃线程数等于总线程数（所有线程都忙）
    // 3. 总线程数小于最大值
    if (pending > 0 && active >= total_threads && total_threads < max_threads_) {
        size_t new_threads = std::min(pending, max_threads_ - total_threads);
        
        for (size_t i = 0; i < new_threads; ++i) {
            workers_.emplace_back(std::make_unique<ThreadWrapper>(
                "PoolWorker" + std::to_string(total_threads + i),
                [this] { worker_thread(); },
                3072,  // 3KB stack
                ThreadWrapper::Priority::MEDIUM
            ));
            
            ESP_LOGD(TAG, "Added new worker thread (total: %zu)", workers_.size());
        }
    }
}

void ThreadPool::cleanup_idle_threads() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    // 只在超过最小线程数时尝试清理
    while (workers_.size() > min_threads_) {
        // 简单移除队尾线程（假设队尾线程是空闲的）
        auto it = workers_.end();
        --it;
        if (it != workers_.begin()) {
            if (!(*it)->joinable()) {
                workers_.erase(it);
                ESP_LOGI(TAG, "Idle thread cleaned up, current threads: %zu", workers_.size());
            } else {
                break;
            }
        } else {
            break;
        }
    }
}