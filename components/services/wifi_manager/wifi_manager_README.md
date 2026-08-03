# Wi-Fi 管理服务

## 模块概述

`wifi_manager` 是 ESP-IDF 的 Wi-Fi STA（客户端）模式服务，统一处理热点扫描、连接与断开、保存的凭据自动连接、连接状态查询和互联网可达性判断。它向上层提供较小的状态化接口，并把 SSID、密码及启用状态保存到 NVS。

## 整体架构

组件由公开接口 `wifi_manager.h`、事件处理器及一组模块级状态组成：

- `wifi_manager_network_info_t`：保存当前状态、SSID、IPv4、RSSI、启用状态和互联网可达性。
- 扫描结果缓存：最多保存 `WIFI_MANAGER_MAX_SCAN_RESULTS`（20）个 `wifi_manager_ap_info_t`。
- NVS 存储：命名空间为 `wifi`，保存 `enabled`、`ssid` 和 `password`。
- FreeRTOS 互斥锁：保护网络状态和扫描结果缓存。

基本流程为：初始化 NVS、默认事件循环、STA 网络接口和 Wi-Fi 驱动；若存在且启用了已保存凭据，则尝试连接。ESP-IDF 的 Wi-Fi/IP 事件推动状态变化：连接后等待 IPv4，获取 IP 后启动独立任务访问固定 HTTPS 地址；收到 HTTP 204 时标记互联网可用并启动 SNTP 同步，否则标记为仅本地网络可用。断开时最多自动重连 5 次。

## 外部依赖

- ESP-IDF：`esp_wifi`、`esp_netif` 和 `esp_event` 提供 STA、网络接口和事件；`nvs_flash` 提供非易失化存储。
- ESP-IDF HTTP/TLS：`esp_http_client` 与 `esp_crt_bundle` 用 HTTPS 连通性检查验证互联网访问。
- FreeRTOS：互斥锁、事件任务及连通性检查任务。
- `system_time`：互联网可用后调用 `system_time_sntp_start()` 启动 SNTP 同步。
- `platform_log`：记录错误日志。
- 硬件与环境：需要 ESP 芯片的 Wi-Fi STA 能力、已配置的 NVS 分区，以及可用的目标热点；互联网检查还依赖能够访问 `https://connectivitycheck.gstatic.com/generate_204` 的网络和证书包。

## 主要功能

- 初始化 Wi-Fi STA，读取并按需自动连接已保存的网络。
- 异步扫描附近热点并缓存 SSID、RSSI、认证类型。
- 校验 SSID/密码，发起连接；成功获取 IPv4 后再持久化凭据。
- 启用或停用 Wi-Fi，主动断开，或删除已保存网络。
- 跟踪认证失败、未找到热点、获取 IP、互联网可用等状态，并提供中文状态文本。
- 通过 HTTPS 204 检查互联网可达性；成功时触发 SNTP 同步。

## 对外接口

### 宏和数据类型

| 项目 | 用途 |
| --- | --- |
| `WIFI_MANAGER_SSID_MAX_LEN` | SSID 最大长度，32 字节。 |
| `WIFI_MANAGER_PASSWORD_MAX_LEN` | 密码最大长度，63 字节。 |
| `WIFI_MANAGER_MAX_SCAN_RESULTS` | 扫描结果缓存容量，20 个热点。 |
| `wifi_manager_status_t` | 服务状态枚举，覆盖停用、未配置、扫描、连接、认证失败、IP 获取和互联网检查等阶段。 |
| `wifi_manager_ap_info_t` | 单个扫描热点：`ssid`、`rssi`、`authmode`。 |
| `wifi_manager_network_info_t` | 当前网络信息：状态、SSID、IPv4、RSSI、启用状态和互联网可达性。 |

组件未定义供调用方注册的回调接口；状态通过查询接口获取。

### 函数

| 函数 | 用途 |
| --- | --- |
| `wifi_manager_init()` | 初始化服务，并按已保存配置尝试自动连接。 |
| `wifi_manager_set_enabled(bool)` | 启用或停用 Wi-Fi；停用不删除凭据。 |
| `wifi_manager_start_scan()` | 异步开始扫描热点。 |
| `wifi_manager_get_scan_results()` | 复制最近一次扫描结果；可传 `NULL` 仅查询数量。 |
| `wifi_manager_is_scanning()` | 查询扫描是否进行中。 |
| `wifi_manager_connect(ssid, password)` | 设置 STA 配置并开始连接。 |
| `wifi_manager_disconnect()` | 主动断开且关闭自动重连，保留凭据。 |
| `wifi_manager_forget_network()` | 删除保存的 SSID/密码、关闭自动重连并断开。 |
| `wifi_manager_get_network_info()` | 获取当前网络状态和信息。 |
| `wifi_manager_status_to_text()` | 将状态枚举转换为界面可用的中文文本。 |

## 输入与输出

输入包括调用方提供的 SSID、密码、启用开关和扫描请求，以及 ESP-IDF 的扫描完成、STA 连接/断开和 IPv4 获取事件。输出包括函数的 `esp_err_t` 返回值、热点列表、`wifi_manager_network_info_t` 状态快照和中文状态文本；错误会通过 `platform_log` 记录。成功获得 IPv4 后，组件还会输出一次 HTTPS 连通性结果并可能启动 SNTP 同步。

## 使用说明

1. 系统启动后调用一次 `wifi_manager_init()`；初始化会创建 STA 网络接口、启动 Wi-Fi，并尝试恢复已保存的启用状态和凭据。
2. 如需配网，调用 `wifi_manager_start_scan()`，随后轮询 `wifi_manager_is_scanning()`；完成后先用 `wifi_manager_get_scan_results(NULL, 0, &count)` 获取数量，再用足够大的数组读取结果。
3. 调用 `wifi_manager_connect(ssid, password)` 发起连接；周期性调用 `wifi_manager_get_network_info()` 判断连接、IP 和互联网状态。拿到 IP 后的 HTTPS 检查为异步过程。
4. 临时离线调用 `wifi_manager_disconnect()`，停用无线调用 `wifi_manager_set_enabled(false)`，清除配网信息调用 `wifi_manager_forget_network()`。

代码没有提供反初始化或资源释放接口；Wi-Fi、默认网络接口、事件处理器和互斥锁将在当前运行周期内保留。

## 限制与注意事项

- 必须先成功调用 `wifi_manager_init()`；除 `wifi_manager_init()` 外，未初始化时接口会返回错误或 `false`。
- 接口使用互斥锁保护内部状态和扫描缓存，适合任务上下文中的并发查询/操作；但代码未明确承诺完整的多调用方操作序列原子性，建议由上层避免并发发起互相冲突的连接、断开、开关和扫描操作。
- 不可在中断上下文调用：接口会获取 FreeRTOS 互斥锁，可能阻塞；NVS、Wi-Fi 和 HTTP API 也要求任务上下文。
- 扫描为异步且同一时刻只允许一次；结果最多保留 20 个热点，新的扫描会清空旧缓存。
- SSID 不能为空且最多 32 字节；密码最多 63 字节，非空密码必须至少 8 字节。空密码按开放网络处理，非空密码以 WPA2-PSK 阈值配置。
- 自动重连最多 5 次；主动断开或忘记网络会关闭自动重连。凭据仅在获取 IPv4 成功后写入 NVS。
- 互联网状态由对固定 Google 地址的 HTTPS 请求（4 秒超时、仅 HTTP 204 视为可用）决定；受 DNS、网络策略、证书和该外部服务可达性影响，不能等同于所有互联网服务均可用。
