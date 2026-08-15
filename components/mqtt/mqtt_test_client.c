#include "mqtt_test_client.h"

#include <inttypes.h>
#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define LOG_TAG "mqtt_test"

#define MQTT_TEST_CONNECTED_BIT BIT0
#define MQTT_TEST_PC_TOPIC "esp32s3/test/pc"
#define MQTT_TEST_PUBLISH_PERIOD_MS 10000
#define MQTT_TEST_TASK_STACK_SIZE 4096
#define MQTT_TEST_TASK_PRIORITY 4

#define MQTT_ADDR "mqtt://192.168.254.99:1883"

static EventGroupHandle_t s_event_group;
static esp_mqtt_client_handle_t s_client;

static void mqtt_test_event_handler(void *handler_args, esp_event_base_t base,
                                    int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    const esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        xEventGroupSetBits(s_event_group, MQTT_TEST_CONNECTED_BIT);
        ESP_LOGI(LOG_TAG, "已连接 Broker: %s", MQTT_ADDR);
        // 订阅主题
        if (esp_mqtt_client_subscribe(s_client, MQTT_TEST_PC_TOPIC, 0) < 0) {
            ESP_LOGW(LOG_TAG, "订阅主题失败: %s", MQTT_TEST_PC_TOPIC);
        }
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(LOG_TAG, "已订阅主题: %s", MQTT_TEST_PC_TOPIC);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(LOG_TAG, "收到 MQTT 消息: topic=%.*s data=%.*s",
                 event->topic_len, event->topic, event->data_len, event->data);
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(s_event_group, MQTT_TEST_CONNECTED_BIT);
        ESP_LOGW(LOG_TAG, "与 Broker 断开，等待自动重连");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(LOG_TAG, "MQTT 连接错误，等待自动重连");
        break;
    default:
        break;
    }
}

static void mqtt_test_publish_task(void *arg)
{
    (void)arg;
    uint32_t sequence = 0;
    char payload[128];

    while (true) {
        xEventGroupWaitBits(s_event_group, MQTT_TEST_CONNECTED_BIT,
                            pdFALSE, pdTRUE, portMAX_DELAY);

        const float temperature_c = 25.0f + (float)(sequence % 20) / 10.0f;
        const float humidity_rh = 55.0f + (float)(sequence % 15) / 10.0f;
        const int payload_length = snprintf(payload, sizeof(payload),
                                            "{\"sequence\":%" PRIu32
                                            ",\"temperature_c\":%.1f,\"humidity_rh\":%.1f}",
                                            sequence, temperature_c, humidity_rh);
        if (payload_length < 0 || payload_length >= (int)sizeof(payload)) {
            ESP_LOGE(LOG_TAG, "构造 MQTT 上报内容失败");
        } else {
            const int message_id = esp_mqtt_client_publish(s_client,
                                                           CONFIG_MQTT_TEST_TOPIC,
                                                           payload, 0, 0, 0);
            if (message_id >= 0) {
                ESP_LOGI(LOG_TAG, "上报成功: topic=%s payload=%s",
                         CONFIG_MQTT_TEST_TOPIC, payload);
                ++sequence;
            } else {
                ESP_LOGW(LOG_TAG, "上报失败，等待下一周期");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(MQTT_TEST_PUBLISH_PERIOD_MS));
    }
}
 
esp_err_t mqtt_test_client_init(void)
{
    if (s_client != NULL) {
        return ESP_OK;
    }

    s_event_group = xEventGroupCreate();
    if (s_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = MQTT_ADDR,
        .credentials.client_id = CONFIG_MQTT_TEST_CLIENT_ID,//可以改为UUID，保证唯一性
    };
    s_client = esp_mqtt_client_init(&mqtt_config);
    if (s_client == NULL) {
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                                    mqtt_test_event_handler, NULL);
    if (err != ESP_OK) {
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
        return err;
    }

    if (xTaskCreate(mqtt_test_publish_task, "mqtt_publish",
                    MQTT_TEST_TASK_STACK_SIZE, NULL, MQTT_TEST_TASK_PRIORITY,
                    NULL) != pdPASS) {
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
        return ESP_ERR_NO_MEM;
    }

    err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "MQTT 客户端启动失败: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}
