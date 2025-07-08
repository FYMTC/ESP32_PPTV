/**
 * @file task_manager.cpp
 * @brief ESP32任务管理器实现，提供任务与系统状态查询和top风格输出
 * 
 * 功能：
 * - 查询所有FreeRTOS任务信息
 * - 查询系统内存与CPU使用情况
 * - 打印类似top的任务状态表
 * 
 * @author
 * @date 2025-05-23
 */

// components/utilities/task_manager/task_manager.cpp
#include "task_manager.hpp"
#include <iomanip>
#include <sstream>
#include <cstring>

static const char* TAG = "TaskManager";

TaskManager& TaskManager::instance() {
    static TaskManager instance;
    return instance;
}

std::vector<TaskManager::TaskInfo> TaskManager::get_all_tasks_info() const {
    std::vector<TaskInfo> tasks_info;

    // 获取任务列表
    TaskStatus_t* task_list = nullptr;
    uint32_t task_count = uxTaskGetNumberOfTasks();
    
    // 分配内存来保存任务状态
    task_list = static_cast<TaskStatus_t*>(pvPortMalloc(sizeof(TaskStatus_t) * task_count));
    if (task_list == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for task list");
        return tasks_info;
    }

    // 获取任务状态
    task_count = uxTaskGetSystemState(task_list, task_count, nullptr);
    
    // 转换为我们的TaskInfo结构
    for (uint32_t i = 0; i < task_count; ++i) {
        TaskInfo info;
        info.name = task_list[i].pcTaskName;
        info.priority = task_list[i].uxCurrentPriority;
        info.stack_high_water_mark = task_list[i].usStackHighWaterMark * sizeof(StackType_t);
        info.core_id = task_list[i].xCoreID == tskNO_AFFINITY ? -1 : task_list[i].xCoreID; 
        info.state = task_list[i].eCurrentState;
        uint32_t tick_rate_div = (configTICK_RATE_HZ / 1000);
        info.runtime = (tick_rate_div != 0) ? (task_list[i].ulRunTimeCounter / tick_rate_div) : 0; // 转换为毫秒，避免除以零
        tasks_info.push_back(info);
    }

    vPortFree(task_list);
    return tasks_info;
}


TaskManager::SystemInfo TaskManager::get_system_info() const {
    return {
        .free_heap = esp_get_free_heap_size(),
        .min_free_heap = esp_get_minimum_free_heap_size(),
        .total_allocated = heap_caps_get_total_size(MALLOC_CAP_DEFAULT),
        .cpu_usage = 0 // 需要额外计算
    };
}

TaskManager::TaskInfo TaskManager::get_task_info(TaskHandle_t handle) const {
    TaskStatus_t task_status;
    TaskInfo info;

    if (eTaskGetState(handle) == eDeleted) {
        return info;
    }

    vTaskGetInfo(handle, &task_status, pdTRUE, eInvalid);
    
    info.name = task_status.pcTaskName;
    info.priority = task_status.uxCurrentPriority;
    info.stack_high_water_mark = task_status.usStackHighWaterMark * sizeof(StackType_t);
    info.core_id = task_status.xCoreID == tskNO_AFFINITY ? -1 : task_status.xCoreID; 
    info.state = task_status.eCurrentState;
    uint32_t tick_rate_div = (configTICK_RATE_HZ / 1000);
    info.runtime = (tick_rate_div != 0) ? (task_status.ulRunTimeCounter / tick_rate_div) : 0;

    return info;
}

void TaskManager::print_top_like_output() const {
    auto system_info = get_system_info();
    auto tasks_info = get_all_tasks_info();

    // 计算CPU使用率 (简化版)
    uint32_t total_runtime = 0;
    for (const auto& task : tasks_info) {
        total_runtime += task.runtime;
    }

    // 打印系统信息
    std::stringstream ss;
    ss << "\n\nESP32 Task Manager\n";
    ss << "============================\n";
    ss << "Memory: Free=" << system_info.free_heap/1024 << " kbytes, Min Free=" 
       << system_info.min_free_heap/1024 << " kbytes, Total=" 
       << system_info.total_allocated/1024 << " kbytes\n";
    
    // if (total_runtime > 0) {
    //     ss << "CPU Usage: ";
    //     for (const auto& task : tasks_info) {
    //         uint8_t usage = (task.runtime * 100) / total_runtime;
    //         ss << task.name << ":" << static_cast<int>(usage) << "%, ";
    //     }
    //     ss << "\n";
    // }

    // 打印任务表头
    ss << "\n";
    ss << std::left << std::setw(20) << "Task Name";
    ss << std::setw(8) << "Prio";
    ss << std::setw(8) << "Core";
    ss << std::setw(12) << "State";
    ss << std::setw(12) << "Stack";
    ss << std::setw(12) << "CPU Usage";
    ss << std::setw(8) << "Killable";
    ss << "\n";
    ss << "-------------------------------------------------------------------------------\n";

    // 打印每个任务信息
    for (const auto& task : tasks_info) {
        ss << std::left << std::setw(20) << task.name;
        ss << std::setw(8) << task.priority;
        ss << std::setw(8) << task.core_id;

        // 状态转换为字符串
        std::string state_str;
        switch (task.state) {
            case eRunning: state_str = "Running"; break;
            case eReady: state_str = "Ready"; break;
            case eBlocked: state_str = "Blocked"; break;
            case eSuspended: state_str = "Suspended"; break;
            case eDeleted: state_str = "Deleted"; break;
            default: state_str = "Unknown"; break;
        }
        ss << std::setw(12) << state_str;
        ss << std::setw(12) << task.stack_high_water_mark;
        ss << std::setw(12) << std::to_string((task.runtime * 100) / total_runtime)+"%";
        ss << std::setw(8) << (is_task_killable(xTaskGetHandle(task.name.c_str())) ? "Y" : "N");
        ss << "\n";
    }

    ESP_LOGI(TAG, "\n%s", ss.str().c_str());
}

TaskManager::KillResult TaskManager::kill_task(const std::string& task_name) {
    // 获取任务句柄
    TaskHandle_t handle = xTaskGetHandle(task_name.c_str());
    if (handle == nullptr) {
        return KillResult::TASK_NOT_FOUND;
    }
    
    return kill_task(handle);
}

TaskManager::KillResult TaskManager::kill_task(TaskHandle_t handle) {
    // 安全检查
    if (handle == nullptr) {
        return KillResult::TASK_NOT_FOUND;
    }
    
    // 不允许终止空闲任务
    const char* pcTaskName = pcTaskGetName(handle);
    if (strncmp(pcTaskName, "IDLE", 4) == 0) {
        ESP_LOGE(TAG, "Cannot kill IDLE task");
        return KillResult::TASK_IS_IDLE;
    }
    
    // 不允许终止高优先级系统任务
    if (uxTaskPriorityGet(handle) >= configMAX_PRIORITIES - 1) {
        ESP_LOGE(TAG, "Cannot kill critical system task");
        return KillResult::TASK_IS_CRITICAL;
    }
    
    // 不允许终止当前任务（除非是自杀）
    if (handle == xTaskGetCurrentTaskHandle()) {
        ESP_LOGE(TAG, "Task cannot kill itself (use suicide instead)");
        return KillResult::PERMISSION_DENIED;
    }
    
    // 实际终止任务
    vTaskDelete(handle);
    ESP_LOGI(TAG, "Task %s killed successfully", pcTaskGetName(handle));
    return KillResult::SUCCESS;
}

void TaskManager::task_suicide() {
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    ESP_LOGI(TAG, "Task %s is committing suicide", pcTaskGetName(handle));
    vTaskDelete(nullptr); // 参数nullptr表示删除当前任务
}

bool TaskManager::is_task_killable(TaskHandle_t handle)const{
    if (handle == nullptr) return false;
    
    const char* pcTaskName = pcTaskGetName(handle);
    UBaseType_t priority = uxTaskPriorityGet(handle);
    
    return !(strncmp(pcTaskName, "IDLE", 4) == 0 || 
            priority >= configMAX_PRIORITIES - 1 ||
            handle == xTaskGetCurrentTaskHandle());
}