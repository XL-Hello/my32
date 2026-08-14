#include "glog.h"

#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LOG_TAG "glog_test"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "platform_log.h"

#define GLOG_TEST_PAYLOAD_SIZE 128U
#define GLOG_TEST_PERIOD_MS    100U

static TaskHandle_t s_glog_test_task;

static void glog_test_fill_random_printable(uint8_t *payload, size_t payload_size)
{
    esp_fill_random(payload, payload_size);

    for (size_t index = 0; index < payload_size; ++index) {
        payload[index] = (uint8_t)(33U + (payload[index] % 94U));
    }
}

static void glog_random_log_test_task(void *arg)
{
    uint8_t payload[GLOG_TEST_PAYLOAD_SIZE];

    (void)arg;
    while (true) {
        glog_test_fill_random_printable(payload, sizeof(payload));
        log_verbose("%.*s", (int)sizeof(payload), (const char *)payload);
        (void)glog_put(payload, sizeof(payload));
        vTaskDelay(pdMS_TO_TICKS(GLOG_TEST_PERIOD_MS));
    }
}

glog_status_t glog_test_start(void)
{
    if (s_glog_test_task != NULL) {
        return GLOG_OK;
    }

    return xTaskCreate(glog_random_log_test_task,
                       "glog_test",
                       3072,
                       NULL,
                       3,
                       &s_glog_test_task) == pdPASS
               ? GLOG_OK
               : GLOG_ERR;
}
