# Skills — ESP-IDF 开发流程

本文档定义本项目的标准开发流程：**如何创建终端、配置环境、编译、烧录、监视串口、
检查日志**。固定流程可让 AI agent 和开发者按一致的方式工作。

## 1. 环境

- **操作系统**: Windows 10/11 + PowerShell
- **ESP-IDF 版本**: v6.0.2
- **目标芯片**: ESP32-S3
- **默认串口**: COM5（ESP32-S3 原生 USB-Serial/JTAG, VID_303A&PID_1001）
- **波特率**: 921600（烧录），115200（监视）

## 2. 创建 PowerShell 终端并加载 IDF 环境

每次新开终端必须先加载 IDF 环境，否则 `idf.py` 不可用。

### 方式 A — 加载官方 profile（推荐）

```powershell
. 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'
```

### 方式 B — 手动 export

```powershell
$env:IDF_PATH       = "C:\Espressif\v6.0.2\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif"
. "$env:IDF_PATH\export.ps1"
```

### 验证环境

```powershell
idf.py --version
```

输出类似 `ESP-IDF v6.0.2` 即可。

## 3. 标准流程：一键编译 + 烧录 + 监视

使用项目辅助脚本 [scripts/build_flash_monitor.ps1](../scripts/build_flash_monitor.ps1)
完成固定流程。脚本默认行为：

1. 调用 `idf.py build` 编译工程
2. 调用 `idf.py -p <PORT> -b 921600 flash` 烧录
3. 调用 `idf.py -p <PORT> monitor` 监视串口
4. **将 build 和 monitor 的输出保存到 `logs/build_YYYYMMDD_HHmmss.log` 和
   `logs/monitor_YYYYMMDD_HHmmss.log`**，便于 agent / 开发者事后检查

### 默认调用（最常用）

```powershell
.\scripts\build_flash_monitor.ps1
```

### 常用参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `-Action` | `all` | `build` / `flash` / `monitor` / `all`（build+flash+monitor） |
| `-Port` | `COM5` | 串口号 |
| `-Baud` | `921600` | 烧录波特率 |
| `-Target` | `esp32s3` | 目标芯片（一般无需改） |
| `-NoMonitor` | `$false` | 加此开关则烧录后不进入 monitor |
| `-Clean` | `$false` | 加此开关则先 `idf.py fullclean` 再 build |
| `-LogDir` | `logs` | 日志保存目录 |

### 示例

```powershell
# 仅编译，日志保存到 logs/
.\scripts\build_flash_monitor.ps1 -Action build

# 完整流程，强制先清理
.\scripts\build_flash_monitor.ps1 -Clean

# 切换串口
.\scripts\build_flash_monitor.ps1 -Port COM7

# 烧录但不监视（用于自动化）
.\scripts\build_flash_monitor.ps1 -Action flash -NoMonitor
```

### 退出 monitor

`Ctrl + ]` 退出 `idf.py monitor`（不是 Ctrl+C，Ctrl+C 会触发芯片复位）。

## 4. 检查日志

脚本运行后，日志保存到工程根目录的 `logs/` 子目录（已加入 `.gitignore`）：

```
logs/
├── build_20260802_153012.log     # 编译输出
├── monitor_20260802_153045.log   # 串口监视输出
└── ...
```

### agent 检查日志的标准步骤

1. 用 `LS` 列出 `logs/` 目录，找到最新的 `build_*.log` 和 `monitor_*.log`
2. 用 `Read` 读取：
   - build 日志：检查结尾 `Project build complete. ...` 字样和 `Generated XX.bin` 行
   - monitor 日志：检查 `ESP_LOGE` / `assert` / `Guru Meditation` / `backtrace` 等错误关键字
3. 如果有错误，定位到具体源文件和行号，修复后重新跑流程

### 检查错误的常用关键字

```
error:            # 编译错误
fatal error:      # 致命编译错误
FAILED:           # ninja 构建失败
ESP_LOGE          # 运行时错误日志
Guru Meditation   # 崩溃 / panic
Backtrace:        # 调用栈
assert            # 断言失败
reason:           # WiFi/网络断开原因码
```

## 5. 原生命令（不用脚本时）

如果脚本不可用，可直接用 `idf.py`：

```powershell
# 设定目标（首次或换芯片时）
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录 + 监视
idf.py -p COM5 -b 921600 flash monitor

# 仅监视
idf.py -p COM5 monitor

# 清理
idf.py fullclean

# menuconfig
idf.py menuconfig
```

## 6. 修改配置（menuconfig）

常用配置项位置：

| 配置 | 路径 |
|------|------|
| Flash 大小 | `Serial flasher config → Flash size` |
| PSRAM | `Component config → ESP PSRAM` |
| 分区表 | `Partition Table → Custom partition CSV file` |
| FreeRTOS 栈大小 | `Component config → FreeRTOS → Timer stack size` |
| WiFi | `Component config → Wi-Fi` |
| LVGL | `Component config → LVGL configuration` |

修改后保存，`sdkconfig` 会自动更新（已提交到 git，请慎重提交无关变更）。

## 7. 子模块操作

```powershell
# 克隆后初始化子模块
git submodule update --init --recursive

# 更新子模块到最新（慎用，会丢失本地补丁）
git submodule update --remote

# 查看子模块状态
git submodule status
```

> ⚠️ `components/lvgl` 有本地补丁，`git submodule update` 后必须重新应用，
> 详见 [developer-guide.md → lvgl 本地补丁](developer-guide.md#3-lvgl-子模块本地补丁)。

## 8. 组件管理

第三方组件通过 ESP Component Registry 拉取，声明在 [main/idf_component.yml](../main/idf_component.yml)，版本锁在 [dependencies.lock](../dependencies.lock)。

```powershell
# 重新拉取/同步组件
idf.py reconfigure

# 添加新组件：编辑 main/idf_component.yml 后
idf.py reconfigure
```

`managed_components/` 是组件的下载位置，**不提交到 git**。

## 9. 排错速查

| 现象 | 可能原因 / 解决 |
|------|-----------------|
| `idf.py : 无法识别` | 终端未加载 IDF 环境，重新执行 `export.ps1` |
| `A fatal error occurred: Failed to connect to ESP32-S3` | 串口被占用 / 按住 BOOT 再按 RST 进入下载模式 |
| 烧录失败 `error: cannot open port COM5` | 关闭其他占用 COM5 的程序（VSCode monitor、PuTTY 等） |
| 编译 OOM / 卡死 | 减少并行数：`idf.py build -j4` 或在 menuconfig 关闭 ccache |
| `WERROR` 导致编译失败 | 见 [developer-guide.md → lvgl 补丁](developer-guide.md#3-lvgl-子模块本地补丁) |
| 启动后不断重启 | 检查电源 / PSRAM 配置 / 分区表是否与 Flash 大小匹配 |
| WiFi 连不上路由器 | 见 [wifi-debug-notes.md](wifi-debug-notes.md) |
