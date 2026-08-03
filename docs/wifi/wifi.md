# ESP32-S3 Wi-Fi 配置与联网方案

## 目标

设备以 Wi-Fi Station（STA，客户端）模式接入路由器。用户可在设置界面扫描和选择热点、输入密码、保存配置、查看联网状态，并在后续开机时自动连接。

“已连接 Wi-Fi”不等同于“可访问互联网”。本方案将分别识别无线关联、获取 IP、DNS 可用和外网可达四个阶段，并将结果反馈到 UI。

## 架构

```text
设置 UI
   │  扫描、连接、断开、忘记网络、查询状态
   ▼
wifi_manager（应用 Wi-Fi 服务层）
   ├── Wi-Fi 驱动：esp_wifi
   ├── 网络接口与 DHCP：esp_netif
   ├── 事件处理：esp_event
   ├── 配置存储：NVS
   └── 网络探测：DNS + HTTP(S)
```

设置 UI 不直接调用 ESP-IDF 的 Wi-Fi API，只通过 `wifi_manager` 获取数据或发起操作。这样 Wi-Fi 事件、重连逻辑和凭据管理可集中处理，界面也更易维护。

## 模块划分

当前实现已按服务与 UI 分层放置：

- `components/services/wifi_manager/wifi_manager.h`：公开的数据类型、状态和接口。
- `components/services/wifi_manager/wifi_manager.c`：Wi-Fi 初始化、扫描、连接、事件处理、重连和联网探测。
- `app/ui/set/wifi/wifi_settings_ui.c`：设置页与 `wifi_manager` 的适配。

首页左滑可进入 Wi-Fi 设置页；设置页提供开关、扫描、选择热点、密码输入、连接、断开和忘记网络操作。

## ESP-IDF 初始化

应用启动时按以下顺序初始化一次：

1. 初始化 NVS；若遇到 `ESP_ERR_NVS_NO_FREE_PAGES` 或版本不匹配，擦除并重新初始化。
2. 初始化 `esp_netif` 和默认事件循环。
3. 创建默认 STA 网络接口：`esp_netif_create_default_wifi_sta()`。
4. 使用 `WIFI_INIT_CONFIG_DEFAULT()` 初始化 `esp_wifi`。
5. 注册 `WIFI_EVENT` 与 `IP_EVENT` 事件处理函数。
6. 设为 `WIFI_MODE_STA` 并启动 Wi-Fi。

默认使用 DHCP 获取 IPv4 地址、网关和 DNS 服务器。ESP32-S3 支持 2.4 GHz Wi-Fi；设置页和使用说明中应提醒用户选择可用的 2.4 GHz 热点。

## 配置数据与 NVS

在 NVS 命名空间（例如 `wifi`）保存以下内容：

| 键 | 内容 |
| --- | --- |
| `enabled` | Wi-Fi 是否启用 |
| `ssid` | 已选择的 SSID |
| `password` | Wi-Fi 密码 |
| `authmode` | 可选：热点加密方式 |

连接成功后再保存新凭据，避免输入错误密码覆盖当前可用配置。密码不得打印到串口、日志、断言或 UI 调试信息中。若产品具备 Flash Encryption，启用 NVS Encryption 以进一步保护存储的凭据。

## wifi_manager 接口建议

```c
typedef enum {
    WIFI_STATUS_DISABLED,
    WIFI_STATUS_UNCONFIGURED,
    WIFI_STATUS_SCANNING,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_AUTH_FAILED,
    WIFI_STATUS_NO_AP_FOUND,
    WIFI_STATUS_CONNECTED_NO_IP,
    WIFI_STATUS_LOCAL_NETWORK_READY,
    WIFI_STATUS_INTERNET_READY,
    WIFI_STATUS_INTERNET_UNREACHABLE,
} wifi_status_t;

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_scan(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
esp_err_t wifi_manager_disconnect(void);
esp_err_t wifi_manager_forget_network(void);
wifi_status_t wifi_manager_get_status(void);
```

扫描结果应至少包含 SSID、RSSI、认证方式和当前是否已连接。通过回调、消息队列或现有 UI 的事件机制通知状态变化；不要让 UI 轮询底层事件。

## 连接与重连流程

1. 用户选择热点并输入密码，UI 调用 `wifi_manager_connect()`。
2. 服务层校验 SSID 与密码长度，调用 `esp_wifi_set_config()` 和 `esp_wifi_connect()`。
3. 收到 `WIFI_EVENT_STA_CONNECTED` 后显示“已连接，正在获取 IP”。
4. 收到 `IP_EVENT_STA_GOT_IP` 后记录 IP、网关和 DNS，状态变为“本地网络可用”，随后执行联网探测。
5. 收到断连事件后，根据断开原因显示认证失败、找不到热点或连接中断；对暂时性错误使用有限次数的指数退避重试。
6. 认证成功并且完成配置保存后，开机自动读取 NVS 并重新连接。

用户主动关闭 Wi-Fi 或选择“忘记网络”时，必须取消自动重连。重试次数耗尽后保留错误状态，允许用户从设置页手动重试或重新配置。

## 设置 UI 设计

设置页至少包含以下内容：

- Wi-Fi 总开关。
- 当前 SSID、信号强度、IPv4 地址和连接状态。
- “扫描网络”操作及按信号强度排序的热点列表。
- 热点密码输入页，密码以掩码显示。
- “连接/重新连接”“断开连接”“忘记网络”操作。
- 失败原因与下一步提示，例如“密码错误”“未找到热点”“已连接但无互联网”。

扫描或连接期间禁用重复提交按钮，并显示进行中的状态。SSID 可能包含 UTF-8 字符，UI 显示和缓冲区长度应按字节上限处理。

## 联网可用性判定

按以下层级检测并展示状态：

| 阶段 | 判定条件 | UI 状态 |
| --- | --- | --- |
| 无线关联 | 收到 `WIFI_EVENT_STA_CONNECTED` | 已连接，正在获取 IP |
| 本地网络 | 收到 `IP_EVENT_STA_GOT_IP` | 本地网络可用 |
| DNS | 可解析预设探测域名 | 正在验证互联网 |
| 外网访问 | 对可配置 HTTPS/HTTP 探测地址请求成功 | 互联网可用 |

探测地址应可通过编译配置或应用配置替换；优先使用 HTTPS，并设置短超时。DNS 或请求失败时显示“已连接但无互联网”，不要因此立即清除已保存的 Wi-Fi 凭据。若目标网络存在认证门户（Captive Portal），设备可能获取 IP 但不能通过探测；此时应给出对应提示。

## 安全与日志

- 优先支持 WPA2-PSK/WPA3-Personal；拒绝或明确标识开放网络。
- 不记录密码、完整认证报文或敏感探测请求内容。
- 对外网请求启用证书校验，不为了“连通”而跳过 TLS 校验。
- 对配置输入做长度和空值校验，避免复制到固定长度缓冲区时越界。

## 实施顺序

1. 新建 `wifi_manager`，完成 ESP-IDF 初始化、STA 连接、断开与状态机。
2. 接入 NVS，实现配置保存、读取和开机自动连接。
3. 实现热点扫描与 UI 状态通知。
4. 在设置页加入网络列表、密码输入、连接和忘记网络功能。
5. 加入 DNS 与 HTTPS/HTTP 联网探测，并显示分层网络状态。
6. 完成异常场景验证与真机测试。

## 验收与测试

在 ESP32-S3 真机上验证以下场景：

- 首次配置后成功连接 2.4 GHz WPA2/WPA3 热点并取得 IP。
- 重启后自动连接已保存热点。
- 错误密码、热点不存在、路由器断电和恢复后的状态与重连行为正确。
- DHCP 成功但 DNS 或外网不可用时，UI 显示“已连接但无互联网”。
- 联网可用时能够完成目标业务所需的 DNS 和 HTTPS/HTTP 请求。
- 串口日志不包含 Wi-Fi 密码。

构建使用仓库内 ESP-IDF：

```bash
./build.sh build
```
