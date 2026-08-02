# TODO 待办备忘

记录待完成功能、已知待修复问题、未来计划。完成的项目移到 [dev-log.md](dev-log.md)。

格式：`- [ ] 优先级 | 内容 | 备注`

---

## 高优先级

- [ ] P0 | 完善 WiFi 页面：连接成功后显示 IP、RSSI、断开按钮 | wifi_manager.c 已有接口
- [ ] P0 | WiFi 密码改为从 NVS 读取，UI 输入；不再硬编码 | 当前 DEFAULT_WIFI_* 在源码中
- [ ] P0 | 验证 SD 卡热插拔检测 | sd_init 当前只在启动时初始化

## 中优先级

- [ ] P1 | USB 功能：HID 设备（键鼠）、UAC 设备、U 盘 | conf.h 已定义 USE_UAC_AUDIO=1
- [ ] P1 | 系统设置页面完善：屏幕亮度、超时、WiFi 开关、关于 | page_settings.cpp 已搭框架
- [ ] P1 | 集成 I2S 音乐播放器 + UAC 双路 | NAU88C22 待硬件验证
- [ ] P1 | HTTP 服务：文件上传、OTA 升级 | 依赖 WiFi 稳定
- [ ] P1 | LVGL 字体管理器：从 SD 卡动态加载大矢量字体 | 当前用静态字库

## 低优先级

- [ ] P2 | 移植 LVGL 8.3 的旧应用到 9.3 | 部分页面 API 变化
- [ ] P2 | NAU88C22 / QMC5883L 硬件验证 | 待硬件到位
- [ ] P2 | 增加传感器数据曲线图（chart）页面 | MPU6050/QMC5883L 数据可视化
- [ ] P2 | 多语言支持（i18n） | 当前文案硬编码
- [ ] P2 | 主题切换（深色/浅色） | LVGL theme
- [ ] P2 | 屏幕旋转支持（横竖屏动态切换） | conf.h 当前编译期固定

## 已知问题（待修复）

- [ ] BUG | 偶发 `wifi_manager` 在 disable 后立即 enable 会有短暂状态错乱 | 复现条件：1s 内连续切换
- [ ] BUG | `lv_port_indev` 编码器模式长按事件未实现 | USE_ENCODER=0 当前未启用，影响不大
- [ ] BUG | `page_sd_file` 大文件列表滚动时帧率下降 | 怀疑是 LVGL draw buffer 不够
- [ ] BUG | `thread_manager` 的 `ThreadPool` 在析构时未等待任务完成 | 仅影响测试代码

## 文档 / 工具

- [ ] DOC | 补充 `thread_manager` 的 API 文档和示例
- [ ] DOC | 补充 `page_manager` 动画切换示例
- [ ] TOOL | 写一个 `scripts/flash_only.ps1` 只烧录不重新编译（快速调试用）
- [ ] TOOL | 写一个 `scripts/analyze_log.ps1` 自动从 logs/ 找出 ERROR 行

## 复盘 / 优化方向

- [ ] OPT | LVGL draw buffer 改用 PSRAM 双缓冲，提升帧率
- [ ] OPT | WiFi 扫描结果缓存，避免每次切页面都重扫
- [ ] OPT | NVS 分区扩大（当前 0x6000 = 24KB），若存大量 WiFi 配置可能不够
- [ ] ARCH | 考虑引入事件总线（pub/sub）替代部分回调，降低模块耦合
