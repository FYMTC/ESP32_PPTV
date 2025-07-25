#pragma once

#include <functional>

namespace battery_service {

// 电池信息结构体
struct BatteryInfo {
    int percentage;         // 电池百分比 (0-100)
    bool is_charging;       // 是否正在充电
    bool is_connected;      // 电池是否连接
    int voltage_mv;         // 电池电压 (mV)
    bool is_valid;          // 数据是否有效
};

// 电池状态更新回调函数类型
typedef std::function<void(const BatteryInfo&)> BatteryUpdateCallback;

// 初始化电池服务
bool init();

// 反初始化电池服务
void deinit();

// 获取当前电池信息
BatteryInfo get_battery_info();

// 注册电池状态更新回调
void set_battery_update_callback(BatteryUpdateCallback callback);

// 移除电池状态更新回调
void remove_battery_update_callback();

// 检查电池服务是否可用
bool is_available();

// 手动更新电池信息（用于调试）
void update_battery_info();

} // namespace battery_service
