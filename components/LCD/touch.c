#include "touch.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LOG_TAG "touch"
#include "platform_log.h"

#define TOUCH_HOST SPI2_HOST
#define TOUCH_SPI_CLOCK_HZ (1 * 1000 * 1000)
#define TOUCH_POLL_PERIOD_MS 20
#define TOUCH_MOVE_LOG_PERIOD_MS 100
#define TOUCH_AXIS_SAMPLE_COUNT 5
#define TOUCH_AXIS_MIN_VALID_SAMPLES 3
#define TOUCH_TASK_STACK_SIZE 3072
#define TOUCH_TASK_PRIORITY 4

static spi_device_handle_t s_touch_device;
static TaskHandle_t s_touch_task;
static volatile bool s_touch_active;

static void IRAM_ATTR touch_irq_isr_handler(void *arg)
{
    (void)arg;

    /* HR2046 的 T_IRQ 通常低有效：下降沿开始触摸，上升沿结束触摸。 */
    s_touch_active = gpio_get_level(TOUCH_PIN_IRQ) == 0;
}

static esp_err_t touch_read_axis(uint8_t command, uint16_t *value)
{
    spi_transaction_t transaction = {
        .length = 24,
        .rxlength = 24,
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    };
    transaction.tx_data[0] = command;
    transaction.tx_data[1] = 0;
    transaction.tx_data[2] = 0;

    /* 与 esp_lcd 的队列式 DMA 刷新使用同一事务模式，避免混用 polling SPI 事务。 */
    esp_err_t err = spi_device_transmit(s_touch_device, &transaction);
    if (err != ESP_OK) {
        return err;
    }

    *value = (uint16_t)((((uint16_t)transaction.rx_data[1] << 8) |
                         transaction.rx_data[2]) >> 3) & 0x0FFF;
    return ESP_OK;
}

static esp_err_t touch_read_axis_filtered(uint8_t command, uint16_t *value, bool *valid)
{
    uint16_t samples[TOUCH_AXIS_SAMPLE_COUNT - 1];
    size_t sample_count = 0;

    for (size_t index = 0; index < TOUCH_AXIS_SAMPLE_COUNT; ++index) {
        uint16_t sample;
        esp_err_t err = touch_read_axis(command, &sample);
        if (err != ESP_OK) {
            return err;
        }

        /* 通道切换后的第一笔转换用于稳定 ADC，不参与中位数计算。 */
        if (index == 0 || sample == 0 || sample == 0x0FFF) {
            continue;
        }
        samples[sample_count++] = sample;
    }

    if (sample_count < TOUCH_AXIS_MIN_VALID_SAMPLES) {
        *valid = false;
        return ESP_OK;
    }

    for (size_t index = 1; index < sample_count; ++index) {
        uint16_t current = samples[index];
        size_t position = index;
        while (position > 0 && samples[position - 1] > current) {
            samples[position] = samples[position - 1];
            --position;
        }
        samples[position] = current;
    }

    *value = samples[(sample_count - 1) / 2];
    *valid = true;
    return ESP_OK;
}

esp_err_t touch_read_raw(touch_raw_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_touch_device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    *data = (touch_raw_data_t){0};
    if (!s_touch_active) {
        return ESP_OK;
    }

    bool x_valid;
    bool y_valid;
    esp_err_t err = touch_read_axis_filtered(TOUCH_CMD_READ_X, &data->raw_x, &x_valid);
    if (err != ESP_OK) {
        return err;
    }
    err = touch_read_axis_filtered(TOUCH_CMD_READ_Y, &data->raw_y, &y_valid);
    if (err != ESP_OK) {
        return err;
    }

    data->pressed = true;
    data->valid = x_valid && y_valid;
    if (!data->valid) {
        data->raw_x = 0;
        data->raw_y = 0;
    }
    return ESP_OK;
}

static void touch_raw_log_task(void *arg)
{
    (void)arg;

    bool has_valid_sample = false;
    uint16_t last_x = 0;
    uint16_t last_y = 0;
    TickType_t last_move_log = 0;

    while (true) {
        touch_raw_data_t data;
        esp_err_t err = touch_read_raw(&data);
        if (err != ESP_OK) {
            log_error("raw read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_PERIOD_MS));
            continue;
        }

        TickType_t now = xTaskGetTickCount();
        if (!data.pressed) {
            has_valid_sample = false;
        } else if (data.valid && !has_valid_sample) {
            log_info("pressed: raw_x=%u raw_y=%u", data.raw_x, data.raw_y);
            last_move_log = now;
            has_valid_sample = true;
        } else if (data.valid &&
                   (data.raw_x != last_x || data.raw_y != last_y) &&
                   now - last_move_log >= pdMS_TO_TICKS(TOUCH_MOVE_LOG_PERIOD_MS)) {
            log_info("moved: raw_x=%u raw_y=%u", data.raw_x, data.raw_y);
            last_move_log = now;
        }

        if (data.valid) {
            last_x = data.raw_x;
            last_y = data.raw_y;
        }
        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_PERIOD_MS));
    }
}

esp_err_t touch_init(void)
{
    if (s_touch_device != NULL) {
        return ESP_OK;
    }

    const gpio_config_t irq_config = {
        .pin_bit_mask = 1ULL << TOUCH_PIN_IRQ,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t err = gpio_config(&irq_config);
    if (err != ESP_OK) {
        log_error("T_IRQ GPIO init failed: %s", esp_err_to_name(err));
        return err;
    }

    const spi_device_interface_config_t device_config = {
        .clock_speed_hz = TOUCH_SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = TOUCH_PIN_CS,
        .queue_size = 1,
    };
    err = spi_bus_add_device(TOUCH_HOST, &device_config, &s_touch_device);
    if (err != ESP_OK) {
        log_error("SPI device init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_touch_active = gpio_get_level(TOUCH_PIN_IRQ) == 0;
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        spi_bus_remove_device(s_touch_device);
        s_touch_device = NULL;
        log_error("GPIO ISR service init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = gpio_isr_handler_add(TOUCH_PIN_IRQ, touch_irq_isr_handler, NULL);
    if (err != ESP_OK) {
        spi_bus_remove_device(s_touch_device);
        s_touch_device = NULL;
        log_error("T_IRQ handler install failed: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t task_created = xTaskCreate(touch_raw_log_task, "touch_raw",
                                          TOUCH_TASK_STACK_SIZE, NULL,
                                          TOUCH_TASK_PRIORITY, &s_touch_task);
    if (task_created != pdPASS) {
        gpio_isr_handler_remove(TOUCH_PIN_IRQ);
        spi_bus_remove_device(s_touch_device);
        s_touch_device = NULL;
        log_error("raw log task creation failed");
        return ESP_ERR_NO_MEM;
    }

    log_info("HR2046 raw test initialized: SPI=%d CS=%d IRQ=%d mode=%d clock=%dHz",
             TOUCH_HOST, TOUCH_PIN_CS, TOUCH_PIN_IRQ, device_config.mode,
             TOUCH_SPI_CLOCK_HZ);
    return ESP_OK;
}
