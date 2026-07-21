#include "environment_sensor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "aht20.h"

#define LOG_TAG "env_sensor"
#include "platform_log.h"

#define ENVIRONMENT_SENSOR_PERIOD_MS 10000
#define ENVIRONMENT_SENSOR_TASK_STACK_SIZE 3072
#define ENVIRONMENT_SENSOR_TASK_PRIORITY 4

static SemaphoreHandle_t s_data_mutex;
static environment_sensor_data_t s_latest_data = {
    .last_error = ESP_ERR_INVALID_STATE,
};

static void environment_sensor_store(float temperature_c, float humidity_rh,
                                     bool valid, esp_err_t error)
{
    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    if (valid) {
        s_latest_data.temperature_c = temperature_c;
        s_latest_data.humidity_rh = humidity_rh;
        s_latest_data.valid = true;
    }
    s_latest_data.last_error = error;
    xSemaphoreGive(s_data_mutex);
}

static void environment_sensor_task(void *arg)
{
    (void)arg;
    esp_err_t sensor_error = ESP_ERR_INVALID_STATE;

    while (true) {
        if (sensor_error != ESP_OK) {
            sensor_error = aht20_init();
        }

        if (sensor_error == ESP_OK) {
            float temperature_c;
            float humidity_rh;
            sensor_error = aht20_read(&temperature_c, &humidity_rh);
            if (sensor_error == ESP_OK) {
                environment_sensor_store(temperature_c, humidity_rh, true, ESP_OK);
            } else {
                environment_sensor_store(0.0f, 0.0f, false, sensor_error);
            }
        } else {
            environment_sensor_store(0.0f, 0.0f, false, sensor_error);
        }

        if (sensor_error != ESP_OK) {
            log_warn("AHT20 sampling failed: %s", esp_err_to_name(sensor_error));
        }
        vTaskDelay(pdMS_TO_TICKS(ENVIRONMENT_SENSOR_PERIOD_MS));
    }
}

esp_err_t environment_sensor_init(void)
{
    if (s_data_mutex != NULL) {
        return ESP_OK;
    }

    s_data_mutex = xSemaphoreCreateMutex();
    if (s_data_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_created = xTaskCreate(environment_sensor_task, "env_sensor",
                                          ENVIRONMENT_SENSOR_TASK_STACK_SIZE, NULL,
                                          ENVIRONMENT_SENSOR_TASK_PRIORITY, NULL);
    if (task_created != pdPASS) {
        vSemaphoreDelete(s_data_mutex);
        s_data_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t environment_sensor_get_latest(environment_sensor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_data_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    *data = s_latest_data;
    xSemaphoreGive(s_data_mutex);
    return ESP_OK;
}
