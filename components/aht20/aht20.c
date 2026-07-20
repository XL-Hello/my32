#include "aht20.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LOG_TAG "aht20"
#include "platform_log.h"

#ifndef AHT20_I2C_PORT
#define AHT20_I2C_PORT I2C_NUM_0
#endif

#ifndef AHT20_I2C_SDA_GPIO
#define AHT20_I2C_SDA_GPIO 15
#endif

#ifndef AHT20_I2C_SCL_GPIO
#define AHT20_I2C_SCL_GPIO 16
#endif

#define AHT20_I2C_ADDRESS             0x38
#define AHT20_I2C_CLOCK_HZ            100000
#define AHT20_I2C_TIMEOUT_MS          1000
#define AHT20_STATUS_BUSY_MASK        0x80
#define AHT20_STATUS_READY_MASK       0x18
#define AHT20_STATUS_COMMAND          0x71
#define AHT20_INIT_COMMAND            0xBE
#define AHT20_TRIGGER_COMMAND         0xAC
#define AHT20_POWER_ON_DELAY_MS       100
#define AHT20_INIT_DELAY_MS           10
#define AHT20_MEASUREMENT_DELAY_MS    80
#define AHT20_BUSY_RETRY_DELAY_MS     10
#define AHT20_BUSY_RETRY_COUNT        5

static bool s_i2c_initialized;
static bool s_initialized;

static uint8_t aht20_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFF;

    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) != 0 ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t aht20_write(const uint8_t *data, size_t length)
{
    return i2c_master_write_to_device(AHT20_I2C_PORT, AHT20_I2C_ADDRESS, data, length,
                                      pdMS_TO_TICKS(AHT20_I2C_TIMEOUT_MS));
}

static esp_err_t aht20_read_status(uint8_t *status)
{
    const uint8_t command = AHT20_STATUS_COMMAND;

    return i2c_master_write_read_device(AHT20_I2C_PORT, AHT20_I2C_ADDRESS,
                                        &command, sizeof(command), status, 1,
                                        pdMS_TO_TICKS(AHT20_I2C_TIMEOUT_MS));
}

esp_err_t aht20_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (!s_i2c_initialized) {
        const i2c_config_t config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = AHT20_I2C_SDA_GPIO,
            .scl_io_num = AHT20_I2C_SCL_GPIO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = AHT20_I2C_CLOCK_HZ,
            .clk_flags = 0,
        };
        esp_err_t err = i2c_param_config(AHT20_I2C_PORT, &config);
        if (err == ESP_OK) {
            err = i2c_driver_install(AHT20_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
        }
        if (err != ESP_OK) {
            log_error("I2C init failed: %s", esp_err_to_name(err));
            return err;
        }
        s_i2c_initialized = true;
        vTaskDelay(pdMS_TO_TICKS(AHT20_POWER_ON_DELAY_MS));
    }

    uint8_t status = 0;
    esp_err_t err = aht20_read_status(&status);
    if (err != ESP_OK) {
        log_error("AHT20 at 0x%02X is not responding on SDA=GPIO%d SCL=GPIO%d: %s",
                  AHT20_I2C_ADDRESS, AHT20_I2C_SDA_GPIO, AHT20_I2C_SCL_GPIO,
                  esp_err_to_name(err));
        return err;
    }

    if ((status & AHT20_STATUS_READY_MASK) != AHT20_STATUS_READY_MASK) {
        const uint8_t init_command[] = {AHT20_INIT_COMMAND, 0x08, 0x00};
        err = aht20_write(init_command, sizeof(init_command));
        if (err != ESP_OK) {
            log_error("AHT20 calibration command failed: %s", esp_err_to_name(err));
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(AHT20_INIT_DELAY_MS));

        err = aht20_read_status(&status);
        if (err != ESP_OK) {
            log_error("AHT20 status read after calibration failed: %s", esp_err_to_name(err));
            return err;
        }
    }
    if ((status & AHT20_STATUS_READY_MASK) != AHT20_STATUS_READY_MASK) {
        log_error("AHT20 is not ready after calibration, status=0x%02X", status);
        return ESP_ERR_INVALID_STATE;
    }
    if ((status & AHT20_STATUS_BUSY_MASK) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    s_initialized = true;
    log_info("AHT20 initialized: I2C%d SDA=GPIO%d SCL=GPIO%d address=0x%02X",
             AHT20_I2C_PORT, AHT20_I2C_SDA_GPIO, AHT20_I2C_SCL_GPIO, AHT20_I2C_ADDRESS);
    return ESP_OK;
}

esp_err_t aht20_read(float *temperature_c, float *humidity_rh)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (temperature_c == NULL || humidity_rh == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t trigger_command[] = {AHT20_TRIGGER_COMMAND, 0x33, 0x00};
    esp_err_t err = aht20_write(trigger_command, sizeof(trigger_command));
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(AHT20_MEASUREMENT_DELAY_MS));

    uint8_t data[7] = {0};
    for (uint8_t attempt = 0; attempt < AHT20_BUSY_RETRY_COUNT; ++attempt) {
        err = i2c_master_read_from_device(AHT20_I2C_PORT, AHT20_I2C_ADDRESS, data,
                                          sizeof(data), pdMS_TO_TICKS(AHT20_I2C_TIMEOUT_MS));
        if (err != ESP_OK) {
            return err;
        }
        if ((data[0] & AHT20_STATUS_BUSY_MASK) == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(AHT20_BUSY_RETRY_DELAY_MS));
    }
    if ((data[0] & AHT20_STATUS_BUSY_MASK) != 0) {
        log_warn("AHT20 measurement is still busy after %d ms",
                 AHT20_MEASUREMENT_DELAY_MS +
                 AHT20_BUSY_RETRY_COUNT * AHT20_BUSY_RETRY_DELAY_MS);
        return ESP_ERR_INVALID_STATE;
    }
    if (aht20_crc8(data, sizeof(data) - 1) != data[sizeof(data) - 1]) {
        log_warn("AHT20 CRC check failed");
        return ESP_ERR_INVALID_CRC;
    }

    const uint32_t humidity_raw = ((uint32_t)data[1] << 12) |
                                  ((uint32_t)data[2] << 4) |
                                  ((uint32_t)data[3] >> 4);
    const uint32_t temperature_raw = ((uint32_t)(data[3] & 0x0F) << 16) |
                                     ((uint32_t)data[4] << 8) |
                                     data[5];
    *humidity_rh = (float)humidity_raw * 100.0f / 1048576.0f;
    *temperature_c = (float)temperature_raw * 200.0f / 1048576.0f - 50.0f;
    return ESP_OK;
}
