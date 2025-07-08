// components/system/init.cpp
#include "init.hpp"
#include "esp_log.h"

static const char* initTAG = "SystemInit";

SystemInitializer& SystemInitializer::instance() {
    static SystemInitializer instance;
    return instance;
}

void SystemInitializer::add_task(InitStage stage, const char* name, InitFunc func, bool critical) {
    init_tasks_.push_back({stage, name, func, critical});
}

bool SystemInitializer::execute() {
    // 按阶段排序
    std::sort(init_tasks_.begin(), init_tasks_.end(), 
        [](const InitTask& a, const InitTask& b) {
            return static_cast<int>(a.stage) < static_cast<int>(b.stage);
        });

    // 按顺序执行
    for (const auto& task : init_tasks_) {
        ESP_LOGI(initTAG, "Executing init task [%s]", task.name);
            task.function();
        // } catch (const std::exception& e) {
        //     ESP_LOGE(TAG, "Init task [%s] failed: %s", task.name, e.what());
        //     if (task.critical) {
        //         ESP_LOGE(TAG, "Critical init failure, system halted");
        //         return false;
        //     }
        // }
        // 如果需要错误处理，可让 task.function() 返回 bool 或用其他机制
    }
    return true;
}