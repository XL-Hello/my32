#include "lcd.h"

#include <stdbool.h>
#include <stdlib.h>

#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define LOG_TAG "lcd"
#include "platform_log.h"

#define LCD_HOST SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_TEST_LINES 80
#define LCD_VERSION_TEST_PERIOD_MS 2000

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_panel_io;
static SemaphoreHandle_t s_color_trans_done;
static TaskHandle_t s_color_test_task;
static TaskHandle_t s_version_test_task;
static lcd_color_trans_done_cb_t s_color_trans_done_callback;
static void *s_color_trans_done_user_ctx;

static bool lcd_color_trans_done_callback(esp_lcd_panel_io_handle_t panel_io,
                                          esp_lcd_panel_io_event_data_t *event_data,
                                          void *user_ctx)
{
    (void)panel_io;
    (void)event_data;
    BaseType_t high_task_woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_ctx, &high_task_woken);

    bool callback_should_yield = false;
    if (s_color_trans_done_callback != NULL) {
        callback_should_yield = s_color_trans_done_callback(s_color_trans_done_user_ctx);
    }

    return high_task_woken == pdTRUE || callback_should_yield;
}

static uint16_t lcd_rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)(((red & 0xF8) << 8) |
                      ((green & 0xFC) << 3) |
                      ((blue & 0xF8) >> 3));
}

static void lcd_fill_buffer(uint8_t *buffer, size_t pixel_count, uint16_t color)
{
    const uint8_t high_byte = (uint8_t)(color >> 8);
    const uint8_t low_byte = (uint8_t)(color & 0xFF);

    for (size_t index = 0; index < pixel_count; ++index) {
        buffer[index * 2] = high_byte;
        buffer[index * 2 + 1] = low_byte;
    }
}

esp_err_t lcd_init(void)
{
    if (s_panel != NULL) {
        return ESP_OK;
    }

    const spi_bus_config_t bus_config = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = LCD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_TEST_LINES * sizeof(uint16_t),
    };
    esp_err_t err = spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        log_error("LCD SPI bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_color_trans_done = xSemaphoreCreateBinary();
    if (s_color_trans_done == NULL) {
        log_error("LCD color transaction semaphore creation failed");
        spi_bus_free(LCD_HOST);
        return ESP_ERR_NO_MEM;
    }

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = lcd_color_trans_done_callback,
        .user_ctx = s_color_trans_done,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                   &io_config, &s_panel_io);
    if (err != ESP_OK) {
        log_error("LCD panel IO init failed: %s", esp_err_to_name(err));
        s_panel_io = NULL;
        spi_bus_free(LCD_HOST);
        return err;
    }

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    err = esp_lcd_new_panel_ili9341(s_panel_io, &panel_config, &s_panel);
    if (err != ESP_OK) {
        log_error("ILI9341 panel create failed: %s", esp_err_to_name(err));
        s_panel = NULL;
        return err;
    }

    err = esp_lcd_panel_reset(s_panel);
    if (err == ESP_OK) {
        err = esp_lcd_panel_init(s_panel);
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_mirror(s_panel, false, true);
    }
    if (err == ESP_OK) {
        err = esp_lcd_panel_disp_on_off(s_panel, true);
    }
    if (err != ESP_OK) {
        log_error("ILI9341 panel init failed: %s", esp_err_to_name(err));
        s_panel = NULL;
        return err;
    }

    log_info("ILI9341 initialized: SPI=%d SCLK=%d MOSI=%d MISO=%d CS=%d DC=%d RST=%d",
             LCD_HOST, LCD_PIN_SCLK, LCD_PIN_MOSI, LCD_PIN_MISO,
             LCD_PIN_CS, LCD_PIN_DC, LCD_PIN_RST);
    return ESP_OK;
}

esp_err_t lcd_draw_bitmap(int x_start, int y_start, int x_end, int y_end,
                          const void *color_data)
{
    if (s_panel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_lcd_panel_draw_bitmap(s_panel, x_start, y_start, x_end, y_end,
                                     color_data);
}

esp_err_t lcd_set_color_trans_done_callback(lcd_color_trans_done_cb_t callback,
                                            void *user_ctx)
{
    if (s_panel_io == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_color_trans_done_callback = callback;
    s_color_trans_done_user_ctx = user_ctx;
    return ESP_OK;
}

static esp_err_t lcd_read_version(uint8_t identification[4],
                                  uint8_t id1[2], uint8_t id2[2], uint8_t id3[2])
{
    if (s_panel == NULL || s_panel_io == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_lcd_panel_io_rx_param(s_panel_io, 0x04,
                                              identification, sizeof(uint8_t[4]));
    if (err != ESP_OK) {
        return err;
    }

    err = esp_lcd_panel_io_rx_param(s_panel_io, 0xDA, id1, sizeof(uint8_t[2]));
    if (err != ESP_OK) {
        return err;
    }
    err = esp_lcd_panel_io_rx_param(s_panel_io, 0xDB, id2, sizeof(uint8_t[2]));
    if (err != ESP_OK) {
        return err;
    }
    return esp_lcd_panel_io_rx_param(s_panel_io, 0xDC, id3, sizeof(uint8_t[2]));
}

static void lcd_test_version_task(void *arg)
{
    (void)arg;

    while (true) {
        uint8_t identification[4] = {0};
        uint8_t id1[2] = {0};
        uint8_t id2[2] = {0};
        uint8_t id3[2] = {0};
        esp_err_t err = lcd_read_version(identification, id1, id2, id3);

        if (err != ESP_OK) {
            log_error("LCD version read failed: %s", esp_err_to_name(err));
        } else {
            log_info("LCD ID(04): %02X %02X %02X %02X, "
                     "ID(DA): %02X %02X, ID(DB): %02X %02X, ID(DC): %02X %02X",
                     identification[0], identification[1], identification[2], identification[3],
                     id1[0], id1[1], id2[0], id2[1], id3[0], id3[1]);
        }

        vTaskDelay(pdMS_TO_TICKS(LCD_VERSION_TEST_PERIOD_MS));
    }
}

esp_err_t lcd_test_version(void)
{
    if (s_panel == NULL || s_panel_io == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_version_test_task != NULL) {
        return ESP_OK;
    }

    BaseType_t result = xTaskCreate(lcd_test_version_task, "lcd_version_test",
                                    4096, NULL, 5, &s_version_test_task);
    if (result != pdPASS) {
        s_version_test_task = NULL;
        log_error("LCD version test task creation failed");
        return ESP_ERR_NO_MEM;
    }

    log_info("LCD version test task started");
    return ESP_OK;
}

esp_err_t lcd_fill(uint8_t red, uint8_t green, uint8_t blue)
{
    if (s_panel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const size_t pixel_count = LCD_H_RES * LCD_TEST_LINES;
    uint8_t *buffer = heap_caps_malloc(pixel_count * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buffer == NULL) {
        log_error("LCD color buffer allocation failed");
        return ESP_ERR_NO_MEM;
    }

    lcd_fill_buffer(buffer, pixel_count, lcd_rgb565(red, green, blue));
    esp_err_t err = ESP_OK;
    for (int y = 0; y < LCD_V_RES; y += LCD_TEST_LINES) {
        err = esp_lcd_panel_draw_bitmap(s_panel, 0, y, LCD_H_RES,
                                        y + LCD_TEST_LINES, buffer);
        if (err != ESP_OK) {
            break;
        }

        if (xSemaphoreTake(s_color_trans_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
            log_error("LCD color transaction timeout at y=%d", y);
            err = ESP_ERR_TIMEOUT;
            break;
        }
    }
    free(buffer);
    return err;
}

static void lcd_test_colors_task(void *arg)
{
    (void)arg;

    static const uint8_t colors[][3] = {
        {255, 0, 0},
        {0, 255, 0},
        {0, 0, 255},
        {255, 255, 255},
        {0, 0, 0},
    };

    while (true) {
        for (size_t index = 0; index < sizeof(colors) / sizeof(colors[0]); ++index) {
            esp_err_t err = lcd_fill(colors[index][0], colors[index][1], colors[index][2]);
            if (err != ESP_OK) {
                log_error("LCD color test failed at index=%u: %s", (unsigned)index,
                          esp_err_to_name(err));
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            log_info("LCD color test: R=%u G=%u B=%u", colors[index][0],
                     colors[index][1], colors[index][2]);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

esp_err_t lcd_test_colors(void)
{
    if (s_panel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_color_test_task != NULL) {
        return ESP_OK;
    }

    BaseType_t result = xTaskCreate(lcd_test_colors_task, "lcd_color_test",
                                    4096, NULL, 5, &s_color_test_task);
    if (result != pdPASS) {
        s_color_test_task = NULL;
        log_error("LCD color test task creation failed");
        return ESP_ERR_NO_MEM;
    }

    log_info("LCD color test task started");
    return ESP_OK;
}
