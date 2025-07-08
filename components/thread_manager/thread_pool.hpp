/**
 * @file thread_pool.hpp
 * @brief 动态线程池实现，可根据负载自动调整线程数量
 * 
 * 特点：
 * - 最小/最大线程数限制
 * - 空闲线程超时回收
 * - 任务队列
 * - 线程安全的任务提交
 * 
 * @author
 * @date 2025-05-23
 */

// components/utilities/thread_manager/thread_pool.hpp
#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include "thread_wrapper.hpp"

/**
 * @brief 动态线程池实现，可根据负载自动调整线程数量
 * 
 * 特点：
 * - 最小/最大线程数限制
 * - 空闲线程超时回收
 * - 任务队列
 * - 线程安全的任务提交
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数
     * @param min_threads 最小线程数
     * @param max_threads 最大线程数
     * @param idle_timeout_sec 空闲线程超时时间(秒)
     */
    explicit ThreadPool(size_t min_threads = 2, 
                      size_t max_threads = 8,
                      size_t idle_timeout_sec = 30);
    
    /// 析构函数，等待所有任务完成
    ~ThreadPool();

    // 禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief 提交任务到线程池
     * @tparam F 可调用对象类型
     * @tparam Args 参数类型
     * @param f 可调用对象
     * @param args 调用参数
     * @return std::future 用于获取异步结果
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>{
        
        using return_type = typename std::invoke_result<F, Args...>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                // Exception handling is disabled; handle error without throwing
                // You can use assert, abort, or return a default-constructed future
                // Here, we return a default-constructed future
                return std::future<return_type>();
            }
            
            tasks_.emplace([task](){ (*task)(); });
        }
        
        condition_.notify_one();
        adjust_thread_count();
        
        return res;
    }

    /// 获取当前活跃线程数
    size_t active_threads() const;
    
    /// 获取等待中的任务数
    size_t pending_tasks() const;
    
    /// 等待所有任务完成
    void wait_all();

private:
    /// 工作线程执行函数
    void worker_thread();
    
    /// 根据负载调整线程数量
    void adjust_thread_count();
    
    /// 清理空闲线程
    void cleanup_idle_threads();

    std::vector<std::unique_ptr<ThreadWrapper>> workers_; ///< 工作线程集合
    std::queue<std::function<void()>> tasks_;            ///< 任务队列
    
    mutable std::mutex queue_mutex_;                     ///< 队列互斥锁
    std::condition_variable condition_;                  ///< 条件变量
    
    std::atomic<size_t> active_count_{0};                ///< 活跃线程计数
    std::atomic<bool> stop_{false};                      ///< 停止标志
    
    const size_t min_threads_;                           ///< 最小线程数
    const size_t max_threads_;                           ///< 最大线程数
    const size_t idle_timeout_sec_;                      ///< 空闲超时(秒)
};