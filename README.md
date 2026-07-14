| 支持的目标 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ---------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

# Hello World 示例

本示例启动一个 FreeRTOS 任务并输出 `Hello World`。

有关示例的更多信息，请参阅上级 `examples` 目录中的 `README.md`。

## 使用方法

请根据开发板所搭载的 Espressif 芯片选择对应的入门指南：

- [ESP32 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/get-started/index.html)
- [ESP32-S2 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s2/get-started/index.html)

在已启用 ESP-IDF 环境的终端中，可以使用以下常用命令：

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

请将 `esp32s3` 替换为实际目标芯片，并将 `PORT` 替换为开发板串口，例如 `/dev/ttyUSB0`。

## 示例目录内容

`hello_world` 项目包含一个 C 语言源文件 [hello_world_main.c](main/hello_world_main.c)，该文件位于 [main](main) 目录中。

ESP-IDF 项目使用 CMake 构建。项目中的 `CMakeLists.txt` 文件包含源文件、组件和目标相关的构建配置。

```text
├── CMakeLists.txt
├── pytest_hello_world.py      用于自动化测试的 Python 脚本
├── main
│   ├── CMakeLists.txt
│   └── hello_world_main.c
└── README.md                  当前文档
```

有关 ESP-IDF 项目结构和构建机制的更多信息，请参阅 ESP-IDF 编程指南中的[构建系统](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/build-system.html)。

## 故障排查

### 固件烧录失败

- 检查硬件连接是否正确。运行 `idf.py -p PORT monitor`，然后重启开发板并查看串口日志。
- 如果下载波特率过高，请在 `menuconfig` 中降低波特率后重新烧录。

## 技术支持与反馈

- 技术问题可前往 [esp32.com](https://esp32.com/) 论坛讨论。
- 功能建议或缺陷报告可在 ESP-IDF 仓库中创建 [GitHub Issue](https://github.com/espressif/esp-idf/issues)。
