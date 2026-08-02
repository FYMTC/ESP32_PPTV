# ESP32-S3 PPTV 多线程 LVGL 应用

基于 **ESP32-S3-SoC-N16R8**（16MB Flash + 8MB PSRAM）的 C++ 多线程 GUI 项目，
使用 ESP-IDF v6.0.2 + LVGL 9.3 + LovyanGFX 驱动 2.4 寸 240×320 LCD。

## 硬件配置

| 模块 | 型号 / 说明 |
|------|-------------|
| MCU | ESP32-S3, 16MB Flash, 8MB PSRAM |
| 屏幕 | 2.4 寸 240×320 SPI LCD (ST7789 兼容) |
| 触摸 | CST128 I2C 电容触摸 |
| PMU | AXP2101 电源管理 |
| IO 扩展 | PCA9554 / TCA9554 |
| RTC | DS1302 |
| IMU | MPU6050 |
| 磁力计 | QMC5883L |
| LED | WS2812 单颗 (GPIO48) |
| 串口 | 原生 USB-Serial/JTAG (VID_303A&PID_1001), COM5 |

详细引脚配置见 [components/system/conf.h](components/system/conf.h)，
分区表见 [partitions.csv](partitions.csv)。

## 软件栈

- **ESP-IDF**: v6.0.2（从 v5.2 升级而来，升级说明见 [docs/developer-guide.md](docs/developer-guide.md)）
- **LVGL**: 9.3 (git submodule, `components/lvgl`)
- **LovyanGFX**: `components/LovyanGFX` (git submodule, 用于显示驱动)
- **FreeRTOS**: 随 IDF 提供
- **第三方组件**: 通过 ESP Component Registry 拉取，见 [main/idf_component.yml](main/idf_component.yml)

## 目录结构

```
ESP32_PPTV/
├── main/                     # 主程序入口 (app_main)
├── components/
│   ├── system/               # 自研系统层
│   │   ├── drivers/          # 硬件驱动 (LCD/TP/PMU/RTC/I2C/PCA9554...)
│   │   ├── services/         # 服务层 (WiFi/Time/Battery/PMU/MPU6050...)
│   │   ├── storage/          # NVS / SD 卡
│   │   ├── init.cpp/hpp      # 分阶段系统初始化器
│   │   └── conf.h            # 全局引脚/功能开关
│   ├── ui/                   # LVGL 页面 (page_*.cpp) + 页面管理器
│   ├── lvgl_app/             # LVGL 应用层封装 (page_manager)
│   ├── thread_manager/       # 线程池 / 任务调度 / ThreadWrapper
│   ├── lvgl/                 # [submodule] LVGL 9.3
│   ├── LovyanGFX/            # [submodule] 显示驱动
│   ├── XPowersLib/           # AXP 系列 PMU 驱动
│   └── lv_conf.h             # LVGL 配置
├── docs/                     # 项目文档（见下）
├── scripts/                  # 辅助脚本（编译/烧录/监视）
├── CMakeLists.txt            # 顶层 CMake
├── partitions.csv            # 分区表
├── sdkconfig                 # IDF 配置
├── dependencies.lock         # 组件版本锁（提交到 git）
└── .gitmodules               # 子模块声明
```

> `managed_components/` 和 `build/` 由构建工具自动生成，**不提交到 git**。

## 快速开始

### 1. 环境准备

安装 ESP-IDF v6.0.2，参考 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/v6.0.2/esp32s3/get-started/)。

Windows 推荐使用官方 installer，安装后通过 PowerShell 配置环境：

```powershell
. 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'
```

或手动设置（按实际安装路径调整）：

```powershell
$env:IDF_PATH     = "C:\Espressif\v6.0.2\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif"
. "$env:IDF_PATH\export.ps1"
```

### 2. 克隆仓库（含子模块）

```powershell
git clone --recurse-submodules https://github.com/FYMTC/ESP32_PPTV.git
cd ESP32_PPTV
```

如果已经 clone 但忘了 `--recurse-submodules`：

```powershell
git submodule update --init --recursive
```

> ⚠️ 子模块 `components/lvgl` 包含一个本地补丁（关闭 ThorVG 的 `-Werror`），
> `git submodule update` 后需重新应用，详见
> [docs/developer-guide.md → lvgl 本地补丁](docs/developer-guide.md#3-lvgl-子模块本地补丁)。

### 3. 选择串口

ESP32-S3 原生 USB-Serial/JTAG 通常识别为 `COM5`（设备管理器中 VID_303A&PID_1001）。
推荐使用此串口；CH343 桥（COM4）驱动在部分 Windows 上有兼容性问题。

### 4. 编译 / 烧录 / 监视

**方式 A — 使用辅助脚本（推荐）**：

```powershell
# 一键：编译 + 烧录 + 监视（默认 COM5, 921600 波特率），日志自动保存到 logs/
.\scripts\build_flash_monitor.ps1

# 仅编译
.\scripts\build_flash_monitor.ps1 -Action build

# 指定串口
.\scripts\build_flash_monitor.ps1 -Port COM7

# 烧录后不监视，仅保存编译日志
.\scripts\build_flash_monitor.ps1 -Action flash -NoMonitor
```

脚本说明见 [docs/skills.md](docs/skills.md)。

**方式 B — 原生 IDF 命令**：

```powershell
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 -b 921600 flash monitor
```

### 5. 修改 WiFi 配置

默认 WiFi SSID/密码硬编码在 [components/system/services/wifi_manager.c](components/system/services/wifi_manager.c) 顶部的 `DEFAULT_WIFI_SSID` / `DEFAULT_WIFI_PASSWORD`，运行时通过 `wifi_manager_connect_to_ap(ssid, password)` 覆盖。
UI 中的 WiFi 页面可扫描并连接指定 AP（含 BSSID 锁定，详见开发者文档）。

## 文档导航

| 文档 | 内容 |
|------|------|
| [docs/skills.md](docs/skills.md) | IDF 开发流程：终端创建、编译、烧录、监视、日志检查 |
| [docs/developer-guide.md](docs/developer-guide.md) | 软件架构、二次开发指南、已踩过的坑及解决方法 |
| [docs/todo.md](docs/todo.md) | TODO 待办备忘 |
| [docs/dev-log.md](docs/dev-log.md) | 开发日志（历史变更） |
| [docs/wifi-debug-notes.md](docs/wifi-debug-notes.md) | WiFi 兼容性调试笔记（路由器/热点连接问题） |

## 许可证

本项目代码遵循各自组件的许可证；自研代码部分作者保留版权。
LVGL / LovyanGFX / ESP-IDF 等第三方代码遵循其原始许可证。
