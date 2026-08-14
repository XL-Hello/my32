| 支持的目标 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ---------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

# MY32 工程

本工程按应用代码、应用组件和 SDK 分层管理，当前目标为 ESP32-S3。

### 工程基础与构建

- 建立 ESP-IDF 固件工程和第一版演示程序，并将 ESP-IDF v5.1.2 SDK 固定在仓库内管理。
- 新增 `build.sh`，构建流程统一优先使用项目内 `esp-idf/`，避免误用开发机全局 SDK。
- 完成应用、服务、驱动、HAL、适配层和第三方依赖的分层重构；嵌套组件由根 `CMakeLists.txt` 显式注册。
- 配置双 OTA 应用分区及 2 MiB LittleFS 数据分区；启用 PSRAM，并将 CPU 调整为 240 MHz、PSRAM 调整为 80 MHz，以改善界面运行性能。

### 显示、触摸与图形界面基础

- 接入 ILI9341 LCD 驱动，提供显示方向配置，并完成显示、触摸初始化及刷新速度优化。
- 接入 LVGL，使用项目内自定义配置而非 `menuconfig` 配置；支持 PNG 图片解码、自定义字体、压缩字体、自定义内存和默认 16 px 字体。
- 将 LVGL UI 刷新周期由 33 ms 优化至 10 ms，并在 SPI 40 MHz 下修复启动卡死问题：显示刷新相关回调链已放入 IRAM，避免 Wi-Fi/NVS 禁用缓存期间的 ISR 死锁。
- 接入 XPT2046/HR2046 触摸：实现原始触摸采样、五点校准界面与 LVGL 输入对接；同时重新调配 LVGL 与触摸的硬件接口。
- 完成统一返回按钮和页面跳转关系优化。

### 首页、控制中心与设备界面

- 实现首页环境监测和性能信息展示，持续优化时钟、CPU 使用率、FPS、温湿度图标和状态滑条。
- 实现状态栏 Wi-Fi 状态展示及下滑打开控制中心的交互。
- 实现控制中心及其三项设置入口，并完成资源布局优化。
- 实现 Wi-Fi 设置页面，按 UI—管理器—ESP 的三层结构接入 Wi-Fi 管理；界面已接入控制中心入口并补充字体资源。
- 实现相册浏览与温湿度展示，后续优化相册和显示性能。
- 实现系统信息详情页，显示设备信息及软件更新入口；提供全局性能悬浮窗的可选配置。
- 已纳入 UI 设计稿、图标素材和页面预览图，并支持将 PNG、字体等资源一键打包到 LittleFS。

### 联网、时间与软件更新

- Wi-Fi 联网后自动触发 SNTP 对时，并提供系统时间服务。
- 实现本地 OTA 更新状态机：可通过 HTTP 查询更新、下载并显示进度、写入候选 OTA 分区、切换启动分区后重启；新固件在关键服务启动成功后确认运行，以支持回滚保护。

### 外设与底层能力

- 接入 AHT20 温湿度采集，提供规格书校验日志和环境传感器服务。
- 接入 WS2812B 可编程灯带和 RGB LED 指示灯控制。
- 实现软件 SPI，并接入 W25Q64 SPI NOR Flash；启动时执行通信自检。
- 实现软件 UART，并用于 LVGL 触摸输入对接。
- 集成 LittleFS 核心和 ESP VFS 挂载层，提供 LittleFS 功能/速度诊断；挂载失败时不再自动格式化分区，保护已有资源和数据。
- 提供 CPU 使用率、帧率和系统性能监测能力。

### 调试与日志

- 新增平台日志封装和 `greatlogger` 日志系统骨架，当前已具备日志数据流转能力；日志持久化仍待实现。
- 新增 ESP 平台 Console，便于通过串口执行调试操作。

### 提交阶段索引

| 时间 | 对应提交 | 主要完成内容 |
| --- | --- | --- |
| 2026-07-14 | `c93434b`–`d9bf511` | 工程初始化、第一版 demo、文档、仓库内 SDK 与 WS2812B 控制 |
| 2026-07-15 | `4add8fa`–`010e8d4` | ILI9341、LVGL Hello World 与 LVGL 自定义配置 |
| 2026-07-20 至 2026-07-24 | `c15e654`–`3af6572` | LCD 方向、项目内 SDK 构建、触摸/AHT20、首页、性能优化与软件 UART |
| 2026-07-31 | `1fb0a96`–`7a60918` | 软件 SPI、W25Q64、LittleFS 集成与工程分层优化 |
| 2026-08-03 至 2026-08-05 | `236714a`–`e562424` | 资源打包、控制中心、Wi-Fi、SNTP、首页、相册、PSRAM 与性能优化 |
| 2026-08-06 | `47c241b` | 系统信息、HTTP OTA，以及 SPI/IRAM 稳定性修复 |
| 2026-08-14 | `b1621b2` | 日志系统骨架与 ESP Console |

## 目录结构

```text
MY32/
├── app/                    应用组件：启动、OTA、Console 与 LVGL UI
│   ├── main.c              应用入口
│   ├── local_ota.{c,h}     本地 HTTP OTA 更新
│   └── ui/
│       ├── common/         返回按钮与性能监控 UI
│       ├── controlcenter/  控制中心与系统信息页
│       ├── developer_mode/ 触摸校准页
│       ├── front/          字体及字体生成工具
│       ├── home/           首页、相册与温湿度卡片
│       ├── icon/           UI 图标及 PNG 源资源
│       ├── set/            设置与 Wi-Fi 设置页
│       └── statusbar/      状态栏 UI
├── assets/ui/              README 使用的页面预览图
├── UI稿件/                 UI 设计稿、尺寸标注、图标与示例图片
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
│   ├── greatlogger/        日志系统及其平台移植层
│   ├── hal/                总线、VFS 与通用底层能力
│   │   ├── common/ringbuffer/
│   │   ├── littlefs_vfs/   ESP 分区挂载与 VFS 注册
│   │   └── {soft_spi,soft_uart}/
│   ├── platform/
│   │   └── platform_log/   平台日志封装
│   ├── services/           产品服务
│   │   └── {console,cpu_usage,environment_sensor,system_time,wifi_manager}/
│   └── third_party/        保持可追溯的第三方源码
│       └── {led_strip,littlefs,lvgl}/
├── docs/                   设计方案、问题分析与器件资料
├── esp-idf/                固定版本的 ESP-IDF SDK 源码副本
├── tools/                  本地 FTP、字体转换与 LittleFS 打包工具
├── build.sh                使用仓库内 ESP-IDF 的构建、烧录与监控脚本
├── CMakeLists.txt
├── partitions.csv          Flash 分区表（含 OTA 与 LittleFS 分区）
├── sdkconfig.defaults      默认 ESP-IDF 配置
├── sdkconfig.ci            CI 专用 ESP-IDF 配置
└── pytest_hello_world.py   真机与 QEMU 集成测试
```

`esp-idf/` 来自 ESP-IDF v5.1.2。SDK 的必要改动应单独记录，避免与 `app/`、`components/` 中的产品代码混杂。`build/`、`sdkconfig` 等构建生成文件未在上图展开。

依赖主要自上而下：`app → services/adapters → drivers → hal → ESP-IDF/third_party`。
其中 LittleFS 核心仅位于 `third_party/littlefs`；应用先调用
`hal/littlefs_vfs` 的 `littlefs_esp_mount()` 挂载，随后通过 `/littlefs/...` 的
标准 VFS 接口（如 `fopen`、`fread`、`fwrite`）访问文件。嵌套组件目录由根
`CMakeLists.txt` 的 `EXTRA_COMPONENT_DIRS` 显式注册。

## 使用方法

本项目使用仓库内的 `esp-idf/`，通过 `build.sh` 自动启用 SDK、工具链和 Python
环境。当前 LittleFS 烧录目标固定为 ESP32-S3；无需也不应加载开发机的全局 ESP-IDF。

### 首次准备

资源打包时需要 `lv_font_conv` 生成 LVGL 字体；只修改 C 代码时不需要安装它：

```bash
npm i lv_font_conv -g
```

默认串口为 `/dev/ttyACM0`。可在单次命令中传入串口，或在当前终端设置
`DEFAULT_SERIAL_PORT`：

```bash
export DEFAULT_SERIAL_PORT=/dev/ttyUSB0
```

### 构建、烧录与日志验证

仅修改 C 代码或配置时，依次编译、烧录应用并查看串口日志：

```bash
./build.sh build
./build.sh flash /dev/ttyACM0
./build.sh monitor /dev/ttyACM0
```

`monitor` 会持续占用前台终端；验证完成后按 `Ctrl+]` 退出。

修改 `app/ui/icon/png/`、`app/ui/front/` 或
`app/ui/home/sub_home/album/photo/` 中的资源时，需重新生成并烧录 LittleFS
镜像。`./build.sh build flash` 仅生成 `build/littlefs_icons.bin`，不会写入设备：

```bash
./build.sh build flash
./build.sh flash /dev/ttyACM0
./build.sh flash flash /dev/ttyACM0
./build.sh monitor /dev/ttyACM0
```

需要将当前固件一同放入 LittleFS 的 `ota/` 目录以供本地 OTA 测试时，使用：

```bash
./build.sh build flash ota
./build.sh flash flash /dev/ttyACM0
```

### 其他常用命令

| 命令 | 作用 |
| --- | --- |
| `source ./build.sh env` | 在当前终端启用项目内 ESP-IDF，之后可直接执行 `idf.py`。 |
| `./build.sh menuconfig` | 打开 ESP-IDF 配置界面。 |
| `./build.sh size` | 将内存、组件和源文件大小报告写入 `build/firmware-size-report.txt`。 |
| `./build.sh clean` | 清理 bootloader 的 SDK/CMake 缓存。 |
| `./build.sh flash` | 将应用烧录到默认串口。 |
| `./build.sh monitor` | 监控默认串口日志。 |

直接执行 `./build.sh env` 不会修改当前终端环境，必须使用 `source ./build.sh env`。
若需直接调用 `idf.py`，可先执行该命令并确认 `IDF_PATH` 指向仓库内的 `esp-idf/`。

新增产品功能时，将入口与业务编排放在 `app/`；按职责放入对应的
`components/services`、`components/drivers` 或 `components/hal`；除 SDK 升级或必要补丁外，不在 `esp-idf/` 中开发产品功能。

## 故障排查

### 固件烧录失败

- 检查硬件连接是否正确。运行 `idf.py -p PORT monitor`，然后重启开发板并查看串口日志。
- 如果下载波特率过高，请在 `menuconfig` 中降低波特率后重新烧录。

## 技术支持与反馈

- 技术问题可前往 [esp32.com](https://esp32.com/) 论坛讨论。
- 功能建议或缺陷报告可在 ESP-IDF 仓库中创建 [GitHub Issue](https://github.com/espressif/esp-idf/issues)。
