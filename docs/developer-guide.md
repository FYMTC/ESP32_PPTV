# 开发者指南

本文档面向二次开发者，涵盖：软件架构、模块职责、二次开发流程、以及项目过程中
踩过的坑和对应解决方法。

## 1. 软件架构

### 1.1 分层总览

```
┌─────────────────────────────────────────────┐
│  main/                app_main, init_tasks   │  应用入口
├─────────────────────────────────────────────┤
│  components/ui/        LVGL 页面             │  UI 层
│  components/lvgl_app/  page_manager          │
├─────────────────────────────────────────────┤
│  components/system/services/                 │  服务层
│    wifi_manager, time_service, battery,      │
│    pmu_service, mpu6050_service, ...         │
├─────────────────────────────────────────────┤
│  components/system/drivers/                  │  驱动层
│    LovyanGFX, CST128, AXP2101, PCA9554,      │
│    DS1302, encoder, lcd_brightness, ...      │
├─────────────────────────────────────────────┤
│  components/system/storage/                  │  存储
│    nvs_init, sd_init                         │
├─────────────────────────────────────────────┤
│  components/thread_manager/                  │  并发基础设施
│    thread_pool, task_scheduler, ThreadWrapper│
├─────────────────────────────────────────────┤
│  ESP-IDF v6.0.2 + FreeRTOS                   │  系统层
└─────────────────────────────────────────────┘
```

### 1.2 启动流程

入口在 [main/main.cpp](../main/main.cpp) 的 `app_main()`：

1. `register_init_tasks()` —— 向 `SystemInitializer` 注册各阶段任务
2. `SystemInitializer::execute()` —— 按 `EARLY → CORE → DRIVERS → SERVICES → APPLICATION`
   顺序执行；任一 `critical=true` 的任务失败则阻塞
3. `init_lvgl()` —— 创建 UI 初始化任务（核心 1）和 LVGL tick/timer 任务

各阶段任务在 [main/init_tasks.cpp](../main/init_tasks.cpp) 中注册：

| 阶段 | 任务 |
|------|------|
| EARLY | CPU Cache（占位） |
| CORE | NVS 初始化 |
| DRIVERS | I2C 总线 + 扫描、LVGL 初始化、亮度控制、SD 卡、RTC DS1302 |
| SERVICES | Time Service、Battery Service |
| APPLICATION | 应用层（占位） |

### 1.3 关键模块说明

#### SystemInitializer（分阶段初始化器）

[components/system/init.hpp](../components/system/init.hpp) / [init.cpp](../components/system/init.cpp)

按阶段注册任务，统一执行。新增初始化代码时**不要直接在 `app_main` 里写**，
而是用 `initializer.add_task(InitStage::XXX, "name", func, critical)` 注册，
保持启动流程清晰。

#### ThreadManager / ThreadWrapper

[components/thread_manager/](../components/thread_manager/)

- `ThreadWrapper`: 对 `std::thread` / FreeRTOS task 的轻量封装，支持优先级、栈大小、
  core affinity、`sleep_ms` 等。
- `ThreadPool`: 固定大小线程池。
- `TaskScheduler`: 周期任务调度器。

> 在 IDF 6.0 + FreeRTOS 双核上，`std::thread` 可用但需注意栈大小。
> LVGL 相关任务用 `xTaskCreatePinnedToCore` 固定到核心 1，避免与 WiFi/系统任务竞争核心 0。

#### LVGL 集成

- 配置文件：[components/lv_conf.h](../components/lv_conf.h)（LVGL 9.3）
- 显示桥接：[components/system/drivers/lv_port_disp.cpp](../components/system/drivers/lv_port_disp.cpp)
  底层调用 LovyanGFX 绘制
- 输入桥接：[components/system/drivers/lv_port_indev.cpp](../components/system/drivers/lv_port_indev.cpp)
  支持 CST128 触摸 / 鼠标 / 编码器（`conf.h` 开关）
- LVGL tick：1ms 周期 `esp_timer`（见 [init_tasks.cpp](../main/init_tasks.cpp) `lvgl_tick_timer_init`）
- LVGL timer handler：独立任务 `lvgl_timer`，核心 1，24KB 栈

**调用顺序铁律**：`lv_init()` → `lv_port_disp_init()` → `lv_port_indev_init()` → 任何 LVGL 对象创建。
顺序错会触发 assert。

#### Page Manager

[components/ui/page_manager.h](../components/ui/page_manager.h)

基于栈的页面管理，支持：
- `gotoPage(name)` / `gotoPageAndDestroy(name)`
- 带动画版本：`gotoPage(name, anim, time)`
- `back()` 弹栈

页面用 `registerPage(name, factory)` 注册工厂函数。新增页面步骤：

1. 在 `components/ui/` 新建 `page_xxx.cpp` 和 `page_xxx.h`
2. 在 `page_xxx.cpp` 实现 `lv_obj_t* create_xxx_page()`，返回根对象
3. 在 `page_manager` 注册：`pm.registerPage("xxx", create_xxx_page)`
4. 在 `components/ui/CMakeLists.txt` 的 SRCS 列表加入 `page_xxx.cpp`
   （若用 `GLOB`，则无需手动加）

#### WiFi Manager

[components/system/services/wifi_manager.c](../components/system/services/wifi_manager.c)

完整的 STA 模式 WiFi 管理 + SNTP 时间同步。关键能力：
- 连接/断开/扫描（阻塞或异步）
- BSSID 锁定（解决部分路由器/热点兼容性问题，详见 [wifi-debug-notes.md](wifi-debug-notes.md)）
- SNTP 时间同步（连接成功后自动启动）
- 状态机 + 回调通知
- 开关控制 + NVS 持久化
- 线程安全（mutex 保护 + event group）

**重要修复**（已在代码中）：
1. **BSSID 锁定**：连接前先做带 SSID 的定向扫描，拿到 AP 的 BSSID/channel 后写入
   `wifi_config.sta.bssid` 并切换为 `WIFI_FAST_SCAN`，让驱动跳过内部扫描。
2. **stale event bit 修复**：`esp_wifi_connect()` 前先 `xEventGroupClearBits`，
   避免上一次连接遗留的 `WIFI_CONNECTED_BIT`/`WIFI_FAIL_BIT` 让
   `xEventGroupWaitBits` 立即返回旧状态。

#### 低功耗设计

[main/system_low_power_design.hpp](../main/system_low_power_design.hpp)

LVGL 功耗管理器，根据屏幕活动状态动态调整刷新率：
- `NORMAL`: 5ms（活动）
- `LOW_POWER`: 20ms
- `SLEEP`: 100ms（息屏）
- `DEEP_SLEEP`: 暂停定时器

亮度控制 [components/system/drivers/lcd_brightness.cpp](../components/system/drivers/lcd_brightness.cpp)
配合实现自动息屏和亮度调节。

### 1.4 配置开关

全局开关集中在 [components/system/conf.h](../components/system/conf.h)：

| 宏 | 说明 |
|----|------|
| `MY_DISP_HOR_RES` / `MY_DISP_VER_RES` | 屏幕分辨率 |
| `OFFSET_ROTATION` | 横竖屏切换 |
| `USE_LGFX` / `USE_eTFT` | 显示驱动选择 |
| `USE_PSRAM_FOR_BUFFER` | 用 PSRAM 作显示缓冲 |
| `USE_TOUCHPAD` / `USE_MOUSE` / `USE_ENCODER` | 输入设备开关 |
| `USE_UAC_AUDIO` | USB Audio Class |
| `disp_*` / `TOUCH_PAD_NUM` / `LED_STRIP_*` | 引脚定义 |

修改 `conf.h` 后必须 `idf.py fullclean && idf.py build`。

## 2. ESP-IDF v5.2 → v6.0.2 升级笔记

升级主要变更点（用于回溯和二次开发参考）：

### 2.1 组件系统变更

- IDF 6.0 强制 `idf_component_register` 的 `REQUIRES` / `PRIV_REQUIRES` 必须列出
  所有依赖组件，5.x 部分隐式依赖不再自动可用。
- 各 `components/*/CMakeLists.txt` 已更新 `REQUIRES`。

### 2.2 Driver API 变更

- 旧 `driver/i2c.h` 的 `i2c_param_config` / `i2c_master_write_to_device` 在 6.0 标记
  废弃，新代码改用 `driver/i2c_master.h` 的新 master bus API。
- I2C 初始化 [components/system/drivers/i2c_init.cpp](../components/system/drivers/i2c_init.cpp)
  已适配。
- LEDC、GPIO、SPI 等基本兼容。

### 2.3 FreeRTOS

- `xTaskCreate` 推荐改 `xTaskCreatePinnedToCore` 显式指定 core。
- IDF 6.0 默认开启 `configENABLE_BACKWARD_COMPATIBILITY=y`，旧 API 仍可用。

### 2.4 WiFi / 网络栈

- `esp_wifi` API 基本兼容；建议启用 PMF (`pmf_cfg.capable = true`) 以兼容 WPA3 transition。
- SNTP 改用 `esp_netif_sntp.h` 的新 API。

### 2.5 sdkconfig

升级后 sdkconfig 有 ~1700 行变更，主要是：
- 新增 `CONFIG_IDF_TARGET_*` 选项
- PSRAM 配置从 `CONFIG_ESP32S3_SPIRAM_SUPPORT` 改为 `CONFIG_SPIRAM`
- FreeRTOS 配置项重命名
- ccache、其它工具链选项更新

如需重新生成 sdkconfig：备份当前 `sdkconfig`，删除后 `idf.py reconfigure`，
再 `idf.py menuconfig` 按需配置。

## 3. lvgl 子模块本地补丁

**问题**：lvgl 9.3 的 ThorVG 子库在 ESP-IDF 6.0.2 工具链下会触发多个
`-Werror=format`、`-Werror=type-limits` 等警告转错误，导致编译失败。

**解决**：在 [components/lvgl/env_support/cmake/esp.cmake](../components/lvgl/env_support/cmake/esp.cmake)
中追加一段代码，对 ThorVG 源文件统一关闭 `-Wno-error`：

```cmake
# ThorVG 是第三方库，多种 -Werror=xxx 警告(format/type-limits 等)无需逐个修，统一关闭警告转错误
file(GLOB_RECURSE THORVG_SOURCES ${LVGL_ROOT_DIR}/src/libs/thorvg/*.cpp)
set_source_files_properties(${THORVG_SOURCES} COMPILE_FLAGS "-Wno-error")
```

补丁位置：在 `target_compile_definitions(... LV_CONF_INCLUDE_SIMPLE)` 之后。

### 重新应用补丁

如果 `git submodule update --init` 或 `--force` 后补丁丢失，按以下步骤重新应用：

**方式 A** —— 直接编辑文件（推荐，简单）

用编辑器打开 `components/lvgl/env_support/cmake/esp.cmake`，找到
`target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_CONF_INCLUDE_SIMPLE")`
这一行，在其后插入上面的 3 行 CMake 代码。

**方式 B** —— 用脚本自动应用

```powershell
.\scripts\apply-lvgl-patch.ps1
```

脚本会检测补丁是否已存在，没有则自动应用。

### 为什么不 fork lvgl？

fork 后需要长期维护与上游的同步，对一个 3 行补丁而言成本太高。
当前方案是「子模块 + 本地补丁 + 文档说明 + 自动化脚本」，性价比最高。

## 4. 二次开发指南

### 4.1 新增一个 LVGL 页面

以「新增传感器页面」为例：

1. 在 `components/ui/` 创建 `page_sensor.cpp` 和 `page_sensor.h`，实现：
   ```cpp
   // page_sensor.h
   #pragma once
   #include "lvgl/lvgl.h"
   lv_obj_t* create_sensor_page();
   ```
2. 在 `page_sensor.cpp` 中创建页面控件、绑定回调。
3. 在 `my_ui.cpp` 或 `page_manager` 中注册：`pm.registerPage("sensor", create_sensor_page);`
4. 在某个入口页面（如 `page_menu.cpp`）添加按钮，点击 `pm.gotoPage("sensor")`。
5. 修改 `components/ui/CMakeLists.txt`（若用 `file(GLOB)` 则无需手动加）。
6. 编译烧录验证。

### 4.2 新增一个驱动

1. 在 `components/system/drivers/` 创建 `xxx.cpp` 和 `xxx.h`。
2. 实现 `xxx_init()` 等接口。
3. 在 [init_tasks.cpp](../main/init_tasks.cpp) 的 `DRIVERS` 阶段注册：
   ```cpp
   initializer.add_task(InitStage::DRIVERS, "XXX", xxx_init, false);
   ```
4. 在 [components/system/CMakeLists.txt](../components/system/CMakeLists.txt)
   的 `REQUIRES` 添加依赖组件（如 `driver`）。
5. 路线：如果是 I2C 设备，复用 [i2c_init.cpp](../components/system/drivers/i2c_init.cpp)
   提供的总线句柄，不要再独立初始化 I2C。

### 4.3 新增一个服务

服务通常是一个长期任务 + 一组对外接口。模板：

```cpp
// my_service.h
#pragma once
#include "esp_err.h"
esp_err_t my_service_init(void);
esp_err_t my_service_get_value(int* out);
```

```cpp
// my_service.cpp
#include "my_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "thread_wrapper.hpp"

static void service_task(void*) {
    while (true) {
        // 周期采样
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
esp_err_t my_service_init(void) {
    xTaskCreatePinnedToCore(service_task, "my_svc", 4096, nullptr, 5, nullptr, 0);
    return ESP_OK;
}
```

注册到 [init_tasks.cpp](../main/init_tasks.cpp) 的 `SERVICES` 阶段。

### 4.4 添加第三方 IDF 组件

1. 编辑 [main/idf_component.yml](../main/idf_component.yml)，添加依赖：
   ```yaml
   dependencies:
     espressif/xxx: "^1.0.0"
   ```
2. 运行 `idf.py reconfigure`，组件会下载到 `managed_components/`。
3. 在使用该组件的 `CMakeLists.txt` 的 `REQUIRES` 加入 `xxx`。
4. 提交 `main/idf_component.yml` 和 `dependencies.lock`（这两个必须提交）。
5. **不要**提交 `managed_components/` 目录。

### 4.5 添加 git submodule 组件

```powershell
git submodule add https://github.com/xxx/yyy.git components/yyy
```

然后像普通组件一样在 `REQUIRES` 中引用。

## 5. 已知问题与解决方法

### 5.1 中断处理函数中调用不安全函数

**现象**：在 GPIO ISR 中调用 `ESP_LOGI` 等会触发 assert 或硬 fault。

**原因**：ISR 中不能调用会获取锁/分配内存的函数。`ESP_LOGx` 默认走 `vprintf`，
可能触发调度。

**解决**：
- ISR 中只做 `xQueueSendFromISR` / `xEventGroupSetBitsFromISR` 等带 `FromISR` 后缀的 API。
- 日志改成在任务上下文中打印，或用 `ESP_DRAM_LOGI`（仅 IDR 6.0+）。
- 复杂处理放到 task 中：ISR 用 `xTaskNotifyFromISR` 唤醒 task。

### 5.2 Timer Service 栈溢出

**现象**：`ERROR: A stack overflow in task Tmr Svc has been detected.`

**原因**：FreeRTOS 默认 Timer Service 任务栈 2048 不够，复杂定时器回调会溢出。

**解决**：menuconfig 调大：
```
Component config → FreeRTOS → (4096) Timer stack size
```

### 5.3 LVGL 对象创建前未初始化显示

**现象**：`assert: lv_obj.c: ...` 或屏幕花屏。

**解决**：确保 `lv_init` → `lv_port_disp_init` → `lv_port_indev_init` → UI 创建
的顺序。[init_tasks.cpp](../main/init_tasks.cpp) 已按此顺序注册。

### 5.4 WiFi 连不上某些路由器

详见 [wifi-debug-notes.md](wifi-debug-notes.md)。简而言之：
- 部分路由器固件不响应 ESP32 的 auth 帧（reason=2 AUTH_EXPIRE），手机能连，根因是
  路由器-芯片组兼容性问题，无解。
- 手机热点可用 BSSID 锁定方案连上，代码已实现。

### 5.5 managed_components 被误提交

历史上有一次 `managed_components/` 整个目录被提交到 git，导致每次构建后
`git status` 出现 2000+ 文件变更。已在 `chore: stop tracking build artifacts`
提交中清理：

- `managed_components/` 加入 `.gitignore`
- 用 `git rm -r --cached` 从索引移除（本地文件保留）
- `dependencies.lock` 保留提交，用于锁版本

**复现预防**：永远不要 `git add managed_components/`；新增组件只改
`main/idf_component.yml` 和 `dependencies.lock`。

### 5.6 CRLF 行尾警告

部分文件（`axp2101.cpp`, `pca9554.cpp`）以 LF 保存，git 在 Windows 上会提示
`LF will be replaced by CRLF`。这是无害警告，可忽略。如需消除，添加
`.gitattributes`：
```
*.cpp text eol=lf
*.hpp text eol=lf
*.c   text eol=lf
*.h   text eol=lf
```

## 6. 编码规范

- C++ 文件 `.cpp/.hpp`，C 文件 `.c/.h`；C 头文件用 `#ifdef __cplusplus` 包裹。
- 模块对外接口在头文件中声明，内部函数 `static`。
- 日志统一用 `ESP_LOGI/W/E/D`，每个 .c/.cpp 文件顶部 `static const char *TAG = "模块名";`。
- 错误返回 `esp_err_t`，用 `ESP_ERROR_CHECK` 仅在初始化阶段。
- 资源（任务、队列、信号量）创建后必须能被销毁，便于模块重启。
- 注释用中文，代码标识符用英文。

## 7. 调试

### 7.1 串口日志

用 [scripts/build_flash_monitor.ps1](../scripts/build_flash_monitor.ps1) 监视并自动存档。

### 7.2 OpenOCD / JTAG

ESP32-S3 原生 USB-Serial/JTAG 支持 JTAG 调试，无需外接调试器。

```powershell
# 启动 OpenOCD（使用内置 cfg）
idf.py openocd
```

VSCode 配置见 `.vscode/launch.json`（如已删除，参考 IDF 模板生成）。

### 7.3 性能分析

`TaskManager` 周期打印 top-like 输出（[init_tasks.cpp](../main/init_tasks.cpp) 中
`task_manager()`），可观察各任务栈使用率。

启用 `CONFIG_FREERTOS_USE_TRACE_FACILITY=y` 后可使用 `vTaskList` / `uxTaskGetStackHighWaterMark`。
