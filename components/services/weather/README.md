# Weather 服务故障排查与修复记录

本文记录天气服务接入和风天气 API 期间出现的问题、根因与当前修复方案。

## 运行链路

`weather` FreeRTOS 任务会在网络就绪后执行以下流程：

1. 可选：通过 IP 地理位置服务获取经纬度，并调用和风城市查询接口得到城市 ID。
2. 调用和风实时天气接口，读取 HTTP 响应。
3. 识别并解压响应数据，解析 JSON 中的 `code`、`now.temp`、`now.icon`、`now.obsTime`。
4. 更新 `weather_snapshot_t`，供首页 UI 显示城市、温度和天气图标。

## 问题一：`ESP_ERR_INVALID_RESPONSE`

### 现象

串口出现如下日志：

```text
W (...) weather: (weather_task:...): refresh failed: ESP_ERR_INVALID_RESPONSE
```

网络连接正常，HTTP 状态码为 `200`，但天气数据未更新。

### 根因

和风天气接口返回的响应带有 `Content-Encoding: gzip`。即使客户端请求头设置为 `Accept-Encoding: identity`，服务端仍可能返回 Gzip 压缩数据。

旧实现将原始 HTTP 字节直接传入 `cJSON_Parse()`。压缩数据并非 JSON 文本，因此 JSON 字段校验失败，最终返回 `ESP_ERR_INVALID_RESPONSE`。

### 修复方案

`weather_http_get()` 现在会先保存原始响应，再检查前两个字节是否为 Gzip 魔数 `0x1f 0x8b`：

- 非 Gzip 响应：直接复制为 JSON 文本；
- Gzip 响应：解析 Gzip 头部，去除头部和 8 字节尾部；
- 使用 ESP-ROM 提供的 `tinfl_decompress()` 解码原始 DEFLATE 数据；
- 解码后的文本以 `\0` 结尾，再交给 cJSON 解析；
- 数据不完整、格式非法或输出缓冲不足时，明确返回错误，不使用不完整 JSON。

当前客户端显式声明 `Accept-Encoding: gzip`，以匹配服务端的实际响应行为。

## 问题二：`weather` 任务栈溢出

### 现象

设备出现以下异常后重启：

```text
***ERROR*** A stack overflow in task weather has been detected.
Guru Meditation Error: Core 0 panic'ed (Double exception).
```

后续回溯可能显示为 `CORRUPTED`。这是栈破坏后的常见结果，不能据此继续可靠定位业务函数。

### 根因

天气任务中的 HTTPS 请求、JSON 解析和 Gzip 解码会形成嵌套调用。初始实现同时在栈上分配响应缓冲，并调用高层内存解压函数；解压状态与已有调用栈叠加后超过了任务栈容量。

### 修复方案

- 天气任务栈由较小配置提升为 `12288` 字节；
- HTTP 原始响应缓冲和解析响应缓冲使用 `malloc()` 分配，调用结束立即 `free()`；
- Gzip 解码器 `tinfl_decompressor` 同样在堆上分配，避免其大体积状态占用任务栈；
- 内存分配失败时返回 `ESP_ERR_NO_MEM`，不继续执行解码或 JSON 解析。

上述策略将大对象从任务栈迁移到堆，保留任务栈给 HTTPS 和 FreeRTOS 调用链使用。

## 诊断工具提示

日志中若同时出现：

```text
xtensa-esp32s3-elf-addr2line: [Errno 2] No such file or directory
```

表示主机端缺少或未配置 `xtensa-esp32s3-elf-addr2line`，导致监视器无法把地址转换为源码位置；它不是设备崩溃的根因。请使用项目内 SDK 启动监视器：

```bash
./build.sh monitor
```

不要混用全局 ESP-IDF 的 `idf_monitor.py` 与本项目的构建产物。

## 构建、烧录与验证

本次仅修改 C 源代码，无需重新烧录 LittleFS 资源分区：

```bash
./build.sh build
./build.sh flash
./build.sh monitor
```

验证通过应同时满足：

- 首次网络就绪后的天气刷新出现 `weather: refresh succeeded`；
- 首页显示有效的城市、温度与天气图标；
- 不出现 `refresh failed: ESP_ERR_INVALID_RESPONSE`；
- 不出现 `A stack overflow in task weather`、`Guru Meditation` 或自动重启。

若串口被其他监视器占用，先在该终端按 `Ctrl+]` 退出，再执行烧录命令。

## 安全注意事项

和风天气 API Key 仅应填写在 `menuconfig`/`sdkconfig` 的配置项中，不得写入源码、README、串口日志或提交记录。
