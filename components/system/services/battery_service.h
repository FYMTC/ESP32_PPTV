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

// 低电量关机配置
struct LowBatteryConfig {
    int shutdown_percentage;    // 关机电量阈值 (默认5%)
    int warning_percentage;     // 警告电量阈值 (默认15%)
    bool enabled;               // 是否启用自动关机 (默认true)
};

// 电池状态更新回调函数类型
typedef std::function<void(const BatteryInfo&)> BatteryUpdateCallback;

// 低电量警告回调函数类型
typedef std::function<void(int percentage)> LowBatteryWarningCallback;

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

// 设置低电量警告回调
void set_low_battery_warning_callback(LowBatteryWarningCallback callback);

// 配置低电量关机设置
void configure_low_battery_shutdown(const LowBatteryConfig& config);

// 获取当前低电量配置
LowBatteryConfig get_low_battery_config();

// 检查电池服务是否可用
bool is_available();

// 手动更新电池信息（用于调试）
void update_battery_info();

} // namespace battery_service
