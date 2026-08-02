# WiFi 兼容性调试笔记

记录本项目在 WiFi 连接上遇到的兼容性问题及最终解决方案，供后人避坑。

## 环境

- ESP32-S3-SoC-N16R8，ESP-IDF v6.0.2（5.2 时同样复现）
- STA 模式连接
- 测试设备 1：FYMTC 路由器（家用宽带路由器）
- 测试设备 2：FYMWER 手机热点

---

## 1. FYMTC 路由器：无法连接

### 现象

- ESP32 调用 `esp_wifi_connect()` 后，驱动发出 auth 帧，**路由器完全不响应**。
- 日志关键行：
  ```
  wifi_manager: Retry to connect to AP (1/5)
  wifi_manager: Retry to connect to AP (2/5)
  ...
  wifi:nvs_flash_init
  wifi:state: auth -> assoc (xx)
  wifi:state: init -> auth (x)
  wifi:new:<x,x,x>, old:<x,x,x>, ap:<255,255,255>, sta:<"FYMTC",x,x>, prof:1
  Reason: 2  (AUTH_EXPIRE)
  ```
- 同一时刻手机、笔记本均能正常连接该路由器。

### 已排除项

逐一排除以下因素（均无效）：

| 排查项 | 操作 | 结果 |
|--------|------|------|
| MAC 地址过滤 | 路由器后台确认未开启 | 排除 |
| 客户端数量限制 | 关闭限制 | 排除 |
| WPA3 / WPA2 | 切换 WPA2-PSK / WPA3 transition | 排除 |
| 信道 | 锁定 1/6/11 信道 | 排除 |
| 信号强度 | 距路由器 30cm | RSSI -40dBm，排除 |
| 加密模式 | TKIP / AES / 开放 | 排除 |
| 重启 | 重启路由器 / 重启 ESP32 | 排除 |
| PMF | pmf_cfg.capable = true/false | 排除 |
| 隐藏 SSID | 关闭隐藏 | 排除 |

### 根因

**路由器固件与 ESP32 芯片组兼容性问题**。

ESP32 发出的 auth 帧（802.11 authentication frame）路由器不响应，
导致驱动等满超时（约 1s）后报 `reason=2 AUTH_EXPIRE`。

具体原因可能是路由器对 auth 算法（Open System vs Shared Key）、
802.11w (PMF) 协商、或某些 802.11 修订字段的处理与 ESP32 不一致。
但手机/笔记本的 WiFi 芯片组（高通/博通/Apple）能通过路由器的兼容路径。

### 结论

**无法在 ESP32 端绕过**。可选方案：

1. 升级路由器固件（若厂商提供）。
2. 换路由器（家用环境最终选择）。
3. 改用手机热点 / 其他 AP（本项目采用）。

---

## 2. FYMWER 手机热点：BSSID 锁定方案

### 现象

- ESP32 连手机热点同样出现 `reason=201 NO_AP_FOUND`，即便 SSID/密码正确、信号满格。
- 手机能看到热点已开启，但 ESP32 扫不到 / 扫到也连不上。

### 根因

部分手机热点（尤其 Android）**不响应广播 probe request**，
导致 ESP32 驱动内部的连接扫描（all-channel scan）找不到 AP。

### 解决方案：BSSID 锁定

在 `wifi_manager_connect_to_ap()` 中，**先做一次带 SSID 的定向扫描**，
拿到 AP 的 BSSID 和 channel 后写入 `wifi_config`，切换为 `WIFI_FAST_SCAN`，
让驱动跳过内部扫描直接连指定 AP。

代码位置：[components/system/services/wifi_manager.c](../components/system/services/wifi_manager.c)
中 `wifi_manager_connect_to_ap()` 函数。

核心片段：

```c
// 默认全信道扫描；若下面的定向扫描拿到 BSSID 则切换为 FAST_SCAN + BSSID 锁定
wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

// === 定向扫描：解决"路由器/手机热点只响应带 SSID 的 probe request"问题 ===
{
    wifi_scan_config_t scan_cfg = {
        .ssid = (uint8_t*)ssid,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_wifi_scan_start(&scan_cfg, true);  // 阻塞扫描

    wifi_ap_record_t ap_record = {};
    uint16_t got = 1;
    if (esp_wifi_scan_get_ap_records(&got, &ap_record) == ESP_OK && got > 0) {
        memcpy(wifi_config.sta.bssid, ap_record.bssid, 6);
        wifi_config.sta.bssid_set = true;
        wifi_config.sta.channel = ap_record.primary;
        wifi_config.sta.scan_method = WIFI_FAST_SCAN;
        ESP_LOGI(TAG, "BSSID locked: %02x:%02x:... ch=%u rssi=%d",
                 ap_record.bssid[0], ...);
    }
}

// 应用配置并连接
ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
```

### 副作用与权衡

- 定向扫描会增加约 120ms~300ms 连接耗时（active scan 一个 SSID）。
- 若 AP 频繁切换 BSSID（如 mesh 漫游），锁定后可能失效，需要重新扫描。
  本项目场景（固定热点）可接受。
- 对正常响应广播 probe 的路由器，定向扫描同样能拿到 BSSID，方案对正常 AP 无副作用。

---

## 3. Stale Event Bit Bug

### 现象

第二次连接 WiFi 时，`xEventGroupWaitBits` 立即返回，但 `bits & WIFI_CONNECTED_BIT`
为真 —— 实际并未连接。导致上层逻辑误判已连接。

### 根因

`xEventGroupWaitBits` 不会自动清除已设置的 bit。第一次连接成功后
`WIFI_CONNECTED_BIT` 一直置位；第二次调用 `esp_wifi_connect()` 后立即 `WaitBits`，
看到旧 bit 直接返回。

### 解决

`esp_wifi_connect()` 前显式清除两个 bit：

```c
// 清除上一次连接尝试遗留的事件位，避免 xEventGroupWaitBits 立即返回旧状态
xEventGroupClearBits(s_wifi_event_group,
                     WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
esp_err_t ret = esp_wifi_connect();
```

代码位置：`wifi_manager_connect_to_ap()` 中 `esp_wifi_connect()` 调用前一行。

---

## 4. WiFi 调试通用技巧

### 4.1 打开详细日志

`sdkconfig` 中：
```
CONFIG_LOG_DEFAULT_LEVEL_VERBOSE=y
CONFIG_ESP_WIFI_LOG_DEBUG=y
```

或运行时 `esp_log_level_set("wifi", ESP_LOG_DEBUG);`。

### 4.2 reason 码速查

| reason | 含义 | 常见原因 |
|--------|------|----------|
| 1 | UNSPECIFIED | 通用失败 |
| 2 | AUTH_EXPIRE | auth 超时，路由器未响应 / 兼容性 |
| 4 | ASSOC_EXPIRE | assoc 超时 |
| 15 | FOURWAY_HANDSHAKE_TIMEOUT | 密码错误 |
| 201 | NO_AP_FOUND | 扫描找不到 AP（隐藏 SSID / 不响应广播 probe） |
| 202 | PASSWORD_ERR | 密码错误（部分版本） |
| 203 | BEACON_TIMEOUT | 信号差 / 路由器离线 |

### 4.3 用 esp_wifi_ap_get_info 检查实际连接

```c
wifi_ap_record_t ap;
esp_wifi_sta_get_ap_info(&ap);
ESP_LOGI(TAG, "Connected BSSID=%02x:... rssi=%d", ap.bssid[0], ap.rssi);
```

### 4.4 监听 IP 事件

连接成功后必须等 `IP_EVENT_ETH_GOT_IP` / `IP_EVENT_STA_GOT_IP` 才算真正联网。
本项目 `wifi_manager.c` 的事件处理器已处理。
