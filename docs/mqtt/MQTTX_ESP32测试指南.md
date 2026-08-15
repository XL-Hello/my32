# MQTTX 与 ESP32 MQTT 温湿度上报测试

本测试让 ESP32-S3 作为 MQTT 客户端，在连上 Wi-Fi 并连通 Broker 后，每 10 秒向 `esp32s3/test/telemetry` 发布一条模拟温湿度 JSON。

## 角色与网络要求

MQTTX 是用于收发和观察消息的客户端，不是 Broker。测试前需要在 PC 或局域网服务器运行一个 MQTT Broker，例如 Mosquitto 或 EMQX。ESP32、运行 Broker 的 PC 和 MQTTX 必须位于可相互访问的网络中。

若 Broker 部署在 PC：

1. 安装并启动 Mosquitto 或 EMQX，监听 TCP `1883` 端口。
2. 在 PC 终端确认本机局域网 IPv4 地址。Windows 可使用 `ipconfig`，macOS/Linux 可使用 `ip addr`。
3. 允许系统防火墙放行该 TCP 端口；不能填 `127.0.0.1`，因为它只代表 ESP32 自身。

## 配置 ESP32

1. 通过设备的 Wi-Fi 设置页连接与 Broker 同一局域网的热点。连接信息会由 `wifi_manager` 保存。
2. 在项目根目录执行下列命令，进入配置界面：

   ```bash
   source ./build.sh env
   idf.py menuconfig
   ```

3. 打开 `MQTT 测试`，配置：

   - `MQTT Broker URI`：填 `mqtt://<PC的局域网IP>:1883`，例如 `mqtt://192.168.1.23:1883`；
   - `遥测上报主题`：保持 `esp32s3/test/telemetry`；
   - `MQTT 客户端 ID`：保持默认或改为唯一值。多块开发板不能使用同一个 ID。

4. 保存后构建和烧录：

   ```bash
   ./build.sh build && ./build.sh flash
   ```

本次只修改固件代码和配置，不涉及 LittleFS 资源，无需额外烧录资源分区。

## 配置 MQTTX

在 MQTTX 创建一个连接：

- Name：任意，例如 `PC-Mosquitto`；
- Host：Broker 所在 PC 的局域网 IPv4 地址；
- Port：`1883`；
- Protocol：`mqtt://`；
- Client ID：任意且不要与 ESP32 的 ID 重复；
- Username / Password：仅在 Broker 已启用认证时填写。

连接成功后，在 MQTTX 新建订阅，Topic 填 `esp32s3/test/telemetry`，QoS 选 `0`。稍后会持续收到消息，例如：

```json
{"sequence":0,"temperature_c":25.0,"humidity_rh":55.0}
```

## 串口验证

烧录后执行：

```bash
./build.sh monitor
```

验证点：

- Wi-Fi 获取地址并可访问 Broker 后，日志出现 `已连接 Broker`；
- 首次连接后立刻出现一条 `上报成功`，之后约每 10 秒重复；
- MQTTX 的订阅窗口同步收到同一主题和 JSON 内容；
- 日志中没有 `MQTT 连接错误` 持续循环、断言或 watchdog reset。

若客户端反复断开，依次确认 PC 地址是否填对、ESP32 是否已连同一 Wi-Fi、Broker 是否运行、端口和防火墙是否放行。
