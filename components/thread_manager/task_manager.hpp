/**
 * @file task_manager.hpp
 * @brief ESP32任务管理器，提供任务与系统状态查询和top风格输出
 *
 * 功能：
 * - 查询所有FreeRTOS任务信息
 * - 查询系统内存与CPU使用情况
 * - 打印类似top的任务状态表
 *
 * @author
 * @date 2025-05-23
 */

// components/utilities/task_manager/task_manager.hpp
#pragma once

#include <vector>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>

class TaskManager
{
public:
    struct TaskInfo
    {
        std::string name;
        uint32_t priority;
        uint32_t stack_high_water_mark; // bytes
        uint32_t core_id;
        eTaskState state;
        uint32_t runtime; // ms
    };

    struct SystemInfo
    {
        size_t free_heap;
        size_t min_free_heap;
        size_t total_allocated;
        uint8_t cpu_usage; // percentage
    };

    static TaskManager &instance();

    // 禁用拷贝和赋值
    TaskManager(const TaskManager &) = delete;
    TaskManager &operator=(const TaskManager &) = delete;

    // 获取所有任务信息
    std::vector<TaskInfo> get_all_tasks_info() const;

    // 获取系统信息
    SystemInfo get_system_info() const;

    // 打印类似top的输出
    void print_top_like_output() const;

    enum class KillResult
    {
        SUCCESS,
        TASK_NOT_FOUND,
        TASK_IS_IDLE,
        TASK_IS_CRITICAL,
        PERMISSION_DENIED
    };

    // 通过任务名终止任务
    KillResult kill_task(const std::string &task_name);

    // 通过任务句柄终止任务
    KillResult kill_task(TaskHandle_t handle);

    //自杀功能，终止当前任务
    void task_suicide();

    //安全检查函数
    bool is_task_killable(TaskHandle_t handle)const;


private:
    TaskManager() = default;
    ~TaskManager() = default;

    // 获取单个任务信息
    TaskInfo get_task_info(TaskHandle_t handle) const;
};