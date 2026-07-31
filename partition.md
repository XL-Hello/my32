# ESP32-S3 主 Flash 分区说明

本文仅说明 ESP32-S3 启动和运行所使用的主 Flash，不包含其他独立存储芯片。

## 当前结论

硬件主 Flash 容量按 8 MiB（`0x800000`）计算；但项目当前配置和烧录镜像均按 **2 MiB** 工作：

- `sdkconfig:439`：`CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y`
- `sdkconfig:446`：`CONFIG_ESPTOOLPY_FLASHSIZE="2MB"`
- `build/flash_args`：烧录参数为 `--flash_size 2MB`

因此，ESP-IDF 当前仅对 `0x000000` 至 `0x1FFFFF` 进行 Flash 容量配置；物理 Flash 的 `0x200000` 至 `0x7FFFFF`（6 MiB）未被当前配置或分区表使用。

## 分区表

项目使用 ESP-IDF 内置的单应用分区表：

- `sdkconfig:460`：启用 `CONFIG_PARTITION_TABLE_SINGLE_APP`
- `sdkconfig:465`：分区表文件为 `partitions_singleapp.csv`
- `sdkconfig:466`：分区表偏移为 `0x8000`

由当前构建产物 `build/partition_table/partition-table.bin` 解析得到：

| 地址范围 | 大小 | 名称 / 类型 | 用途 | 项目中的直接代码位置 |
| --- | ---: | --- | --- | --- |
| `0x000000–0x007FFF` | 32 KiB | Bootloader 区 | ESP-IDF 二级 Bootloader；烧录文件为 `build/bootloader/bootloader.bin`。 | 入口为 `esp-idf/components/bootloader/subproject/main/bootloader_start.c:24`；烧录地址见 `build/flash_args` 的 `0x0`。 |
| `0x008000–0x008FFF` | 4 KiB | Partition Table | 保存分区表及 MD5 校验。 | 无应用层直接代码；配置在 `sdkconfig:460–467`，烧录地址见 `build/flash_args` 的 `0x8000`。 |
| `0x009000–0x00EFFF` | 24 KiB | `nvs` / `data,nvs` | 非易失键值数据，可供 Wi-Fi、应用配置等使用。 | 项目源码没有直接调用 `nvs_*` 或 `nvs_flash_*` API。 |
| `0x00F000–0x00FFFF` | 4 KiB | `phy_init` / `data,phy` | Wi-Fi/PHY 初始化校准数据。 | 无应用层直接读写代码，由 ESP-IDF 网络栈在需要时使用。 |
| `0x010000–0x10FFFF` | 1 MiB | `factory` / `app,factory` | 唯一的固件应用槽。 | 无代码按分区名访问；固件由 `build/flash_args` 烧录到 `0x10000`。 |
| `0x110000–0x1FFFFF` | 960 KiB | 未分区 | 位于当前 2 MiB 配置内，但没有任何分区或文件系统使用它。 | 无。 |
| `0x200000–0x7FFFFF` | 6 MiB | 未纳入当前配置 | 物理 8 MiB Flash 中剩余空间；当前 Flash-size 设置为 2 MiB，不能作为已配置分区使用。 | 无。 |

```text
物理 8 MiB Flash
0x000000  [ Bootloader (32 KiB) ]
0x008000  [ Partition Table (4 KiB) ]
0x009000  [ NVS (24 KiB) ]
0x00F000  [ PHY init (4 KiB) ]
0x010000  [ factory app (1 MiB) ]
0x110000  [ 未分区：960 KiB ]
0x200000  [ 未纳入当前 2 MiB 配置：6 MiB ]
0x800000  Flash 结束
```

## Bootloader 层级与默认启动流程

ESP32-S3 的启动程序分为两级：

1. **ROM 一级 Bootloader**：固化在 ESP32-S3 芯片内部，不能由本项目修改。芯片复位后先执行它；它从主 Flash 加载二级 Bootloader。
2. **ESP-IDF 二级 Bootloader**：由仓库内 `esp-idf/` 按 `sdkconfig` 自动构建，产物为 `build/bootloader/bootloader.bin`，当前烧录到 `0x0`。它不是 `app/` 下的应用代码，也不是不可修改的 ROM 程序。

当前 ESP-IDF 二级 Bootloader 的关键源码位置如下：

| 职责 | 代码位置 |
| --- | --- |
| 入口 `call_start_cpu0()` | `esp-idf/components/bootloader/subproject/main/bootloader_start.c:24` |
| ESP32-S3 硬件、Cache/MMU、主 Flash、看门狗等初始化 | `esp-idf/components/bootloader_support/src/esp32s3/bootloader_esp32s3.c:139` |
| 读取并校验分区表 | `esp-idf/components/bootloader_support/src/bootloader_utility.c:156` |
| 选择启动分区 | `esp-idf/components/bootloader_support/src/bootloader_utility.c:355` |
| 校验、加载应用镜像并跳转 | `esp-idf/components/bootloader_support/src/bootloader_utility.c:515` |
| 链接器入口声明 | `esp-idf/components/bootloader/subproject/main/ld/esp32s3/bootloader.ld:55` |
| 子工程源文件注册 | `esp-idf/components/bootloader/subproject/main/CMakeLists.txt:1` |

默认启动顺序为：

```text
ROM 一级 Bootloader
  → 加载 ESP-IDF 二级 Bootloader
  → 初始化时钟、内存、日志、Cache/MMU 与主 Flash
  → 配置看门狗、复位保护和早期随机源
  → 从 0x8000 读取并校验分区表
  → 选择可启动应用分区
  → 校验并加载应用镜像，跳转至 app_main()
```

本项目使用单应用分区表，且不存在 `otadata`、`ota_0`、`ota_1`。因此二级 Bootloader 会选择 `factory` 分区并从 `0x10000` 启动应用。当前配置启用了 Bootloader 看门狗（9 秒）和内存区域保护；未启用 OTA 回滚、Secure Boot 或 Flash Encryption。`CONFIG_SECURE_BOOT_V2_RSA_SUPPORTED` 仅表示芯片和 SDK 支持该能力，并不表示已启用 Secure Boot。

## 应用镜像占用

当前构建产物 `build/hello_world.bin` 为 660,688 B（约 645.2 KiB）：

- 占 `factory` 应用分区的约 63.0%。
- 该 1 MiB 应用槽余约 387,888 B（约 378.8 KiB）。
- 当前没有 `ota_0`、`ota_1`、`otadata` 分区，因此不支持通过分区表进行 A/B OTA 回滚。
- 当前没有 SPIFFS、LittleFS、FATFS 或 core dump 分区；项目源码也没有直接调用对应的挂载、OTA 或分区 API。

## 主 Flash 容量检测代码

`app/main.c:131–159` 在启动时调用 `esp_flash_get_size(NULL, &flash_size)` 并打印主 Flash 的容量。该代码只读取并输出容量，不会创建分区，也不会使用上表中的未分区空间。

若串口实际输出为 `8MB`，则可确认物理容量为 8 MiB；这也意味着当前 `2MB` 的构建配置与硬件容量不一致。要利用剩余空间，需要将 Flash size 改为 8MB，并启用自定义分区表后重新规划应用、OTA 和数据分区。
