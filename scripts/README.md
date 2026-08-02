# Scripts — 项目辅助脚本

本目录的脚本面向 Windows PowerShell + ESP-IDF v6.0.2 环境，封装了固定开发流程。

## 脚本列表

| 脚本 | 用途 |
|------|------|
| [build_flash_monitor.ps1](build_flash_monitor.ps1) | 一键编译/烧录/监视串口，日志自动存档到 `logs/` |
| [apply-lvgl-patch.ps1](apply-lvgl-patch.ps1) | 重新应用 lvgl 子模块的 ThorVG -Werror 补丁 |

## 前置条件

每次新开 PowerShell 终端，**必须先加载 ESP-IDF 环境**：

```powershell
. 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'
```

或在脚本调用前手动 export：

```powershell
$env:IDF_PATH = 'C:\Espressif\v6.0.2\esp-idf'
. "$env:IDF_PATH\export.ps1"
```

脚本会自动检测 `idf.py` 是否可用，未加载时报错退出。

## build_flash_monitor.ps1

### 默认调用（最常用）

```powershell
.\scripts\build_flash_monitor.ps1
```

等价于：`idf.py build` → `idf.py -p COM5 -b 921600 flash` → `idf.py -p COM5 monitor`，
所有输出存到 `logs/build_*.log`、`logs/flash_*.log`、`logs/monitor_*.log`。

### 参数

| 参数 | 默认 | 说明 |
|------|------|------|
| `-Action` | `all` | `build` / `flash` / `monitor` / `all` |
| `-Port` | `COM5` | 串口 |
| `-Baud` | `921600` | 烧录波特率 |
| `-Target` | `esp32s3` | 目标芯片 |
| `-NoMonitor` | off | 烧录后不监视 |
| `-Clean` | off | 先 `idf.py fullclean` |
| `-LogDir` | `logs` | 日志目录 |

### 退出码

| 码 | 含义 |
|----|------|
| 0 | 成功 |
| 1 | IDF 环境未加载 |
| 2 | 编译失败 |
| 3 | 烧录失败 |
| 4 | monitor 异常 |

### 退出 monitor

`Ctrl + ]`（不是 Ctrl+C，Ctrl+C 会触发芯片复位）。

## apply-lvgl-patch.ps1

无参数。检测 `components/lvgl/env_support/cmake/esp.cmake` 是否已含
`THORVG_SOURCES`，没有则自动插入补丁块。

使用场景：`git submodule update --init --force` 或 `git submodule update --remote`
之后。

```powershell
.\scripts\apply-lvgl-patch.ps1
```

补丁详情见 [docs/developer-guide.md §3](../docs/developer-guide.md#3-lvgl-子模块本地补丁)。

## agent 使用约定

AI agent 在执行构建任务时：

1. 优先调用 `build_flash_monitor.ps1`，不要直接拼 `idf.py` 命令
2. 构建后用 `LS logs/` 找最新日志，用 `Read` 检查：
   - build 日志：搜索 `error:` / `FAILED` / `Project build complete`
   - monitor 日志：搜索 `ESP_LOGE` / `Guru Meditation` / `Backtrace` / `reason:`
3. 失败时定位文件行号，修复后重跑（用 `-Clean` 强制重新编译）

## 扩展脚本

如需新增脚本，遵循：

- PowerShell 5.1+ 兼容（不要用 PowerShell 7 专有语法）
- 顶部 `[CmdletBinding()]` + `param()` + `.SYNOPSIS` 注释
- 关键步骤 `Write-Step` / `Write-OK` / `Write-Err` 着色输出
- 日志保存到 `logs/` 目录
- 退出码语义清晰
