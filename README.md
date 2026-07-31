| 支持的目标 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ---------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

# product_A 固件工程

本工程按应用代码、应用组件和 SDK 分层管理，当前目标为 ESP32-S3。

## 目录结构

```text
product_A/
├── app/                    产品入口、启动编排和 UI
│   └── ui/                 首页与开发者模式页面
├── components/
│   ├── adapters/           框架适配层
│   │   └── lvgl_port/      LVGL 显示与输入适配
│   ├── diagnostics/        可选诊断与自检
│   │   ├── littlefs_test/  LittleFS VFS 回归与速度测试
│   │   └── w25q64_test/    W25Q64 通信自检
│   ├── drivers/            具体芯片或模块驱动
│   │   ├── display/ili9341/
│   │   ├── input/xpt2046/
│   │   ├── lighting/{rgb_led,ws2812b}/
│   │   ├── sensors/aht20/
│   │   └── storage/w25q64/
│   ├── hal/                总线、VFS 与通用底层能力
│   │   ├── common/ringbuffer/
│   │   ├── littlefs_vfs/   ESP 分区挂载与 VFS 注册
│   │   └── {soft_spi,soft_uart}/
│   ├── platform/
│   │   └── platform_log/   平台日志封装
│   ├── services/           产品服务
│   │   └── {cpu_usage,environment_sensor,system_time}/
│   └── third_party/        保持可追溯的第三方源码
│       └── {led_strip,littlefs,lvgl}/
├── esp-idf/                固定版本的 ESP-IDF SDK 源码副本
├── CMakeLists.txt
├── sdkconfig
└── pytest_hello_world.py
```

`esp-idf/` 来自 ESP-IDF v5.1.2。SDK 的必要改动应单独记录，避免与 `app/`、`components/` 中的产品代码混杂。

依赖只能自上而下：`app → services/adapters → drivers → hal → ESP-IDF/third_party`。
其中 LittleFS 核心仅位于 `third_party/littlefs`；应用先调用
`hal/littlefs_vfs` 的 `littlefs_esp_mount()` 挂载，随后通过 `/littlefs/...` 的
标准 VFS 接口（如 `fopen`、`fread`、`fwrite`）访问文件。嵌套组件目录由根
`CMakeLists.txt` 的 `EXTRA_COMPONENT_DIRS` 显式注册。

## 使用方法

请根据开发板所搭载的 Espressif 芯片选择对应的入门指南：

- [ESP32 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32/get-started/index.html)
- [ESP32-S2 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s2/get-started/index.html)

首次使用时仍需在开发机安装 ESP-IDF 对应的编译工具链。本项目始终使用仓库内的
`esp-idf/`，推荐直接执行：

```bash
./build.sh build
./build.sh flash /dev/ttyACM0
```

若需要直接使用 `idf.py`，先在当前终端启用项目内 SDK：

```bash
source ./build.sh env
idf.py set-target esp32s3
idf.py build
```

`source` 的作用是让 `IDF_PATH` 在当前终端持续有效；直接执行 `./build.sh env` 只会在该脚本进程内临时切换，结束后不会影响终端。

请将 `esp32s3` 替换为实际目标芯片，并将 `PORT` 替换为开发板串口，例如 `/dev/ttyUSB0`。

新增产品功能时，将入口与业务编排放在 `app/`；按职责放入对应的
`components/services`、`components/drivers` 或 `components/hal`；除 SDK 升级或必要补丁外，不在 `esp-idf/` 中开发产品功能。

## 故障排查

### 固件烧录失败

- 检查硬件连接是否正确。运行 `idf.py -p PORT monitor`，然后重启开发板并查看串口日志。
- 如果下载波特率过高，请在 `menuconfig` 中降低波特率后重新烧录。

## 技术支持与反馈

- 技术问题可前往 [esp32.com](https://esp32.com/) 论坛讨论。
- 功能建议或缺陷报告可在 ESP-IDF 仓库中创建 [GitHub Issue](https://github.com/espressif/esp-idf/issues)。
