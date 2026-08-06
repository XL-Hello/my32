# ESP32-S3 主 Flash 分区说明

主 Flash 按 8 MiB（`0x800000`）配置，并使用根目录的自定义 `partitions.csv`。

| 地址范围 | 大小 | 分区 | 用途 |
| --- | ---: | --- | --- |
| `0x000000–0x007FFF` | 32 KiB | Bootloader | ESP-IDF 二级 Bootloader |
| `0x008000–0x008FFF` | 4 KiB | Partition Table | 分区表与 MD5 |
| `0x009000–0x00EFFF` | 24 KiB | `nvs` | Wi-Fi 和应用配置 |
| `0x00F000–0x00FFFF` | 4 KiB | `phy_init` | PHY 初始化数据 |
| `0x010000–0x011FFF` | 8 KiB | `otadata` | 下次启动所选 OTA 分区 |
| `0x020000–0x21FFFF` | 2 MiB | `ota_0` | 应用槽 A |
| `0x220000–0x41FFFF` | 2 MiB | `ota_1` | 应用槽 B |
| `0x420000–0x5FFFFF` | 1920 KiB | 未分区 | 预留备用空间 |
| `0x600000–0x7FFFFF` | 2 MiB | `littlefs` | UI 资源、相册与本地 OTA 固件包 |

应用启动时，ESP-IDF 二级 Bootloader 读取 `otadata` 并从 `ota_0` 或 `ota_1` 启动。首次烧录该分区表时没有已选应用，ESP-IDF 的烧录流程会同时写入 `ota_data_initial.bin` 和 `ota_0`。

本地升级实现详见 [本地 OTA 升级方案](docs/本地OTA升级方案.md)。升级包位于 LittleFS 的 `/littlefs/ota/firmware.bin`，应用仅会向未运行的 OTA 槽写入数据；写入后校验失败时不会更改 `otadata`，因此仍可启动旧应用。

当前未启用 OTA 回滚或 Secure Boot。若产品需要新固件自检失败自动回退，应启用 ESP-IDF app rollback，并在新固件完成自检后确认其有效。
