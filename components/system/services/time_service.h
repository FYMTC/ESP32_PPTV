#pragma once

#include <time.h>
#include <stdbool.h>
#include <functional>

namespace time_service {

// 时间信息结构体
struct TimeInfo {
    char time_str[16];      // HH:MM 格式
    char datetime_str[32];  // YYYY-MM-DD HH:MM:SS 格式
    char running_str[16];   // 运行时间 HH:MM:SS 格式
    bool is_synced;         // 时间是否已同步
    time_t timestamp;       // Unix 时间戳
};

// 时间更新回调函数类型
typedef std::function<void(const TimeInfo&)> TimeUpdateCallback;

// 初始化时间服务（包含SNTP功能）
void init();

// 反初始化时间服务
void deinit();

// 获取当前时间信息
TimeInfo get_time_info();

// 注册时间更新回调（每秒触发一次）
void set_time_update_callback(TimeUpdateCallback callback);

// 移除时间更新回调
void remove_time_update_callback();

// 检查时间是否已同步
bool is_time_synced();

// 格式化运行时间
const char* format_running_time(unsigned long millis);

// SNTP相关功能
void start_sntp_sync();
void stop_sntp_sync();
bool is_sntp_syncing();

// RTC相关功能
bool is_rtc_available();
bool sync_rtc_from_system();
bool get_rtc_time(struct tm* rtc_time);
bool init_system_time_from_rtc_public(); // 公共接口，手动从RTC初始化系统时间

} // namespace time_service
