# 开发日志

记录项目主要变更、里程碑、修复历史。最新在前。

---

## 2026-08-02 仓库整理 + IDF 6.0.2 升级

### Git 清理

- 修复 `.gitignore`：补充 `sdkconfig.old`、`.idea/`、`*.log`、`logs/`、`desktop.ini` 等
- 从 git 索引移除（保留本地）：
  - `build/`（2144 文件）
  - `managed_components/`（345 文件）
  - `.vscode/`（3 文件，含机器特定路径）
  - `sdkconfig.old`
- 追加 `.gitmodules` 到版本控制（之前未提交，导致子模块无法 init）
- 删除已废弃的 `pytest_examples_cxx_pthread.py`
- 提交：`chore: stop tracking build artifacts and IDE files`

提交后 `git status` 干净，clone 后按 README 步骤可直接编译。

### ESP-IDF v5.2 → v6.0.2 升级

- 升级 `sdkconfig`（~1700 行变更，主要是 PSRAM/FreeRTOS 配置项重命名）
- 升级 `dependencies.lock`、`main/idf_component.yml`
- 适配 IDF 6.0 新 I2C master API：`i2c_init.cpp`
- 适配新 FreeRTOS API：`task_manager.cpp`、`init_tasks.cpp`
- 适配新 LVGL 9.3 API：`page_*.cpp`、`my_ui.cpp`
- 适配新 SDMMC API：`sd_init.cpp/hpp`
- 适配新驱动 API：`axp2101.cpp`、`cst128.cpp`、`pca9554.cpp`、`lcd_brightness.cpp`
- lvgl 子模块打补丁：关闭 ThorVG `-Werror`（详见 [developer-guide.md §3](developer-guide.md)）
- 提交：`feat: upgrade ESP-IDF from v5.2 to v6.0.2`

### WiFi 调试

详细笔记见 [wifi-debug-notes.md](wifi-debug-notes.md)。

- 路由器 FYMTC：ESP32 auth 帧超时（reason=2 AUTH_EXPIRE），手机能连，根因为路由器固件
  与 ESP32 芯片组兼容性问题，无解。
- 手机热点 FYMWER：用 BSSID 锁定方案（定向扫描 → 取 BSSID/channel → 写入 wifi_config →
  FAST_SCAN）成功连上。
- 修复 stale event bit bug：`esp_wifi_connect()` 前先 `xEventGroupClearBits`。
- 提交历史：`d3547f9 solve wifi manager problem`

### 文档与脚本

新增：
- `README.md` 重写
- `docs/skills.md` — IDF 开发流程
- `docs/developer-guide.md` — 架构 / 二次开发 / 踩坑记录
- `docs/todo.md` — 待办备忘
- `docs/dev-log.md` — 本文件
- `docs/wifi-debug-notes.md` — WiFi 调试笔记
- `scripts/build_flash_monitor.ps1` — 一键编译/烧录/监视 + 日志存档
- `scripts/apply-lvgl-patch.ps1` — 子模块补丁重应用脚本

---

## 2025-07-30 设置页面 + NVS 修复

- 完成设置页面搭建（`page_settings.cpp`）
- 修复 NVS 某些状态无法保留的错误
- 提交：`9e4f092 add setting page`

## 2025-07-29 UI 动态刷新率 + 代码结构

- 完成 UI 动态刷新率设计（`system_low_power_design.cpp`）
- 完善代码结构：分阶段初始化器、模块分层
- 引入 `SystemInitializer` 单例

## 2025-07-28 PC 模拟器 + 编码器 + 低功耗

- 完成搭建 PC 模拟器，适配接口
- 添加编码器输入设备（`encoder_driver.cpp`）
- 初步完成软件低功耗设计（亮度自动调节、息屏）

## 2025-07-27 文件服务 + NVS + I2C 从机

- 添加文件服务、NVS 数据库服务、I2C 从机管理
- 完成文件浏览器页面（`page_sd_file.cpp`）、QMC 指南针页面（`page_qmc5883l.cpp`）

## 2025-07-26 RTC 驱动 + 系统时间

- 添加 RTC 芯片 DS1302 驱动
- 完善系统时间管理（`time_service.cpp`）

## 2025-07-25 WiFi / MPU6050 / AXP2101

- 添加 WiFi、MPU6050、AXP2101 页面
- 完成时间、电源、姿态传感器服务

## 2025-07-24 时间页面

- 添加时间页面（`page_time.cpp`）
- 项目启动

---

## 历史踩坑速查

| 日期 | 问题 | 解决 | 详情 |
|------|------|------|------|
| 2025-07-25 | Timer Service 栈溢出 | 调大 FreeRTOS Timer 栈到 4096 | developer-guide §5.2 |
| 2025-07-27 | GPIO ISR 中调用 ESP_LOGI 触发 assert | 改用 FromISR API | developer-guide §5.1 |
| 2025-07-29 | LVGL 对象创建前未 init disp | 严格按 lv_init → disp → indev 顺序 | developer-guide §5.3 |
| 2026-07-xx | WiFi 连不上 FYMTC 路由器 | 兼容性无解，改用热点 | wifi-debug-notes |
| 2026-07-xx | WiFi 连不上手机热点 | BSSID 锁定方案 | wifi-debug-notes |
| 2026-08-02 | IDF 6.0 ThorVG -Werror 编译失败 | lvgl esp.cmake 加 -Wno-error | developer-guide §3 |
| 2026-08-02 | managed_components 误提交导致 2000+ 文件变更 | git rm --cached + .gitignore | dev-log §本次 |
