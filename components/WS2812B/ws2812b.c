#include "ws2812b.h"

#include <stdbool.h>

#include "led_strip.h"

#if WS2812B_BACKEND == WS2812B_BACKEND_RMT
#include "led_strip_rmt.h"
#elif WS2812B_BACKEND == WS2812B_BACKEND_SPI
#include "led_strip_spi.h"
#else
#error "WS2812B_BACKEND must be WS2812B_BACKEND_RMT or WS2812B_BACKEND_SPI"
#endif

#define LOG_TAG "ws2812b"
#include "platform_log.h"

static led_strip_handle_t s_strip;

static esp_err_t ws2812b_require_ready(void)
{
    return s_strip != NULL ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t ws2812b_init(void)
{
    if (s_strip != NULL) {
        return ESP_OK;
    }

    const led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812B_GPIO_NUM,
        .max_leds = WS2812B_LED_COUNT,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    esp_err_t err;

#if WS2812B_BACKEND == WS2812B_BACKEND_RMT
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = WS2812B_RMT_RESOLUTION_HZ,
        .mem_block_symbols = 0,
        .flags.with_dma = false,
    };
    err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
#else
    const led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    err = led_strip_new_spi_device(&strip_config, &spi_config, &s_strip);
#endif

    if (err != ESP_OK) {
        s_strip = NULL;
        log_error("WS2812B init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = led_strip_clear(s_strip);
    if (err != ESP_OK) {
        led_strip_del(s_strip);
        s_strip = NULL;
        log_error("WS2812B clear after init failed: %s", esp_err_to_name(err));
        return err;
    }

#if WS2812B_BACKEND == WS2812B_BACKEND_RMT
    log_info("WS2812B initialized: backend=RMT, GPIO=%d, LEDs=%u", WS2812B_GPIO_NUM,
             (unsigned)WS2812B_LED_COUNT);
#else
    log_info("WS2812B initialized: backend=SPI, GPIO=%d, LEDs=%u", WS2812B_GPIO_NUM,
             (unsigned)WS2812B_LED_COUNT);
#endif
    return ESP_OK;
}

esp_err_t ws2812b_set_pixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err = ws2812b_require_ready();
    if (err != ESP_OK) {
        return err;
    }
    if (index >= WS2812B_LED_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    return led_strip_set_pixel(s_strip, index, red, green, blue);
}

esp_err_t ws2812b_set_all(uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t err = ws2812b_require_ready();
    if (err != ESP_OK) {
        return err;
    }

    for (uint32_t index = 0; index < WS2812B_LED_COUNT; ++index) {
        err = led_strip_set_pixel(s_strip, index, red, green, blue);
        if (err != ESP_OK) {
            return err;
        }
    }
    return led_strip_refresh(s_strip);
}

esp_err_t ws2812b_refresh(void)
{
    esp_err_t err = ws2812b_require_ready();
    return err == ESP_OK ? led_strip_refresh(s_strip) : err;
}

esp_err_t ws2812b_clear(void)
{
    esp_err_t err = ws2812b_require_ready();
    return err == ESP_OK ? led_strip_clear(s_strip) : err;
}

esp_err_t ws2812b_deinit(void)
{
    esp_err_t err = ws2812b_require_ready();
    if (err != ESP_OK) {
        return err;
    }

    err = led_strip_del(s_strip);
    if (err == ESP_OK) {
        s_strip = NULL;
    }
    return err;
}
