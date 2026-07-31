#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "w25q64.h"

#define W25Q64_CMD_WRITE_ENABLE      0x06U
#define W25Q64_CMD_READ_STATUS_1     0x05U
#define W25Q64_CMD_READ_DATA          0x03U
#define W25Q64_CMD_PAGE_PROGRAM       0x02U
#define W25Q64_CMD_SECTOR_ERASE_4K    0x20U
#define W25Q64_CMD_JEDEC_ID           0x9FU

#define W25Q64_STATUS_1_BUSY          (1U << 0)
#define W25Q64_STATUS_1_WRITE_ENABLE  (1U << 1)

#define W25Q64_READ_CHUNK_SIZE            256U
#define W25Q64_PAGE_PROGRAM_TIMEOUT_MS     20U
#define W25Q64_SECTOR_ERASE_TIMEOUT_MS   1000U

static bool w25q64_range_is_valid(uint32_t address, size_t len)
{
    return len > 0 && address < W25Q64_CAPACITY_BYTES &&
           len <= W25Q64_CAPACITY_BYTES - address;
}

static esp_err_t w25q64_check_device(const w25q64_t *device)
{
    if (device == NULL || device->spi == NULL || device->spi->timer_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (device->spi->mode != SIM_SPI_MODE_0 && device->spi->mode != SIM_SPI_MODE_3) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static void w25q64_set_address(uint8_t *command, uint32_t address)
{
    command[1] = (uint8_t)(address >> 16U);
    command[2] = (uint8_t)(address >> 8U);
    command[3] = (uint8_t)address;
}

static esp_err_t w25q64_read_status_1(w25q64_t *device, uint8_t *status)
{
    uint8_t tx[] = { W25Q64_CMD_READ_STATUS_1, 0xFFU };
    uint8_t rx[sizeof(tx)] = { 0 };

    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = w25q64_check_device(device);
    if (err != ESP_OK) {
        return err;
    }

    sim_spi_transfer(device->spi, tx, rx, sizeof(tx));
    *status = rx[1];
    return ESP_OK;
}

static esp_err_t w25q64_write_enable(w25q64_t *device)
{
    uint8_t command = W25Q64_CMD_WRITE_ENABLE;
    uint8_t status = 0;

    esp_err_t err = w25q64_check_device(device);
    if (err != ESP_OK) {
        return err;
    }

    sim_spi_transfer(device->spi, &command, NULL, sizeof(command));
    err = w25q64_read_status_1(device, &status);
    if (err != ESP_OK) {
        return err;
    }
    return (status & W25Q64_STATUS_1_WRITE_ENABLE) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static esp_err_t w25q64_page_program(w25q64_t *device, uint32_t address,
                                      const uint8_t *data, size_t len)
{
    uint8_t tx[4 + W25Q64_PAGE_SIZE] = { W25Q64_CMD_PAGE_PROGRAM };

    if (data == NULL || len == 0 || len > W25Q64_PAGE_SIZE ||
        ((address & (W25Q64_PAGE_SIZE - 1U)) + len > W25Q64_PAGE_SIZE)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = w25q64_wait_ready(device, W25Q64_PAGE_PROGRAM_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    err = w25q64_write_enable(device);
    if (err != ESP_OK) {
        return err;
    }

    w25q64_set_address(tx, address);
    memcpy(&tx[4], data, len);
    sim_spi_transfer(device->spi, tx, NULL, (uint16_t)(4U + len));
    return w25q64_wait_ready(device, W25Q64_PAGE_PROGRAM_TIMEOUT_MS);
}

esp_err_t w25q64_init(w25q64_t *device, sim_spi_config_t *spi)
{
    if (device == NULL || spi == NULL || spi->timer_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (spi->mode != SIM_SPI_MODE_0 && spi->mode != SIM_SPI_MODE_3) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    device->spi = spi;
    return ESP_OK;
}

esp_err_t w25q64_read_jedec_id(w25q64_t *device, uint32_t *jedec_id)
{
    uint8_t tx[] = { W25Q64_CMD_JEDEC_ID, 0xFFU, 0xFFU, 0xFFU };
    uint8_t rx[sizeof(tx)] = { 0 };

    if (jedec_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = w25q64_check_device(device);
    if (err != ESP_OK) {
        return err;
    }

    sim_spi_transfer(device->spi, tx, rx, sizeof(tx));
    *jedec_id = ((uint32_t)rx[1] << 16U) | ((uint32_t)rx[2] << 8U) | rx[3];
    return ESP_OK;
}

esp_err_t w25q64_wait_ready(w25q64_t *device, uint32_t timeout_ms)
{
    const TickType_t delay_ticks = pdMS_TO_TICKS(1) ? pdMS_TO_TICKS(1) : 1;

    for (uint32_t elapsed_ms = 0; elapsed_ms <= timeout_ms; elapsed_ms++) {
        uint8_t status = 0;
        esp_err_t err = w25q64_read_status_1(device, &status);
        if (err != ESP_OK) {
            return err;
        }
        if ((status & W25Q64_STATUS_1_BUSY) == 0) {
            return ESP_OK;
        }
        vTaskDelay(delay_ticks);
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t w25q64_erase_sector(w25q64_t *device, uint32_t address)
{
    uint8_t tx[] = { W25Q64_CMD_SECTOR_ERASE_4K, 0, 0, 0 };

    if (!w25q64_range_is_valid(address, W25Q64_SECTOR_SIZE) ||
        (address & (W25Q64_SECTOR_SIZE - 1U)) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = w25q64_wait_ready(device, W25Q64_SECTOR_ERASE_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    err = w25q64_write_enable(device);
    if (err != ESP_OK) {
        return err;
    }

    w25q64_set_address(tx, address);
    sim_spi_transfer(device->spi, tx, NULL, sizeof(tx));
    return w25q64_wait_ready(device, W25Q64_SECTOR_ERASE_TIMEOUT_MS);
}

esp_err_t w25q64_write(w25q64_t *device, uint32_t address, const uint8_t *data, size_t len)
{
    if (data == NULL || !w25q64_range_is_valid(address, len)) {
        return ESP_ERR_INVALID_ARG;
    }

    while (len > 0) {
        const size_t page_offset = address & (W25Q64_PAGE_SIZE - 1U);
        const size_t page_remaining = W25Q64_PAGE_SIZE - page_offset;
        const size_t chunk_len = len < page_remaining ? len : page_remaining;
        esp_err_t err = w25q64_page_program(device, address, data, chunk_len);
        if (err != ESP_OK) {
            return err;
        }

        address += chunk_len;
        data += chunk_len;
        len -= chunk_len;
    }
    return ESP_OK;
}

esp_err_t w25q64_read(w25q64_t *device, uint32_t address, uint8_t *data, size_t len)
{
    uint8_t tx[4 + W25Q64_READ_CHUNK_SIZE] = { 0 };
    uint8_t rx[sizeof(tx)] = { 0 };

    if (data == NULL || !w25q64_range_is_valid(address, len)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = w25q64_check_device(device);
    if (err != ESP_OK) {
        return err;
    }

    while (len > 0) {
        const size_t chunk_len = len < W25Q64_READ_CHUNK_SIZE ? len : W25Q64_READ_CHUNK_SIZE;

        tx[0] = W25Q64_CMD_READ_DATA;
        w25q64_set_address(tx, address);
        memset(&tx[4], 0xFF, chunk_len);
        sim_spi_transfer(device->spi, tx, rx, (uint16_t)(4U + chunk_len));
        memcpy(data, &rx[4], chunk_len);

        address += chunk_len;
        data += chunk_len;
        len -= chunk_len;
    }
    return ESP_OK;
}
