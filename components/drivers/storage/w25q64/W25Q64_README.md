# W25Q64 组件开发参考

本文档说明本仓库 `components/drivers/storage/w25q64` 的**当前实现**及其使用约束，供在 ESP-IDF 项目中接入 W25Q64（8 MiB SPI NOR Flash）时查阅。芯片的完整指令、时序和电气参数仍应以所用器件版本的数据手册为准。

## 1. 组件范围

该组件通过 `sim_spi` 软件 SPI 驱动 W25Q64 的标准 SPI 指令集，目前实现以下能力：

| 能力 | 公开 API | 底层指令 |
| --- | --- | --- |
| 初始化并绑定 SPI 总线 | `w25q64_init()` | — |
| 读取 JEDEC ID | `w25q64_read_jedec_id()` | `0x9F` |
| 查询忙状态 | `w25q64_wait_ready()` | `0x05` |
| 擦除一个 4 KiB 扇区 | `w25q64_erase_sector()` | `0x06` + `0x20` |
| 读取任意长度数据 | `w25q64_read()` | `0x03` |
| 写入任意长度数据 | `w25q64_write()` | `0x06` + `0x02` |

`w25q64_write()` 会按页边界自动拆分，调用方不需要自行拆成 256 字节块；但调用方必须根据数据生命周期自行规划、擦除存储区域。

当前组件**没有**提供 Fast Read、Dual/Quad SPI、状态寄存器配置、写保护、32/64 KiB 块擦除、整片擦除、掉电和软件复位 API。不要将这些数据手册指令误认为组件已支持的功能。

## 2. 存储布局和基本规则

| 项目 | 数值 | 对应宏 |
| --- | ---: | --- |
| 总容量 | 8 MiB（64 Mbit） | `W25Q64_CAPACITY_BYTES` |
| 有效地址 | `0x000000` ～ `0x7FFFFF` | — |
| 页大小 | 256 字节 | `W25Q64_PAGE_SIZE` |
| 擦除扇区 | 4 KiB | `W25Q64_SECTOR_SIZE` |
| JEDEC 容量码 | `0x17` | `W25Q64_JEDEC_CAPACITY_CODE` |
| 典型 JEDEC ID | `0xEF4017` | — |

SPI NOR Flash 只能把位从 `1` 编程为 `0`。若要将某一位恢复为 `1`，必须先擦除其所在扇区；擦除后整个 4 KiB 扇区都变为 `0xFF`。因此，覆盖写已有数据前应先判断该扇区是否可直接编程；通常更简单可靠的流程是“读出并合并旧数据 → 擦除扇区 → 写回完整扇区”。

> `w25q64_erase_sector()` 会永久清除整个目标扇区。请在应用中划定专用 Flash 区域，不能与配置、日志或其他业务数据共用。

## 3. 硬件连接和总线限制

### 3.1 当前 `sim_spi` 固定配置

当前 `sim_spi_init()` 会重置传入结构并使用下列固定配置：

| W25Q64 信号 | 芯片引脚名 | ESP GPIO | 方向 |
| --- | --- | ---: | --- |
| 数据输入 | `DI` / `D1` | GPIO35 | MCU → Flash |
| 时钟 | `CLK` | GPIO36 | MCU → Flash |
| 数据输出 | `DO` / `D0` | GPIO37 | Flash → MCU |
| 片选 | `CS#` | GPIO38 | MCU → Flash |
| 写保护 | `WP#` / `IO2` | 拉高至 3.3 V | — |
| 保持/复位 | `HOLD#` / `RESET#` / `IO3` | 拉高至 3.3 V | — |
| 电源 | `VCC`、`GND` | 3.3 V、GND | — |

还固定使用 SPI Mode 0 和 50 kHz。W25Q64 本身也支持 Mode 3，且 `w25q64_init()` 会接受 Mode 0 或 Mode 3；不过 `sim_spi` 目前没有公开的配置入口，不能仅在 `sim_spi_init()` 后直接改写 `mode` 字段。若需更换引脚、频率或 Mode 3，应先完善 `sim_spi` 的初始化配置，并同步重新计算时钟空闲电平和采样沿。

W25Q64 使用 3.3 V 逻辑电平，不应直接接到 5 V GPIO。`WP#` 和 `HOLD#`/`RESET#` 未正确拉高时，常会表现为写使能失败、读写异常或无法响应。

### 3.2 并发约束

底层 `sim_spi_transfer()` 不可重入，只支持单任务调用。本组件也没有锁；多个任务共用同一个 `w25q64_t` 时，必须由调用方使用 mutex 等方式串行化整个“擦除/写入/读取”操作。

## 4. 集成方式

应用组件需要在其 `CMakeLists.txt` 的 `REQUIRES` 中声明依赖：

```cmake
idf_component_register(
    SRCS "main.c"
    REQUIRES W25Q64 sim_spi
)
```

随后包含头文件：

```c
#include "sim_spi.h"
#include "w25q64.h"
```

`sim_spi_config_t` 和 `w25q64_t` 应具有足够长的生命周期，通常定义为 `static` 全局变量；不要把它们定义为已返回函数中的局部变量。

## 5. 初始化和设备识别

下面是一个最小、可复用的初始化示例。它只读取 ID，不会修改 Flash 内容。

```c
#include <inttypes.h>

#include "esp_check.h"
#include "esp_log.h"
#include "sim_spi.h"
#include "w25q64.h"

static const char *TAG = "w25q64_example";
static sim_spi_config_t s_spi;
static w25q64_t s_flash;

static esp_err_t external_flash_init(void)
{
    uint32_t jedec_id = 0;

    ESP_RETURN_ON_ERROR(sim_spi_init(&s_spi), TAG, "初始化软件 SPI 失败");
    ESP_RETURN_ON_ERROR(w25q64_init(&s_flash, &s_spi), TAG, "初始化 W25Q64 失败");
    ESP_RETURN_ON_ERROR(w25q64_read_jedec_id(&s_flash, &jedec_id), TAG, "读取 JEDEC ID 失败");

    ESP_LOGI(TAG, "JEDEC ID: 0x%06" PRIX32, jedec_id);
    if (jedec_id != 0xEF4017U) {
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}
```

若希望兼容同容量、不同厂商的器件，可只校验容量码：

```c
if ((jedec_id & 0xFFU) != W25Q64_JEDEC_CAPACITY_CODE) {
    return ESP_ERR_NOT_FOUND;
}
```

不再使用总线时，在没有传输进行的情况下调用：

```c
ESP_ERROR_CHECK(sim_spi_deinit(&s_spi));
```

`sim_spi_deinit()` 会释放 GPTimer 并清空 SPI 配置。之后若要继续访问 Flash，必须再次执行 `sim_spi_init()` 和 `w25q64_init()`。

## 6. API 速查

### `w25q64_init`

```c
esp_err_t w25q64_init(w25q64_t *device, sim_spi_config_t *spi);
```

绑定已经成功初始化的 `sim_spi` 总线。`device`、`spi` 和 `spi->timer_handle` 均不能为空；SPI 模式必须为 Mode 0 或 Mode 3。成功后 `device->spi` 指向该总线。

### `w25q64_read_jedec_id`

```c
esp_err_t w25q64_read_jedec_id(w25q64_t *device, uint32_t *jedec_id);
```

读取 24 位 ID，结果格式为 `0x<厂商ID><存储器类型><容量码>`，例如 Winbond W25Q64 常见值为 `0xEF4017`。这是排查接线、供电和 SPI 模式的首选只读操作。

### `w25q64_wait_ready`

```c
esp_err_t w25q64_wait_ready(w25q64_t *device, uint32_t timeout_ms);
```

每约 1 ms 读取一次状态寄存器 1 的 WIP/BUSY 位，直到设备空闲或超时。常规读写 API 会在需要时自行调用它；仅在应用执行了额外底层操作时才需要手动调用。

### `w25q64_erase_sector`

```c
esp_err_t w25q64_erase_sector(w25q64_t *device, uint32_t address);
```

擦除 `address` 所在的 4 KiB 扇区。地址必须满足以下条件：

```c
address % W25Q64_SECTOR_SIZE == 0
address <= W25Q64_CAPACITY_BYTES - W25Q64_SECTOR_SIZE
```

驱动会等待空闲、发送写使能、发送擦除命令，并等待完成；内部超时为 1000 ms。

### `w25q64_write`

```c
esp_err_t w25q64_write(w25q64_t *device, uint32_t address,
                        const uint8_t *data, size_t len);
```

向 `[address, address + len)` 写入数据。`data` 不能为空，`len` 必须大于 0，且范围不得超过 `0x800000`。驱动会自动拆分跨页数据、对每页执行写使能、写入和等待完成；单页编程的内部超时为 20 ms。

函数不会自动擦除目标区域，也不会替你保护相邻数据。写入前请确保目标位可从 `1` 变为 `0`，或显式先擦除对应扇区。

### `w25q64_read`

```c
esp_err_t w25q64_read(w25q64_t *device, uint32_t address,
                       uint8_t *data, size_t len);
```

从 `[address, address + len)` 读取数据。参数范围规则与 `w25q64_write()` 相同。组件内部按最多 256 字节分段读取，调用方可传入任意合法长度。

## 7. 推荐读写流程

以下示例将一个短记录写入已预留的第一个扇区，并立即回读校验。示例中的地址仅作演示：实际项目应集中定义 Flash 分区，避免不同功能重叠。

```c
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

#define APP_FLASH_BASE 0x000000U

static esp_err_t save_record(void)
{
    static const uint8_t write_data[] = "config-v1";
    uint8_t read_data[sizeof(write_data)] = { 0 };

    ESP_RETURN_ON_ERROR(w25q64_erase_sector(&s_flash, APP_FLASH_BASE),
                        TAG, "擦除记录扇区失败");
    ESP_RETURN_ON_ERROR(w25q64_write(&s_flash, APP_FLASH_BASE,
                                     write_data, sizeof(write_data)),
                        TAG, "写入记录失败");
    ESP_RETURN_ON_ERROR(w25q64_read(&s_flash, APP_FLASH_BASE,
                                    read_data, sizeof(read_data)),
                        TAG, "回读记录失败");

    if (memcmp(write_data, read_data, sizeof(write_data)) != 0) {
        ESP_LOGE(TAG, "回读数据不一致");
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
```

对于同一扇区内的局部更新，应按下述顺序处理，以免误删同扇区的其他记录：

1. 读出完整 4 KiB 扇区到 RAM。
2. 在 RAM 缓冲中修改目标内容。
3. 擦除该扇区。
4. 将完整缓冲写回。
5. 回读并校验关键数据；重要配置还应使用版本号、长度和 CRC。

## 8. 返回值和排障

| 现象或返回值 | 常见原因 | 检查方向 |
| --- | --- | --- |
| `ESP_ERR_INVALID_ARG` | 空指针、长度为 0、地址越界或擦除地址未 4 KiB 对齐 | 核对缓冲区、长度和地址 |
| `ESP_ERR_INVALID_STATE` | 未初始化 SPI、总线已释放或写使能未生效 | 先执行 `sim_spi_init()`，检查 `WP#`/`HOLD#` |
| `ESP_ERR_NOT_SUPPORTED` | SPI 模式不是 Mode 0 或 Mode 3 | 使用当前默认 Mode 0 |
| `ESP_ERR_TIMEOUT` | 编程或擦除一直处于忙状态 | 检查供电、接线和芯片状态；必要时调整驱动超时 |
| JEDEC ID 为 `0x000000`、`0xFFFFFF` 或不稳定 | CS/MISO 接线、供电、芯片方向或模式异常 | 从供电和四根 SPI 信号逐项检查 |
| 写后数据未改变 | 未先擦除、写保护有效或地址错误 | 确认目标区域已擦除为 `0xFF`，并检查 `WP#` |
| 多任务下偶发读写错误 | 同时访问了不可重入的软件 SPI | 为整段事务加锁 |

仓库 `app/main.c` 已包含一次通信自检：读取 JEDEC ID 后擦除、写入并回读最后一个扇区 `0x7FF000`。该测试会破坏该扇区内容，仅应在它未用于业务数据时执行。

## 9. 数据手册指令对照

下表可用于阅读逻辑分析仪抓包或扩展驱动；“已使用”表示当前组件实际发出的命令。

| 指令 | 名称 | 当前状态 | 说明 |
| --- | --- | --- | --- |
| `0x9F` | Read JEDEC ID | 已使用 | 返回厂商、类型和容量码 |
| `0x05` | Read Status Register-1 | 已使用 | 读取 WIP/BUSY 和 WEL |
| `0x06` | Write Enable | 已使用（内部） | 每次编程、擦除前发送 |
| `0x03` | Read Data | 已使用 | 24 位地址普通读取，无 dummy byte |
| `0x02` | Page Program | 已使用（内部） | 最多 256 字节，不能跨页 |
| `0x20` | Sector Erase | 已使用 | 擦除 4 KiB 扇区 |
| `0x0B` | Fast Read | 未实现 | 需要 dummy byte |
| `0x32`、`0x6B`、`0xEB` | Quad Program/Read | 未实现 | 需要配置 QE 位和四线连接 |
| `0x52`、`0xD8`、`0xC7`/`0x60` | 块/整片擦除 | 未实现 | 需另行封装并评估擦除时间 |
| `0x66` + `0x99` | Enable Reset / Reset | 未实现 | 用于软件复位 |
| `0xB9` / `0xAB` | Power Down / Release | 未实现 | 用于低功耗场景 |

## 10. 开发检查清单

- [ ] 确认 Flash 供电为 3.3 V，`WP#` 和 `HOLD#`/`RESET#` 为高电平。
- [ ] 确认 GPIO35/36/37/38 与当前 `sim_spi` 固定接线一致。
- [ ] 首先调用 `w25q64_read_jedec_id()`，再进行破坏性读写测试。
- [ ] 为应用划分独占的、按 4 KiB 对齐的存储区域。
- [ ] 每次需要将位从 `0` 恢复为 `1` 前，先擦除整个相关扇区。
- [ ] 对关键数据在写后回读校验，并在记录中保存 CRC、长度和版本。
- [ ] 多任务访问时为整个 Flash 操作序列加锁。
- [ ] 使用结束后调用 `sim_spi_deinit()` 释放 GPTimer；不再访问已释放的设备。
