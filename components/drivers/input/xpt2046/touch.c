#include "touch.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "lcd.h"

#define LOG_TAG "touch"
#include "platform_log.h"

#define TOUCH_HOST SPI2_HOST
#define TOUCH_SPI_CLOCK_HZ (2 * 1000 * 1000)
#define TOUCH_AXIS_SAMPLE_COUNT 5
#define TOUCH_AXIS_MIN_VALID_SAMPLES 3

/*
 * 五点最小二乘仿射校准，采集于 LCD_ORIENTATION_PORTRAIT_INVERTED。
 *
 * screen_x =  0.06737225 * raw_x + 0.00064867 * raw_y - 16.61356263
 * screen_y =  0.00069024 * raw_x - 0.09033900 * raw_y + 350.29080470
 */
#define TOUCH_CAL_X_RAW_X 0.06737225f
#define TOUCH_CAL_X_RAW_Y 0.00064867f
#define TOUCH_CAL_X_OFFSET -16.61356263f
#define TOUCH_CAL_Y_RAW_X 0.00069024f
#define TOUCH_CAL_Y_RAW_Y -0.09033900f
#define TOUCH_CAL_Y_OFFSET 350.29080470f

static spi_device_handle_t s_touch_device;

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
    /* LVGL 输入回调轮询 T_IRQ：低电平表示触摸按下。 */
    if (gpio_get_level(TOUCH_PIN_IRQ) != 0) {
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

static uint16_t touch_clamp_coordinate(float coordinate, uint16_t maximum)
{
    if (coordinate <= 0.0f) {
        return 0;
    }
    if (coordinate >= (float)maximum) {
        return maximum;
    }
    return (uint16_t)(coordinate + 0.5f);
}

esp_err_t touch_read_point(touch_point_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    touch_raw_data_t raw_data;
    esp_err_t err = touch_read_raw(&raw_data);
    if (err != ESP_OK) {
        return err;
    }

    *data = (touch_point_t){
        .pressed = raw_data.pressed,
        .valid = raw_data.valid,
    };
    if (!raw_data.valid) {
        return ESP_OK;
    }

    const float x = TOUCH_CAL_X_RAW_X * raw_data.raw_x +
                    TOUCH_CAL_X_RAW_Y * raw_data.raw_y + TOUCH_CAL_X_OFFSET;
    const float y = TOUCH_CAL_Y_RAW_X * raw_data.raw_x +
                    TOUCH_CAL_Y_RAW_Y * raw_data.raw_y + TOUCH_CAL_Y_OFFSET;
    data->x = touch_clamp_coordinate(x, LCD_H_RES - 1);
    data->y = touch_clamp_coordinate(y, LCD_V_RES - 1);
    return ESP_OK;
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
        .intr_type = GPIO_INTR_DISABLE,
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

    /*
     * 上电后 PENIRQ 状态没有可依赖的保证，不能仅凭 GPIO 高电平认定未触摸。
     * 0xD0 的 PD1/PD0 均为 0；完成这一次 24 时钟转换后，HR2046 会进入
     * 自动掉电且允许 PENIRQ 的确定状态（未触摸为高，触摸为低）。返回的
     * 坐标值不用于初始化。
     */
    uint16_t discarded_sample;
    err = touch_read_axis(TOUCH_CMD_READ_X, &discarded_sample);
    if (err != ESP_OK) {
        spi_bus_remove_device(s_touch_device);
        s_touch_device = NULL;
        log_error("HR2046 initial command failed: %s", esp_err_to_name(err));
        return err;
    }

    log_info("HR2046 input initialized: SPI=%d CS=%d IRQ=%d mode=%d clock=%dHz, LVGL polling",
             TOUCH_HOST, TOUCH_PIN_CS, TOUCH_PIN_IRQ, device_config.mode,
             TOUCH_SPI_CLOCK_HZ);
    return ESP_OK;
}
