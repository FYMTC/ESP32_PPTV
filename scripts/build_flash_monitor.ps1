<#
.SYNOPSIS
    ESP32 PPTV 项目标准构建/烧录/监视脚本。

.DESCRIPTION
    固定流程：编译 -> 烧录 -> 监视串口，并将所有输出保存到 logs/ 目录。
    默认参数针对本项目：ESP32-S3, COM5, 921600 波特率。
    AI agent 和开发者都应优先使用此脚本，保证流程一致、日志可回溯。

.PARAMETER Action
    执行哪个动作：build / flash / monitor / all（默认 all = build+flash+monitor）

.PARAMETER Port
    串口号，默认 COM5（ESP32-S3 原生 USB-Serial/JTAG）。

.PARAMETER Baud
    烧录波特率，默认 921600。

.PARAMETER Target
    目标芯片，默认 esp32s3。

.PARAMETER NoMonitor
    加此开关则烧录后不进入 monitor（用于自动化场景）。

.PARAMETER Clean
    加此开关则先 idf.py fullclean 再 build。

.PARAMETER LogDir
    日志保存目录，默认 logs（相对当前目录）。

.EXAMPLE
    .\scripts\build_flash_monitor.ps1
    # 一键编译+烧录+监视，日志保存到 logs/

.EXAMPLE
    .\scripts\build_flash_monitor.ps1 -Action build
    # 仅编译

.EXAMPLE
    .\scripts\build_flash_monitor.ps1 -Port COM7 -Clean
    # 切换串口并先清理再编译

.EXAMPLE
    .\scripts\build_flash_monitor.ps1 -Action flash -NoMonitor
    # 仅烧录，不监视

.NOTES
    退出码：
      0 = 成功
      1 = IDF 环境未加载
      2 = 编译失败
      3 = 烧录失败
      4 = monitor 异常退出（Ctrl+] 退出算正常）
#>
[CmdletBinding()]
param(
    [ValidateSet('build','flash','monitor','all')]
    [string]$Action = 'all',

    [string]$Port = 'COM5',

    [int]$Baud = 921600,

    [string]$Target = 'esp32s3',

    [switch]$NoMonitor,

    [switch]$Clean,

    [string]$LogDir = 'logs'
)

$ErrorActionPreference = 'Stop'

# ---------- 工具函数 ----------
function Write-Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-OK($msg)   { Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Err($msg)  { Write-Host "[ERR] $msg" -ForegroundColor Red }

function Test-IdfLoaded {
    return ($null -ne (Get-Command idf.py -ErrorAction SilentlyContinue))
}

function Invoke-Idf {
    param(
        [Parameter(Mandatory)][string[]]$Args,
        [string]$LogFile
    )
    if ($LogFile) {
        # 同时输出到控制台和日志文件
        & idf.py @Args 2>&1 | Tee-Object -FilePath $LogFile
    } else {
        & idf.py @Args
    }
    return $LASTEXITCODE
}

# ---------- 前置检查 ----------

if (-not (Test-IdfLoaded)) {
    Write-Err "idf.py 未找到。请先加载 ESP-IDF 环境："
    Write-Host "    . 'C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1'"
    Write-Host "或:"
    Write-Host "    `$env:IDF_PATH='C:\Espressif\v6.0.2\esp-idf'; . `"$env:IDF_PATH\export.ps1`""
    exit 1
}

# 切到工程根目录（脚本位于 scripts/ 下，工程根是其父目录）
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
Set-Location $ProjectRoot
Write-Step "Project root: $ProjectRoot"

# 准备日志目录
if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir | Out-Null
}
$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$buildLog   = Join-Path $LogDir "build_$timestamp.log"
$flashLog   = Join-Path $LogDir "flash_$timestamp.log"
$monitorLog = Join-Path $LogDir "monitor_$timestamp.log"

# ---------- set-target（首次） ----------

if (-not (Test-Path "build") -or -not (Test-Path "build\CMakeCache.txt")) {
    Write-Step "First-time setup: idf.py set-target $Target"
    & idf.py set-target $Target
    if ($LASTEXITCODE -ne 0) { Write-Err "set-target failed"; exit 2 }
}

# ---------- Clean ----------

if ($Clean) {
    Write-Step "idf.py fullclean"
    & idf.py fullclean
    if ($LASTEXITCODE -ne 0) { Write-Err "fullclean failed"; exit 2 }
}

# ---------- Build ----------

if ($Action -eq 'build' -or $Action -eq 'all') {
    Write-Step "Build (log: $buildLog)"
    $rc = Invoke-Idf -Args @('build') -LogFile $buildLog
    if ($rc -ne 0) {
        Write-Err "Build failed (exit $rc). See: $buildLog"
        exit 2
    }
    Write-OK "Build succeeded"
}

# ---------- Flash ----------

if ($Action -eq 'flash' -or $Action -eq 'all') {
    Write-Step "Flash to $Port at $Baud (log: $flashLog)"
    $rc = Invoke-Idf -Args @('-p', $Port, '-b', $Baud, 'flash') -LogFile $flashLog
    if ($rc -ne 0) {
        Write-Err "Flash failed (exit $rc). See: $flashLog"
        exit 3
    }
    Write-OK "Flash succeeded"
}

# ---------- Monitor ----------

if ($Action -eq 'monitor' -or ($Action -eq 'all' -and -not $NoMonitor)) {
    Write-Step "Monitor $Port (log: $monitorLog)"
    Write-Host "    Press Ctrl+] to exit monitor"
    Write-Host "    (Ctrl+C triggers chip reset, do NOT use to exit)"
    try {
        # monitor 不用 Tee，直接重定向，避免 Ctrl+] 退出时 Tee 卡住
        & idf.py -p $Port monitor 2>&1 | Tee-Object -FilePath $monitorLog
        $rc = $LASTEXITCODE
    } catch {
        Write-Host "Monitor exited: $_"
        $rc = 0
    }
    Write-Host ""
    Write-OK "Monitor ended. Log saved to: $monitorLog"
}

# ---------- 完成 ----------

Write-Host ""
Write-Step "Done. Logs in $LogDir/:"
Get-ChildItem $LogDir -Filter "*_$timestamp.log" | ForEach-Object {
    Write-Host ("  {0,-40} {1,8} bytes" -f $_.Name, $_.Length)
}

exit 0
