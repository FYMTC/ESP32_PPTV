<#
.SYNOPSIS
    重新应用 lvgl 子模块的 ThorVG -Werror 补丁。

.DESCRIPTION
    components/lvgl 是 git submodule，本项目的 ThorVG -Wno-error 补丁在
    `git submodule update --init` 或 `--force` 后会丢失。
    本脚本检测补丁是否已存在，没有则自动应用。

    补丁内容：在 components/lvgl/env_support/cmake/esp.cmake 中，
    target_compile_definitions(... LV_CONF_INCLUDE_SIMPLE) 行之后插入：
        file(GLOB_RECURSE THORVG_SOURCES ${LVGL_ROOT_DIR}/src/libs/thorvg/*.cpp)
        set_source_files_properties(${THORVG_SOURCES} COMPILE_FLAGS "-Wno-error")

.EXAMPLE
    .\scripts\apply-lvgl-patch.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$EspCmake = Join-Path $ProjectRoot 'components\lvgl\env_support\cmake\esp.cmake'

function Write-Step($m) { Write-Host "==> $m" -ForegroundColor Cyan }
function Write-OK($m)   { Write-Host "[OK] $m" -ForegroundColor Green }
function Write-Err($m)  { Write-Host "[ERR] $m" -ForegroundColor Red }
function Write-Warn($m) { Write-Host "[!]  $m" -ForegroundColor Yellow }

if (-not (Test-Path $EspCmake)) {
    Write-Err "File not found: $EspCmake"
    Write-Err "Did you run 'git submodule update --init --recursive' first?"
    exit 1
}

# 补丁标志：搜索是否已有 THORVG_SOURCES
$content = Get-Content -Path $EspCmake -Raw
if ($content -match 'THORVG_SOURCES') {
    Write-OK "Patch already applied. Nothing to do."
    Write-Host "    File: $EspCmake"
    exit 0
}

Write-Step "Applying ThorVG -Wno-error patch to: $EspCmake"

# 定位插入点：target_compile_definitions(... LV_CONF_INCLUDE_SIMPLE)
# 多种写法兼容
$anchorPatterns = @(
    'target_compile_definitions\(\$\{COMPONENT_LIB\}\s+PUBLIC\s+"-DLV_CONF_INCLUDE_SIMPLE"\)',
    'LV_CONF_INCLUDE_SIMPLE'
)

# 我们用更简单可靠的方法：找到包含 LV_CONF_INCLUDE_SIMPLE 的那一行，
# 在其后插入补丁块。
$lines = Get-Content -Path $EspCmake
$insertIndex = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match 'LV_CONF_INCLUDE_SIMPLE') {
        # 找到该行所在 target_compile_definitions 块的结束（右括号）
        # 简化处理：向下找到第一个只含 ')' 的行作为块结束
        $j = $i
        while ($j -lt $lines.Count) {
            if ($lines[$j] -match '^\s*\)\s*$' -and $j -gt $i) {
                $insertIndex = $j + 1
                break
            }
            $j++
        }
        if ($insertIndex -lt 0) {
            # 没找到块结束，就在匹配行后直接插
            $insertIndex = $i + 1
        }
        break
    }
}

if ($insertIndex -lt 0) {
    Write-Err "Could not find LV_CONF_INCLUDE_SIMPLE anchor in esp.cmake."
    Write-Err "lvgl 版本可能变化，请手动应用补丁。参考 docs/developer-guide.md §3"
    exit 2
}

$patchLines = @(
    '',
    '# ThorVG 是第三方库，多种 -Werror=xxx 警告(format/type-limits 等)无需逐个修，统一关闭警告转错误',
    'file(GLOB_RECURSE THORVG_SOURCES ${LVGL_ROOT_DIR}/src/libs/thorvg/*.cpp)',
    'set_source_files_properties(${THORVG_SOURCES} COMPILE_FLAGS "-Wno-error")',
    ''
)

$newLines = @()
$newLines += $lines[0..($insertIndex - 1)]
$newLines += $patchLines
if ($insertIndex -lt $lines.Count) {
    $newLines += $lines[$insertIndex..($lines.Count - 1)]
}

Set-Content -Path $EspCmake -Value $newLines -Encoding UTF8

# 验证
$verify = Get-Content -Path $EspCmake -Raw
if ($verify -match 'THORVG_SOURCES') {
    Write-OK "Patch applied successfully."
    Write-Host ""
    Write-Host "Applied to: $EspCmake"
    Write-Host "Inserted after line ~$insertIndex (after LV_CONF_INCLUDE_SIMPLE block)."
    Write-Host ""
    Write-Host "Next: rebuild with"
    Write-Host "  .\scripts\build_flash_monitor.ps1 -Action build -Clean"
} else {
    Write-Err "Patch verification failed. Please check the file manually."
    exit 3
}

exit 0
