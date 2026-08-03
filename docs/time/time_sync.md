# 系统时间与 SNTP 对时

## 概述

本项目由 `system_time` 服务维护系统时间。设备启动时先以固件的编译时间提供一个可用的初始时钟；Wi-Fi 确认可访问互联网后，再通过 SNTP（Simple Network Time Protocol）从网络时间服务器校准系统时钟。

这样可以避免在尚未获得 IP、DNS 或互联网不可用时反复发送 SNTP 请求，也让日志、文件时间等功能在首次联网前仍具有一个合理的时间基准。

当前实现位于：

- `components/services/system_time/system_time.c`：时区、初始时间和 SNTP 配置。
- `components/services/system_time/system_time.h`：对外接口。
- `components/services/wifi_manager/wifi_manager.c`：网络就绪后触发对时。
- `app/main.c`：应用启动时初始化系统时间。

## 完整对时流程

```text
app_main()
  │
  ├── system_time_init()
  │     ├── 设置时区为 CST-8（中国标准时间，UTC+8）
  │     └── 将 __DATE__ / __TIME__ 写入系统时钟，作为离线初始时间
  │
  └── wifi_manager_init()
        │
        ├── Wi-Fi 连接并收到 IP_EVENT_STA_GOT_IP
        ├── 创建联网探测任务，对 HTTPS 地址发起请求
        └── 请求成功且 HTTP 状态码为 204
              │
              └── system_time_sntp_start()
                    ├── 配置 SNTP 服务器和立即同步模式
                    ├── 注册同步完成回调
                    └── 初始化或重启 SNTP；收到服务器响应后更新系统时钟
```

`system_time_sntp_start()` 只负责发起请求，不等待服务器响应，因此不会阻塞 Wi-Fi 事件任务或调用它的任务。实际同步完成时，`system_time_sntp_sync_callback()` 会输出本地时间日志。

## 启动时的初始时间

在 `app_main()` 的最开始调用：

```c
ESP_ERROR_CHECK(system_time_init());
```

`system_time_init()` 的行为如下：

1. 通过 `setenv("TZ", "CST-8", 1)` 和 `tzset()` 设置本地时区。
2. 解析编译器提供的 `__DATE__` 与 `__TIME__` 宏。
3. 将解析结果转换为 Unix 时间戳，并用 `settimeofday()` 写入系统时钟。

这不是网络授时，只用于设备离线、尚未联网或 SNTP 同步失败时的兜底时间。它会随每次重新编译固件而变化，设备长期离线时会逐渐产生偏差。网络同步成功后，该初始值会被 SNTP 时间覆盖。

## 对时触发条件

对时由 `wifi_manager` 自动触发，应用层无需在 Wi-Fi 事件回调中直接调用 SNTP。

触发链路如下：

1. STA 获取 DHCP 地址并收到 `IP_EVENT_STA_GOT_IP`。
2. `wifi_manager` 将状态设为 `WIFI_MANAGER_STATUS_CHECKING_INTERNET`，并创建联网探测任务。
3. 探测任务访问 `https://connectivitycheck.gstatic.com/generate_204`，超时为 4000 ms，且使用 ESP-IDF 证书包校验证书。
4. 请求成功且返回 HTTP `204` 时，状态变为 `WIFI_MANAGER_STATUS_INTERNET_READY`，随后调用 `system_time_sntp_start()`。

因此，**仅拿到 IP 地址不会触发 SNTP 对时**；DNS、路由或互联网不可用时，联网探测失败，当前实现不会启动 SNTP。若设备随后断线重连并再次完成联网探测，则会再次触发该接口。

手动需要重新发起同步时，也可以在确认网络可用后调用：

```c
esp_err_t err = system_time_sntp_start();
if (err != ESP_OK) {
    // 记录或处理启动/重启 SNTP 失败
}
```

首次调用会初始化 SNTP；若 SNTP 已在运行，后续调用执行 `esp_sntp_restart()` 以重新发起同步。请不要在中断服务程序中调用该接口。

## 当前 SNTP 配置

配置定义在 `components/services/system_time/system_time.c`：

| 项目 | 当前值 | 说明 |
| --- | --- | --- |
| 本地时区 | `CST-8` | POSIX 时区格式；其中负号表示 UTC 东侧，因此为 UTC+8。 |
| 主 NTP 服务器 | `ntp.aliyun.com` | SNTP 服务器索引 0。 |
| 备用 NTP 服务器 | `time.cloudflare.com` | SNTP 服务器索引 1。 |
| 工作模式 | `ESP_SNTP_OPMODE_POLL` | SNTP 周期轮询模式。 |
| 同步模式 | `SNTP_SYNC_MODE_IMMED` | 收到有效时间后立即更新系统时钟。 |
| 完成通知 | `system_time_sntp_sync_callback()` | 同步成功后以本地时区格式记录时间。 |

如需替换时间服务器，修改以下宏后重新构建和烧录：

```c
#define SYSTEM_TIME_SNTP_PRIMARY "ntp.aliyun.com"
#define SYSTEM_TIME_SNTP_SECONDARY "time.cloudflare.com"
```

如需改为其他时区，同样修改 `SYSTEM_TIME_TIMEZONE`，并使用 POSIX `TZ` 格式。例如中国大陆保持 `CST-8`；不要把 `UTC+8` 直接写入 POSIX `TZ`，其符号含义与通常的 UTC 偏移写法相反。

目前这些值是编译期宏，尚未提供 Kconfig、NVS 或 UI 配置入口。若产品需要让用户选择时区或企业内网 NTP 服务器，应增加配置存储和输入校验，并在保存后重新调用 `system_time_sntp_start()`。

## 同步成功与验证

串口中依次出现类似日志，表示初始时间和网络对时均已完成：

```text
I (… ) system_time: system time initialized from build time: Aug 03 2026 12:00:00, timezone: CST-8
I (… ) system_time: SNTP synchronization started: ntp.aliyun.com, time.cloudflare.com
I (… ) system_time: SNTP synchronized: 2026-08-03 12:00:05
```

第二条日志只表示 SNTP 已启动；以 `SNTP synchronized:` 开头的日志才表示设备已经收到有效网络时间并更新了系统时钟。应用如需使用时间，可在同步后通过标准 C 库的 `time()`、`localtime_r()` 或 `strftime()` 读取和格式化。

```c
time_t now = time(NULL);
struct tm local_time;
if (localtime_r(&now, &local_time) != NULL) {
    // 使用 local_time 生成本地时间显示或日志
}
```

## 注意事项

- SNTP 服务器使用域名，设备必须已获得可用的 DNS 和互联网访问能力；这也是项目将对时放在 HTTPS 联网探测成功之后的原因。
- 当前接口不会等待同步完成，也未向上层暴露“已同步”状态。若某项业务必须在准确时间可用后才执行，应补充同步状态或应用自己的完成通知机制，而不要只依据 `system_time_sntp_start()` 返回 `ESP_OK`。
- 默认的周期同步由 ESP-IDF SNTP 在后台处理；项目未额外创建定时器。调用 `system_time_sntp_start()` 的重启行为适合在网络恢复后主动补发一次同步请求。
- 运行 `./build.sh build` 前不要加载全局 ESP-IDF 环境，项目必须使用仓库内的 `esp-idf/`。
