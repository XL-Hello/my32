#include <inttypes.h>
#include <string.h>

#include "sim_spi.h"
#include "w25q64.h"
#include "w25q64_test.h"

#define LOG_TAG "w25q64"
#include "platform_log.h"

#define W25Q64_TEST_ADDRESS (W25Q64_CAPACITY_BYTES - W25Q64_SECTOR_SIZE)

static sim_spi_config_t s_w25q64_spi;
static w25q64_t s_w25q64;

esp_err_t w25q64_communication_test(void)
{
    static const uint8_t write_data[] = {
        0x53, 0x49, 0x4D, 0x2D, 0x53, 0x50, 0x49, 0x20,
        0x57, 0x32, 0x35, 0x51, 0x36, 0x34, 0x20, 0x4F,
        0x4B,
    };
    uint8_t read_data[sizeof(write_data)] = { 0 };
    uint32_t jedec_id = 0;

    esp_err_t result = sim_spi_init(&s_w25q64_spi);
    if (result != ESP_OK) {
        return result;
    }

    esp_err_t err = w25q64_init(&s_w25q64, &s_w25q64_spi);
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    err = w25q64_read_jedec_id(&s_w25q64, &jedec_id);
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    log_info("JEDEC ID: 0x%06" PRIX32, jedec_id);
    if ((jedec_id & 0xFFU) != W25Q64_JEDEC_CAPACITY_CODE) {
        log_error("unexpected flash capacity code: 0x%02" PRIX32,
                  jedec_id & 0xFFU);
        result = ESP_ERR_NOT_FOUND;
        goto cleanup;
    }

    err = w25q64_erase_sector(&s_w25q64, W25Q64_TEST_ADDRESS);
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    err = w25q64_write(&s_w25q64, W25Q64_TEST_ADDRESS, write_data, sizeof(write_data));
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    err = w25q64_read(&s_w25q64, W25Q64_TEST_ADDRESS, read_data, sizeof(read_data));
    if (err != ESP_OK) {
        result = err;
        goto cleanup;
    }
    if (memcmp(write_data, read_data, sizeof(write_data)) != 0) {
        result = ESP_ERR_INVALID_RESPONSE;
        goto cleanup;
    }

    log_info("write/read verification passed at 0x%06" PRIX32,
             (uint32_t)W25Q64_TEST_ADDRESS);
    result = ESP_OK;

cleanup:
    err = sim_spi_deinit(&s_w25q64_spi);
    if (result == ESP_OK && err != ESP_OK) {
        result = err;
    }
    return result;
}
