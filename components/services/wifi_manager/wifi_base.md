# Wi-Fi 基础知识：结合 `wifi_manager` 学习

本文整理阅读和使用 `wifi_manager` 前建议掌握的 Wi-Fi 与 ESP-IDF 基础概念。

## 1. STA 模式与热点

本组件使用 **STA（Station，客户端）模式**：ESP 芯片作为终端连接路由器或手机热点。它不会创建自己的热点；创建热点属于 SoftAP 模式，不在此组件的职责范围内。

- **SSID**：热点名称。
- **密码**：热点认证凭据。
- **AP**：Access Point，提供无线接入的路由器或热点。

组件限制 SSID 最多 32 字节、密码最多 63 字节。密码为空时按开放网络处理；非空密码至少需要 8 字节。

## 2. 连接不等于能上网

一次典型连接依次经历：

```text
扫描热点 → 连接 AP → 获取 IPv4 地址 → 验证互联网可达性
```

- 连接 AP 成功，仅表示无线链路已建立。
- 获取 IPv4 地址通常由 DHCP 完成，表示本地网络基本可用。
- 本组件随后访问固定 HTTPS 地址，并仅在得到 HTTP 204 响应时标记互联网可用。

因此，`WIFI_MANAGER_STATUS_INTERNET_READY` 比“已连接”或“已获取 IP”更能代表可访问互联网。

## 3. ESP-IDF 的事件驱动模型

Wi-Fi 的实际进度由 ESP-IDF 异步事件通知，调用连接或扫描接口后不会立即得到最终结果。阅读组件时应重点理解：

| 事件 | 含义 | 组件行为 |
| --- | --- | --- |
| `WIFI_EVENT_SCAN_DONE` | 热点扫描结束 | 读取并缓存扫描结果。 |
| `WIFI_EVENT_STA_CONNECTED` | 已连接到热点 | 状态变为“已连接，正在获取 IP”。 |
| `IP_EVENT_STA_GOT_IP` | 已取得 IPv4 地址 | 保存新凭据、开始互联网检查。 |
| `WIFI_EVENT_STA_DISCONNECTED` | 与热点断开 | 清除网络信息，并按条件自动重连或记录失败状态。 |

调用方应使用 `wifi_manager_get_network_info()`、`wifi_manager_is_scanning()` 轮询状态，或由上层自行建立状态通知机制。

## 4. 扫描结果、RSSI 与认证类型

`wifi_manager_start_scan()` 异步扫描附近热点，结果通过 `wifi_manager_get_scan_results()` 获取。每项 `wifi_manager_ap_info_t` 包含：

- `ssid`：热点名称。
- `rssi`：接收信号强度，单位通常为 dBm；数值越接近 0，信号通常越强。
- `authmode`：ESP-IDF 的认证类型值，用于区分开放网络、WPA/WPA2 等安全模式。

组件最多缓存 20 个热点；开始新的扫描会清空上次缓存。

## 5. 自动重连与失败原因

组件在非主动断开的情况下，最多自动重连 5 次。常见状态包括：

- `WIFI_MANAGER_STATUS_AUTH_FAILED`：密码或认证协商失败。
- `WIFI_MANAGER_STATUS_NO_AP_FOUND`：找不到目标热点。
- `WIFI_MANAGER_STATUS_DISCONNECTED`：其他原因导致的断开或重试已用尽。

主动调用 `wifi_manager_disconnect()` 或 `wifi_manager_forget_network()` 会关闭自动重连。

## 6. NVS 凭据持久化

NVS 是 ESP-IDF 的非易失化键值存储。本组件在 `wifi` 命名空间中保存：

- `enabled`：Wi-Fi 是否启用。
- `ssid`：已保存热点名称。
- `password`：已保存热点密码。

组件初始化后会读取这些值；若 Wi-Fi 已启用且存在凭据，将自动尝试连接。新调用 `wifi_manager_connect()` 的凭据在成功获取 IPv4 后才会写入 NVS。

## 7. FreeRTOS、并发与异步任务

组件用 FreeRTOS 互斥锁保护网络状态和扫描结果，并创建独立任务执行 HTTPS 连通性检查。使用时应注意：

- 不要在中断上下文调用公开接口：接口可能等待互斥锁，且会调用 Wi-Fi、NVS 或网络 API。
- 扫描、连接和互联网检查均为异步过程。
- 状态查询可并发使用，但上层应避免同时发起互相冲突的连接、断开、开关和扫描操作。

## 推荐阅读顺序

按以下顺序阅读代码，能较快建立完整认识：

1. `wifi_manager.h`：先了解状态、数据结构和公开接口。
2. `wifi_manager_init()`：了解 NVS、事件循环、STA 网络接口和驱动初始化。
3. `wifi_manager_start_scan()` 与扫描完成事件：了解扫描流程。
4. `wifi_manager_connect()` 与 Wi-Fi/IP 事件：了解连接、获取 IP 和重连流程。
5. `wifi_manager_run_connectivity_check()`：了解“已连接”和“互联网可用”的区别。
