#ifndef W25Q64_H
#define W25Q64_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "sim_spi.h"

#define W25Q64_PAGE_SIZE          256U
#define W25Q64_SECTOR_SIZE        4096U
#define W25Q64_CAPACITY_BYTES     (8UL * 1024UL * 1024UL)
#define W25Q64_JEDEC_CAPACITY_CODE 0x17U

typedef struct {
    sim_spi_config_t *spi;
} w25q64_t;

/**
 * @brief 绑定已初始化的软件 SPI 总线。
 *
 * W25Q64 支持 SPI Mode 0 和 Mode 3。调用前须先成功执行 sim_spi_init()。
 */
esp_err_t w25q64_init(w25q64_t *device, sim_spi_config_t *spi);

/** @brief 读取 24 位 JEDEC ID，典型 W25Q64 为 0xEF4017。 */
esp_err_t w25q64_read_jedec_id(w25q64_t *device, uint32_t *jedec_id);

/** @brief 等待 WIP 清零。 */
esp_err_t w25q64_wait_ready(w25q64_t *device, uint32_t timeout_ms);

/**
 * @brief 擦除一个 4 KiB 扇区。
 *
 * address 必须为 4 KiB 对齐地址。该操作会永久清除目标扇区内容。
 */
esp_err_t w25q64_erase_sector(w25q64_t *device, uint32_t address);

/** @brief 写入任意长度数据；内部自动按 256 字节页边界拆分。 */
esp_err_t w25q64_write(w25q64_t *device, uint32_t address, const uint8_t *data, size_t len);

/** @brief 从指定地址读取数据。 */
esp_err_t w25q64_read(w25q64_t *device, uint32_t address, uint8_t *data, size_t len);

#endif // W25Q64_H
