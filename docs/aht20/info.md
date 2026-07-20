# AHT20 温湿度传感器

本项目已提供 AHT20 的 I2C 采集组件。应用启动时初始化传感器，随后每 2 秒触发一次测量，并在串口输出温度和相对湿度。

## 接线

默认接线适用于 YD-ESP32-S3-COREBOARD，选用未被当前 LCD、触摸和板载 RGB LED 占用的引脚。

| AHT20 引脚 | ESP32-S3 引脚 | 说明 |
| --- | --- | --- |
| VCC | 3V3（J1-1 或 J1-2） | 仅使用 3.3 V 供电。 |
| GND | GND（J1-22 或 J2-1） | 与开发板共地。 |
| SDA | GPIO15（J1-8） | I2C 数据线。 |
| SCL | GPIO16（J1-9） | I2C 时钟线。 |

AHT20 的 7 位 I2C 地址为 `0x38`，总线频率为 100 kHz。SDA/SCL 必须通过约 2.0–4.7 kΩ 外部电阻上拉至 AHT20 的 VCC；开发板内部上拉仅作辅助，不能替代外部上拉。AHT20 的 VCC 与 GND 间还应靠近传感器放置 10 µF 去耦电容。

若接线不同，可在构建参数中覆盖以下宏，或直接修改 `components/aht20/aht20.c` 中的默认值：

```text
AHT20_I2C_PORT      # 默认 I2C_NUM_0
AHT20_I2C_SDA_GPIO  # 默认 GPIO15
AHT20_I2C_SCL_GPIO  # 默认 GPIO16
```

## 采集流程

驱动使用 ESP-IDF 的 `i2c_param_config()`、`i2c_driver_install()`、`i2c_master_write_to_device()` 和 `i2c_master_read_from_device()` 接口完成以下流程：

1. 初始化 I2C 主机后等待至少 100 ms；使用 `0x71` 命令读取 AHT20 状态字节，正常上电流程不发送软件复位命令。
2. 仅在 `(status & 0x18) != 0x18` 时发送校准命令，并再次检查状态字节。
3. 发送测量命令 `0xAC 0x33 0x00`，等待 80 ms。
4. 读取 7 字节测量帧；若忙标志仍为 1，则每 10 ms 重试，最多 5 次；随后检查 CRC-8（多项式 `0x31`，初值 `0xFF`）。
5. 将 20 位原始值换算为温度（°C）和相对湿度（%RH）。

应用通过 `aht20_init()` 初始化传感器，之后调用 `aht20_read()` 读取数据。接口声明见 `components/aht20/include/aht20.h`。

## 日志示例

传感器工作正常时，串口会出现类似输出：

```text
I (...) aht20: (aht20_init:...): AHT20 initialized: I2C0 SDA=GPIO15 SCL=GPIO16 address=0x38
I (...) main: (my_main:...): AHT20: temperature=25.37 C, humidity=48.62 %RH
```

未检测到设备、接线错误或数据校验失败时，会输出 `AHT20 initialization failed`、`AHT20 read failed` 或 `AHT20 CRC check failed`。初始化失败后，应用会每 2 秒重试。请优先确认供电、共地、SDA/SCL 接线和地址 `0x38`。

## 构建与查看日志

```bash
./build.sh build
./build.sh flash /dev/ttyACM0
idf.py -p /dev/ttyACM0 monitor
```

将 `/dev/ttyACM0` 替换为实际串口设备。
