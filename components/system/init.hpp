// components/system/init.hpp
#pragma once

#include <functional>
#include <vector>

enum class InitStage {
    EARLY,      // 最早期的初始化（硬件相关）
    CORE,       // 核心子系统（NVS, 事件循环等）
    DRIVERS,    // 驱动程序（I2C, SPI等）
    SERVICES,   // 服务层（文件系统, 网络等）
    APPLICATION // 应用层初始化
};

using InitFunc = std::function<void()>;

struct InitTask {
    InitStage stage;
    const char* name;
    InitFunc function;
    bool critical; // 如果失败是否阻止继续启动
};

class SystemInitializer {
public:
    static SystemInitializer& instance();
    
    void add_task(InitStage stage, const char* name, InitFunc func, bool critical = false);
    bool execute();
    
private:
    SystemInitializer() = default;
    std::vector<InitTask> init_tasks_;
};