#include "weather_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "miniz.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "wifi_manager.h"

#define LOG_TAG "weather"
#include "platform_log.h"

#define WEATHER_HTTP_TIMEOUT_MS 10000
#define WEATHER_HTTP_BUFFER_SIZE 2048
#define WEATHER_TASK_STACK_SIZE 12288
#define WEATHER_TASK_PRIORITY 4
#define WEATHER_WAIT_NETWORK_MS 5000
#define WEATHER_REFRESH_MS (CONFIG_WEATHER_REFRESH_MINUTES * 60 * 1000)
#define WEATHER_LOCATION_REFRESH_US (24LL * 60LL * 60LL * 1000000LL)
#define WEATHER_NVS_NAMESPACE "weather"
#define WEATHER_NVS_CITY "city"
#define WEATHER_NVS_LOCATION "location"

static SemaphoreHandle_t s_lock;
static weather_snapshot_t s_snapshot = {
    .icon_code = 999,
    .last_error = ESP_ERR_INVALID_STATE,
    .city_name = CONFIG_WEATHER_DEFAULT_CITY,
};
static bool s_initialized;
#if CONFIG_WEATHER_AUTO_LOCATION
static int64_t s_last_location_attempt_us;
#endif

static void weather_lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void weather_unlock(void)
{
    xSemaphoreGive(s_lock);
}

static bool weather_is_configured(void)
{
    return CONFIG_WEATHER_QWEATHER_HOST[0] != '\0' && CONFIG_WEATHER_QWEATHER_API_KEY[0] != '\0';
}

static esp_err_t weather_inflate_gzip(const uint8_t *gzip, size_t gzip_size,
                                      char *output, size_t output_size)
{
    if (gzip == NULL || gzip_size < 18 || output == NULL || output_size < 2 ||
        gzip[0] != 0x1f || gzip[1] != 0x8b || gzip[2] != 8 || (gzip[3] & 0xe0) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    size_t offset = 10;
    const uint8_t flags = gzip[3];
    if ((flags & 0x04) != 0) {
        if (offset + 2 > gzip_size - 8) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        const size_t extra_length = (size_t)gzip[offset] | ((size_t)gzip[offset + 1] << 8);
        offset += 2 + extra_length;
        if (offset > gzip_size - 8) {
            return ESP_ERR_INVALID_RESPONSE;
        }
    }
    for (uint8_t flag = 0x08; flag <= 0x10; flag <<= 1) {
        if ((flags & flag) == 0) {
            continue;
        }
        while (offset < gzip_size - 8 && gzip[offset] != '\0') {
            ++offset;
        }
        if (offset >= gzip_size - 8) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        ++offset;
    }
    if ((flags & 0x02) != 0) {
        offset += 2;
    }
    if (offset >= gzip_size - 8) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    tinfl_decompressor *decoder = calloc(1, sizeof(*decoder));
    if (decoder == NULL) {
        return ESP_ERR_NO_MEM;
    }
    const size_t compressed_size = gzip_size - offset - 8;
    size_t input_size = compressed_size;
    size_t decoded_size = output_size - 1;
    tinfl_init(decoder);
    const tinfl_status status = tinfl_decompress(
        decoder, gzip + offset, &input_size, (uint8_t *)output, (uint8_t *)output, &decoded_size,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(decoder);
    if (status != TINFL_STATUS_DONE || input_size != compressed_size || decoded_size == output_size - 1) {
        return ESP_ERR_INVALID_SIZE;
    }
    output[decoded_size] = '\0';
    return ESP_OK;
}

static esp_err_t weather_http_get(const char *url, char *response, size_t response_size,
                                  bool qweather_auth)
{
    if (url == NULL || response == NULL || response_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = WEATHER_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    char *raw_response = malloc(response_size);
    if (raw_response == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(raw_response);
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Accept-Encoding", "gzip");
    if (qweather_auth) {
        esp_http_client_set_header(client, "X-QW-Api-Key", CONFIG_WEATHER_QWEATHER_API_KEY);
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        const int header_length = esp_http_client_fetch_headers(client);
        const int status_code = esp_http_client_get_status_code(client);
        if (header_length < 0 || status_code != 200) {
            err = ESP_FAIL;
        }
    }

    size_t total = 0;
    while (err == ESP_OK && total < response_size - 1) {
        const int count = esp_http_client_read(client, raw_response + total,
                                               (int)(response_size - 1 - total));
        if (count < 0) {
            err = ESP_FAIL;
            break;
        }
        if (count == 0) {
            break;
        }
        total += (size_t)count;
    }
    raw_response[total] = '\0';
    if (err == ESP_OK && total == response_size - 1) {
        err = ESP_ERR_INVALID_SIZE;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (err == ESP_OK) {
        if (total >= 2 && (uint8_t)raw_response[0] == 0x1f && (uint8_t)raw_response[1] == 0x8b) {
            err = weather_inflate_gzip((const uint8_t *)raw_response, total, response, response_size);
        } else {
            memcpy(response, raw_response, total + 1);
        }
    }
    free(raw_response);
    return err;
}

static bool weather_json_get_string(const cJSON *object, const char *key, char *out, size_t out_size)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    snprintf(out, out_size, "%s", item->valuestring);
    return true;
}

#if CONFIG_WEATHER_AUTO_LOCATION
static bool weather_parse_coordinate(const cJSON *json, float *longitude, float *latitude)
{
    const cJSON *latitude_item = cJSON_GetObjectItemCaseSensitive(json, "latitude");
    const cJSON *longitude_item = cJSON_GetObjectItemCaseSensitive(json, "longitude");
    if (!cJSON_IsNumber(latitude_item) || !cJSON_IsNumber(longitude_item)) {
        latitude_item = cJSON_GetObjectItemCaseSensitive(json, "lat");
        longitude_item = cJSON_GetObjectItemCaseSensitive(json, "lon");
    }
    if (cJSON_IsNumber(latitude_item) && cJSON_IsNumber(longitude_item)) {
        *latitude = (float)latitude_item->valuedouble;
        *longitude = (float)longitude_item->valuedouble;
        return true;
    }

    char location[48];
    if (weather_json_get_string(json, "loc", location, sizeof(location)) &&
        sscanf(location, "%f,%f", latitude, longitude) == 2) {
        return true;
    }
    return false;
}

static esp_err_t weather_resolve_city(char *city_name, size_t city_size,
                                      char *location_id, size_t location_size)
{
    const char *services[] = {CONFIG_WEATHER_IP_GEO_URL, "https://ipapi.co/json/"};
    const size_t service_count = sizeof(services) / sizeof(services[0]);
    const size_t first = esp_random() % service_count;
    char *response = malloc(WEATHER_HTTP_BUFFER_SIZE);
    if (response == NULL) {
        return ESP_ERR_NO_MEM;
    }
    float longitude = 0.0f;
    float latitude = 0.0f;
    bool found = false;
    for (size_t attempt = 0; attempt < service_count; ++attempt) {
        const size_t index = (first + attempt) % service_count;
        if (weather_http_get(services[index], response, WEATHER_HTTP_BUFFER_SIZE, false) != ESP_OK) {
            continue;
        }
        cJSON *json = cJSON_Parse(response);
        if (json != NULL) {
            found = weather_parse_coordinate(json, &longitude, &latitude);
            cJSON_Delete(json);
        }
        if (found) {
            break;
        }
    }
    if (!found || longitude < -180.0f || longitude > 180.0f || latitude < -90.0f || latitude > 90.0f) {
        free(response);
        return ESP_ERR_NOT_FOUND;
    }

    char url[192];
    snprintf(url, sizeof(url), "https://%s/geo/v2/city/lookup?location=%.2f,%.2f&number=1&lang=en",
             CONFIG_WEATHER_QWEATHER_HOST, longitude, latitude);
    esp_err_t err = weather_http_get(url, response, WEATHER_HTTP_BUFFER_SIZE, true);
    if (err != ESP_OK) {
        free(response);
        return err;
    }
    cJSON *json = cJSON_Parse(response);
    const cJSON *locations = json == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(json, "location");
    const cJSON *first_location = cJSON_IsArray(locations) ? cJSON_GetArrayItem(locations, 0) : NULL;
    char code[8];
    if (json == NULL || !weather_json_get_string(json, "code", code, sizeof(code)) || strcmp(code, "200") != 0 ||
        first_location == NULL || !weather_json_get_string(first_location, "name", city_name, city_size) ||
        !weather_json_get_string(first_location, "id", location_id, location_size)) {
        cJSON_Delete(json);
        free(response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    cJSON_Delete(json);
    free(response);
    return ESP_OK;
}
#endif

static void weather_load_location(char *city_name, size_t city_size, char *location_id, size_t location_size)
{
    snprintf(city_name, city_size, "%s", CONFIG_WEATHER_DEFAULT_CITY);
    snprintf(location_id, location_size, "%s", CONFIG_WEATHER_DEFAULT_LOCATION_ID);
    nvs_handle_t handle;
    if (nvs_open(WEATHER_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        size_t city_length = city_size;
        size_t id_length = location_size;
        if (nvs_get_str(handle, WEATHER_NVS_CITY, city_name, &city_length) != ESP_OK ||
            nvs_get_str(handle, WEATHER_NVS_LOCATION, location_id, &id_length) != ESP_OK) {
            snprintf(city_name, city_size, "%s", CONFIG_WEATHER_DEFAULT_CITY);
            snprintf(location_id, location_size, "%s", CONFIG_WEATHER_DEFAULT_LOCATION_ID);
        }
        nvs_close(handle);
    }
}

#if CONFIG_WEATHER_AUTO_LOCATION
static void weather_save_location(const char *city_name, const char *location_id)
{
    nvs_handle_t handle;
    if (nvs_open(WEATHER_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        if (nvs_set_str(handle, WEATHER_NVS_CITY, city_name) == ESP_OK &&
            nvs_set_str(handle, WEATHER_NVS_LOCATION, location_id) == ESP_OK) {
            nvs_commit(handle);
        }
        nvs_close(handle);
    }
}
#endif

static esp_err_t weather_refresh(void)
{
    char city_name[sizeof(s_snapshot.city_name)];
    char location_id[16];
    weather_load_location(city_name, sizeof(city_name), location_id, sizeof(location_id));

#if CONFIG_WEATHER_AUTO_LOCATION
    char resolved_city[sizeof(city_name)];
    char resolved_id[sizeof(location_id)];
    const int64_t now_us = esp_timer_get_time();
    if (s_last_location_attempt_us == 0 ||
        now_us - s_last_location_attempt_us >= WEATHER_LOCATION_REFRESH_US) {
        s_last_location_attempt_us = now_us;
        if (weather_resolve_city(resolved_city, sizeof(resolved_city), resolved_id, sizeof(resolved_id)) == ESP_OK) {
            snprintf(city_name, sizeof(city_name), "%s", resolved_city);
            snprintf(location_id, sizeof(location_id), "%s", resolved_id);
            weather_save_location(city_name, location_id);
        }
    }
#endif

    char url[176];
    char *response = malloc(WEATHER_HTTP_BUFFER_SIZE);
    if (response == NULL) {
        return ESP_ERR_NO_MEM;
    }
    snprintf(url, sizeof(url), "https://%s/v7/weather/now?location=%s&lang=zh",
             CONFIG_WEATHER_QWEATHER_HOST, location_id);
    esp_err_t err = weather_http_get(url, response, WEATHER_HTTP_BUFFER_SIZE, true);
    if (err != ESP_OK) {
        free(response);
        return err;
    }

    cJSON *json = cJSON_Parse(response);
    const cJSON *now = json == NULL ? NULL : cJSON_GetObjectItemCaseSensitive(json, "now");
    char code[8];
    char temperature[8];
    char icon[8];
    char observed_at[32];
    if (json == NULL || !weather_json_get_string(json, "code", code, sizeof(code)) || strcmp(code, "200") != 0 ||
        now == NULL || !weather_json_get_string(now, "temp", temperature, sizeof(temperature)) ||
        !weather_json_get_string(now, "icon", icon, sizeof(icon)) ||
        !weather_json_get_string(now, "obsTime", observed_at, sizeof(observed_at))) {
        cJSON_Delete(json);
        free(response);
        return ESP_ERR_INVALID_RESPONSE;
    }
    const long temp = strtol(temperature, NULL, 10);
    const long icon_code = strtol(icon, NULL, 10);
    cJSON_Delete(json);
    free(response);
    if (temp < -99 || temp > 99 || icon_code < 0 || icon_code > UINT16_MAX) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    weather_lock();
    s_snapshot.valid = true;
    s_snapshot.temperature_c = (int8_t)temp;
    s_snapshot.icon_code = (uint16_t)icon_code;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.updated_at_us = esp_timer_get_time();
    snprintf(s_snapshot.city_name, sizeof(s_snapshot.city_name), "%s", city_name);
    snprintf(s_snapshot.observed_at, sizeof(s_snapshot.observed_at), "%s", observed_at);
    weather_unlock();
    log_info("refresh succeeded: location=%s temp=%ld icon=%ld", location_id, temp, icon_code);
    return ESP_OK;
}

static void weather_task(void *arg)
{
    (void)arg;
    bool configuration_logged = false;
    while (true) {
        wifi_manager_network_info_t network;
        if (!weather_is_configured()) {
            if (!configuration_logged) {
                log_warn("service waiting for QWeather Host and API Key configuration");
                configuration_logged = true;
            }
            vTaskDelay(pdMS_TO_TICKS(WEATHER_WAIT_NETWORK_MS));
            continue;
        }
        if (wifi_manager_get_network_info(&network) != ESP_OK ||
            network.status != WIFI_MANAGER_STATUS_INTERNET_READY) {
            vTaskDelay(pdMS_TO_TICKS(WEATHER_WAIT_NETWORK_MS));
            continue;
        }
        const esp_err_t err = weather_refresh();
        if (err != ESP_OK) {
            weather_lock();
            s_snapshot.last_error = err;
            weather_unlock();
            log_warn("refresh failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(5 * 60 * 1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(WEATHER_REFRESH_MS));
        }
    }
}

esp_err_t weather_service_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(weather_task, "weather", WEATHER_TASK_STACK_SIZE, NULL,
                    WEATHER_TASK_PRIORITY, NULL) != pdPASS) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_initialized = true;
    return ESP_OK;
}

esp_err_t weather_service_get_snapshot(weather_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    weather_lock();
    *snapshot = s_snapshot;
    weather_unlock();
    return ESP_OK;
}
