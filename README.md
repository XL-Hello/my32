| 支持的目标 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ---------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

# product_A 固件工程

本工程按应用代码、应用组件和 SDK 分层管理，当前目标为 ESP32-S3。

## 目录结构

```text
product_A/
├── app/                    应用入口和产品业务编排
├── components/             可复用的应用层组件
│   ├── platform/           平台适配与日志等基础能力
│   └── rgb_led/            RGB LED 应用组件
├── esp-idf/                固定版本的 ESP-IDF SDK 源码副本
├── CMakeLists.txt
├── sdkconfig
└── pytest_hello_world.py
```

`esp-idf/` 来自 ESP-IDF v5.1.2。SDK 的必要改动应单独记录，避免与 `app/`、`components/` 中的产品代码混杂。

## 使用方法

请根据开发板所搭载的 Espressif 芯片选择对应的入门指南：

- [ESP32 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/get-started/index.html)
- [ESP32-S2 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s2/get-started/index.html)

首次使用时仍需在开发机安装 ESP-IDF 对应的编译工具链。默认构建使用全局 SDK `/home/xl/.espressif/v5.1.2/esp-idf`：

```bash
./build.sh build
./build.sh flash /dev/ttyACM0
```

若需切换到项目内的 SDK 副本，必须在当前终端执行：

```bash
source ./build.sh env
./build.sh build
./build.sh flash /dev/ttyACM0
```

切换后脚本会显示当前的 ESP-IDF 路径。`source` 的作用是让 `IDF_PATH` 在当前终端持续有效；直接执行 `./build.sh env` 只会在该脚本进程内临时切换，结束后不会影响终端。

请将 `esp32s3` 替换为实际目标芯片，并将 `PORT` 替换为开发板串口，例如 `/dev/ttyUSB0`。

新增产品功能时，将入口与业务编排放在 `app/`；可跨业务复用的能力放在 `components/<组件名>/`；除 SDK 升级或必要补丁外，不在 `esp-idf/` 中开发产品功能。

## 故障排查

### 固件烧录失败

- 检查硬件连接是否正确。运行 `idf.py -p PORT monitor`，然后重启开发板并查看串口日志。
- 如果下载波特率过高，请在 `menuconfig` 中降低波特率后重新烧录。

## 技术支持与反馈

- 技术问题可前往 [esp32.com](https://esp32.com/) 论坛讨论。
- 功能建议或缺陷报告可在 ESP-IDF 仓库中创建 [GitHub Issue](https://github.com/espressif/esp-idf/issues)。
