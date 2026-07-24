#include "sim_uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "sim_uart";
static const char s_hex_chars[] = "0123456789ABCDEF";

#define SIM_UART_TEST_SEND_PERIOD_MS 1000
#define SIM_UART_TEST_RECV_PERIOD_MS 3000
#define SIM_UART_TEST_LOOP_PERIOD_MS 50

static const uint8_t s_test_message[] = "hello esp32\r\n";

static void sim_uart_test_task(void *param);
static void sim_uart_log_received_data(void);

void sim_uart_test_init(void)
{
    if (xTaskCreate(sim_uart_test_task, "sim_uart_test", 3072, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create test task");
    }
}

void sim_uart_test_task(void *param)
{
    (void)param;
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t next_send_time = last_wake_time;
    TickType_t next_recv_time = last_wake_time + pdMS_TO_TICKS(SIM_UART_TEST_RECV_PERIOD_MS);

    while (true) {
        TickType_t now = xTaskGetTickCount();

        if ((int32_t)(now - next_send_time) >= 0) {
            if (sim_uart_send(s_test_message, sizeof(s_test_message) - 1) != 0) {
                ESP_LOGE(TAG, "sim_uart_send failed");
            } else {
                ESP_LOGI(TAG, "tx: %s", (const char *)s_test_message);
            }
            next_send_time += pdMS_TO_TICKS(SIM_UART_TEST_SEND_PERIOD_MS);
        }

        if ((int32_t)(now - next_recv_time) >= 0) {
            sim_uart_log_received_data();
            next_recv_time += pdMS_TO_TICKS(SIM_UART_TEST_RECV_PERIOD_MS);
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SIM_UART_TEST_LOOP_PERIOD_MS));
    }
}

static void sim_uart_log_received_data(void)
{
    uint8_t recv_data[SIM_UART_RX_BUF_LEN];
    char raw_line[sizeof(recv_data) * 3];
    char char_line[sizeof(recv_data) + 1];

    int recv_len = sim_uart_recv(recv_data, sizeof(recv_data));
    if (recv_len < 0) {
        ESP_LOGE(TAG, "sim_uart_recv failed");
        return;
    }
    if (recv_len == 0) {
        ESP_LOGI(TAG, "rx buffer: empty");
        return;
    }

    size_t raw_offset = 0;
    for (int i = 0; i < recv_len; i++) {
        if (i > 0) {
            raw_line[raw_offset++] = ' ';
        }
        raw_line[raw_offset++] = s_hex_chars[recv_data[i] >> 4];
        raw_line[raw_offset++] = s_hex_chars[recv_data[i] & 0x0f];
        char_line[i] = recv_data[i] >= 0x20 && recv_data[i] <= 0x7e
                           ? (char)recv_data[i]
                           : '.';
    }
    raw_line[raw_offset] = '\0';
    char_line[recv_len] = '\0';

    ESP_LOGI(TAG, "rx raw : %s", raw_line);
    ESP_LOGI(TAG, "rx char: %s", char_line);
}
