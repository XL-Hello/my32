#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "system_time.h"

#define LOG_TAG "wifi_manager"
#include "platform_log.h"

#define WIFI_MANAGER_NVS_NAMESPACE "wifi"
#define WIFI_MANAGER_NVS_KEY_ENABLED "enabled"
#define WIFI_MANAGER_NVS_KEY_SSID "ssid"
#define WIFI_MANAGER_NVS_KEY_PASSWORD "password"
#define WIFI_MANAGER_MAX_RETRY_COUNT 5
#define WIFI_MANAGER_CONNECTIVITY_URL "https://www.baidu.com/"
#define WIFI_MANAGER_CONNECTIVITY_TIMEOUT_MS 4000

static SemaphoreHandle_t s_lock;
static esp_netif_t *s_station_netif;
static wifi_manager_network_info_t s_network_info;
static wifi_manager_ap_info_t s_scan_results[WIFI_MANAGER_MAX_SCAN_RESULTS];
/* 扫描完成回调运行在 2304 B 的系统事件任务中，不能在回调栈上分配此数组。 */
static wifi_ap_record_t s_scan_records[WIFI_MANAGER_MAX_SCAN_RESULTS];
static size_t s_scan_result_count;
static wifi_config_t s_pending_config;
static bool s_initialized;
static bool s_scanning;
static bool s_auto_reconnect;
static bool s_save_pending_config;
static uint8_t s_retry_count;

static void wifi_manager_copy_field_to_string(char *destination, size_t destination_size,
                                              const uint8_t *source, size_t source_size)
{
    const size_t length = strnlen((const char *)source, source_size);
    const size_t copy_length = length < destination_size - 1 ? length : destination_size - 1;
    memcpy(destination, source, copy_length);
    destination[copy_length] = '\0';
}

static void wifi_manager_lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void wifi_manager_unlock(void)
{
    xSemaphoreGive(s_lock);
}

static esp_err_t wifi_manager_save_enabled(bool enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, WIFI_MANAGER_NVS_KEY_ENABLED, enabled ? 1U : 0U);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t wifi_manager_save_config(const wifi_config_t *config)
{
    char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
    char password[WIFI_MANAGER_PASSWORD_MAX_LEN + 1];
    wifi_manager_copy_field_to_string(ssid, sizeof(ssid), config->sta.ssid,
                                      sizeof(config->sta.ssid));
    wifi_manager_copy_field_to_string(password, sizeof(password), config->sta.password,
                                      sizeof(config->sta.password));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, WIFI_MANAGER_NVS_KEY_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, WIFI_MANAGER_NVS_KEY_PASSWORD, password);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, WIFI_MANAGER_NVS_KEY_ENABLED, 1U);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t wifi_manager_load_config(wifi_config_t *config, bool *enabled)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *enabled = true;
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK) {
        return err;
    }

    uint8_t enabled_value = 1;
    err = nvs_get_u8(handle, WIFI_MANAGER_NVS_KEY_ENABLED, &enabled_value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    *enabled = enabled_value != 0;

    char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1] = {0};
    char password[WIFI_MANAGER_PASSWORD_MAX_LEN + 1] = {0};
    size_t ssid_length = sizeof(ssid);
    if (err == ESP_OK) {
        err = nvs_get_str(handle, WIFI_MANAGER_NVS_KEY_SSID, ssid, &ssid_length);
    }
    if (err == ESP_OK) {
        size_t password_length = sizeof(password);
        err = nvs_get_str(handle, WIFI_MANAGER_NVS_KEY_PASSWORD, password, &password_length);
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        memset(config, 0, sizeof(*config));
        memcpy(config->sta.ssid, ssid, strnlen(ssid, sizeof(ssid)));
        memcpy(config->sta.password, password, strnlen(password, sizeof(password)));
    }
    return err;
}

static bool wifi_manager_is_auth_failure(uint8_t reason)
{
    return reason == WIFI_REASON_AUTH_EXPIRE || reason == WIFI_REASON_AUTH_FAIL ||
           reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
           reason == WIFI_REASON_HANDSHAKE_TIMEOUT;
}

static void wifi_manager_run_connectivity_check(void *arg)
{
    (void)arg;
    esp_http_client_config_t config = {
        .url = WIFI_MANAGER_CONNECTIVITY_URL,
        .timeout_ms = WIFI_MANAGER_CONNECTIVITY_TIMEOUT_MS,
        .disable_auto_redirect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    bool internet_available = false;
    if (client != NULL) {
        const esp_err_t err = esp_http_client_perform(client);
        const int status_code = esp_http_client_get_status_code(client);
        internet_available = err == ESP_OK && status_code >= 200 && status_code < 400;
        if (!internet_available) {
            log_warn("HTTPS connectivity check failed: url=%s err=%s status=%d",
                     WIFI_MANAGER_CONNECTIVITY_URL, esp_err_to_name(err), status_code);
        }
        esp_http_client_cleanup(client);
    } else {
        log_warn("HTTPS connectivity check client initialization failed");
    }

    bool should_sync_time = false;
    wifi_manager_lock();
    if (s_network_info.status == WIFI_MANAGER_STATUS_CHECKING_INTERNET) {
        s_network_info.internet_available = internet_available;
        s_network_info.status = internet_available ? WIFI_MANAGER_STATUS_INTERNET_READY :
                                                    WIFI_MANAGER_STATUS_INTERNET_UNREACHABLE;
        should_sync_time = internet_available;
    }
    wifi_manager_unlock();

    if (should_sync_time) {
        const esp_err_t sync_err = system_time_sntp_start();
        if (sync_err != ESP_OK) {
            log_error("failed to start SNTP synchronization: %s", esp_err_to_name(sync_err));
        }
    }
    vTaskDelete(NULL);
}

static void wifi_manager_start_connectivity_check(void)
{
    BaseType_t task_created = xTaskCreate(wifi_manager_run_connectivity_check, "wifi_check",
                                          4096, NULL, 4, NULL);
    if (task_created != pdPASS) {
        wifi_manager_lock();
        s_network_info.status = WIFI_MANAGER_STATUS_LOCAL_NETWORK_READY;
        wifi_manager_unlock();
        log_error("failed to create connectivity check task");
    }
}

static void wifi_manager_handle_scan_done(void)
{
    uint16_t record_count = WIFI_MANAGER_MAX_SCAN_RESULTS;
    const esp_err_t err = esp_wifi_scan_get_ap_records(&record_count, s_scan_records);
    const uint16_t copied_record_count = record_count < WIFI_MANAGER_MAX_SCAN_RESULTS ?
                                             record_count : WIFI_MANAGER_MAX_SCAN_RESULTS;

    wifi_manager_lock();
    s_scan_result_count = 0;
    if (err == ESP_OK) {
        for (uint16_t index = 0; index < copied_record_count; ++index) {
            wifi_manager_ap_info_t *result = &s_scan_results[s_scan_result_count++];
            wifi_manager_copy_field_to_string(result->ssid, sizeof(result->ssid),
                                              s_scan_records[index].ssid,
                                              sizeof(s_scan_records[index].ssid));
            result->rssi = s_scan_records[index].rssi;
            result->authmode = (uint8_t)s_scan_records[index].authmode;
        }
    }
    s_scanning = false;
    if (s_network_info.status == WIFI_MANAGER_STATUS_SCANNING) {
        s_network_info.status = s_network_info.ipv4[0] != '\0' ?
                                    WIFI_MANAGER_STATUS_LOCAL_NETWORK_READY :
                                    WIFI_MANAGER_STATUS_DISCONNECTED;
    }
    wifi_manager_unlock();

    if (err != ESP_OK) {
        log_error("Wi-Fi scan failed: %s", esp_err_to_name(err));
    }
}

static void wifi_manager_event_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        wifi_manager_handle_scan_done();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_manager_lock();
        s_retry_count = 0;
        s_network_info.status = WIFI_MANAGER_STATUS_CONNECTED_NO_IP;
        wifi_manager_unlock();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        wifi_manager_lock();
        esp_ip4addr_ntoa(&event->ip_info.ip, s_network_info.ipv4,
                         sizeof(s_network_info.ipv4));
        s_network_info.status = WIFI_MANAGER_STATUS_CHECKING_INTERNET;
        s_network_info.internet_available = false;
        const bool save_config = s_save_pending_config;
        const wifi_config_t config_to_save = s_pending_config;
        s_save_pending_config = false;
        wifi_manager_unlock();

        if (save_config) {
            const esp_err_t err = wifi_manager_save_config(&config_to_save);
            if (err != ESP_OK) {
                log_error("failed to save Wi-Fi credentials: %s", esp_err_to_name(err));
            }
        }
        wifi_manager_start_connectivity_check();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = event_data;
        bool reconnect = false;
        wifi_manager_lock();
        s_network_info.ipv4[0] = '\0';
        s_network_info.rssi = 0;
        s_network_info.internet_available = false;
        if (!s_network_info.enabled) {
            s_network_info.status = WIFI_MANAGER_STATUS_DISABLED;
        } else if (!s_auto_reconnect && s_network_info.ssid[0] == '\0') {
            s_network_info.status = WIFI_MANAGER_STATUS_UNCONFIGURED;
        } else if (s_auto_reconnect && s_retry_count < WIFI_MANAGER_MAX_RETRY_COUNT) {
            ++s_retry_count;
            s_network_info.status = WIFI_MANAGER_STATUS_CONNECTING;
            reconnect = true;
        } else if (wifi_manager_is_auth_failure(event->reason)) {
            s_network_info.status = WIFI_MANAGER_STATUS_AUTH_FAILED;
        } else if (event->reason == WIFI_REASON_NO_AP_FOUND) {
            s_network_info.status = WIFI_MANAGER_STATUS_NO_AP_FOUND;
        } else {
            s_network_info.status = WIFI_MANAGER_STATUS_DISCONNECTED;
        }
        wifi_manager_unlock();

        if (reconnect) {
            const esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                log_error("Wi-Fi reconnect failed: %s", esp_err_to_name(err));
            }
        }
    }
}

static esp_err_t wifi_manager_init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    return err;
}

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = wifi_manager_init_nvs();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_station_netif = esp_netif_create_default_wifi_sta();
    if (s_station_netif == NULL) {
        return ESP_FAIL;
    }

    const wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        return err;
    }
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                wifi_manager_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                wifi_manager_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_network_info.enabled = true;
    s_network_info.status = WIFI_MANAGER_STATUS_UNCONFIGURED;
    s_initialized = true;

    wifi_config_t saved_config = {0};
    bool enabled = true;
    err = wifi_manager_load_config(&saved_config, &enabled);
    if (err == ESP_OK && enabled) {
        char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
        char password[WIFI_MANAGER_PASSWORD_MAX_LEN + 1];
        wifi_manager_copy_field_to_string(ssid, sizeof(ssid), saved_config.sta.ssid,
                                          sizeof(saved_config.sta.ssid));
        wifi_manager_copy_field_to_string(password, sizeof(password), saved_config.sta.password,
                                          sizeof(saved_config.sta.password));
        return wifi_manager_connect(ssid, password);
    }
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        log_error("failed to load Wi-Fi configuration: %s", esp_err_to_name(err));
    }
    if (!enabled) {
        return wifi_manager_set_enabled(false);
    }
    return ESP_OK;
}

esp_err_t wifi_manager_set_enabled(bool enabled)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t save_err = wifi_manager_save_enabled(enabled);
    if (save_err != ESP_OK) {
        return save_err;
    }

    wifi_config_t saved_config = {0};
    bool saved_enabled = true;
    if (enabled) {
        const esp_err_t load_err = wifi_manager_load_config(&saved_config, &saved_enabled);
        if (load_err != ESP_OK && load_err != ESP_ERR_NOT_FOUND) {
            return load_err;
        }
        if (load_err == ESP_OK) {
            const esp_err_t config_err = esp_wifi_set_config(WIFI_IF_STA, &saved_config);
            if (config_err != ESP_OK) {
                return config_err;
            }
        }
    }

    wifi_manager_lock();
    s_network_info.enabled = enabled;
    s_auto_reconnect = enabled && saved_enabled && saved_config.sta.ssid[0] != '\0';
    s_retry_count = 0;
    if (enabled && saved_config.sta.ssid[0] != '\0') {
        char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
        wifi_manager_copy_field_to_string(ssid, sizeof(ssid), saved_config.sta.ssid,
                                          sizeof(saved_config.sta.ssid));
        snprintf(s_network_info.ssid, sizeof(s_network_info.ssid), "%s",
                 ssid);
    }
    s_network_info.status = enabled ? (s_auto_reconnect ? WIFI_MANAGER_STATUS_CONNECTING :
                                                         WIFI_MANAGER_STATUS_UNCONFIGURED) :
                                      WIFI_MANAGER_STATUS_DISABLED;
    wifi_manager_unlock();

    if (!enabled) {
        return esp_wifi_stop();
    }
    const esp_err_t start_err = esp_wifi_start();
    if (start_err != ESP_OK) {
        return start_err;
    }
    return s_auto_reconnect ? esp_wifi_connect() : ESP_OK;
}

esp_err_t wifi_manager_start_scan(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_manager_lock();
    if (!s_network_info.enabled || s_scanning) {
        wifi_manager_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    s_scanning = true;
    s_scan_result_count = 0;
    s_network_info.status = WIFI_MANAGER_STATUS_SCANNING;
    wifi_manager_unlock();

    const esp_err_t err = esp_wifi_scan_start(NULL, false);
    if (err != ESP_OK) {
        wifi_manager_lock();
        s_scanning = false;
        s_network_info.status = WIFI_MANAGER_STATUS_DISCONNECTED;
        wifi_manager_unlock();
    }
    return err;
}

esp_err_t wifi_manager_get_scan_results(wifi_manager_ap_info_t *results,
                                        size_t results_capacity, size_t *result_count)
{
    if (!s_initialized || result_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_manager_lock();
    *result_count = s_scan_result_count;
    if (results != NULL) {
        const size_t copy_count = results_capacity < s_scan_result_count ?
                                      results_capacity : s_scan_result_count;
        memcpy(results, s_scan_results, copy_count * sizeof(*results));
    } else if (results_capacity != 0) {
        wifi_manager_unlock();
        return ESP_ERR_INVALID_ARG;
    }
    wifi_manager_unlock();
    return ESP_OK;
}

bool wifi_manager_is_scanning(void)
{
    if (!s_initialized) {
        return false;
    }
    wifi_manager_lock();
    const bool scanning = s_scanning;
    wifi_manager_unlock();
    return scanning;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    if (!s_initialized || ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t ssid_length = strnlen(ssid, WIFI_MANAGER_SSID_MAX_LEN + 1);
    const size_t password_length = strnlen(password, WIFI_MANAGER_PASSWORD_MAX_LEN + 1);
    if (ssid_length == 0 || ssid_length > WIFI_MANAGER_SSID_MAX_LEN ||
        password_length > WIFI_MANAGER_PASSWORD_MAX_LEN ||
        (password_length > 0 && password_length < 8)) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t config = {0};
    memcpy(config.sta.ssid, ssid, ssid_length);
    memcpy(config.sta.password, password, password_length);
    config.sta.threshold.authmode = password_length == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;

    const esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (err != ESP_OK) {
        return err;
    }
    wifi_manager_lock();
    s_pending_config = config;
    s_save_pending_config = true;
    s_auto_reconnect = true;
    s_retry_count = 0;
    s_network_info.enabled = true;
    snprintf(s_network_info.ssid, sizeof(s_network_info.ssid), "%s", ssid);
    s_network_info.status = WIFI_MANAGER_STATUS_CONNECTING;
    wifi_manager_unlock();

    return esp_wifi_connect();
}

esp_err_t wifi_manager_disconnect(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    wifi_manager_lock();
    s_auto_reconnect = false;
    s_retry_count = 0;
    s_network_info.status = WIFI_MANAGER_STATUS_DISCONNECTED;
    wifi_manager_unlock();
    const esp_err_t disconnect_err = esp_wifi_disconnect();
    return disconnect_err == ESP_ERR_WIFI_NOT_CONNECT ? ESP_OK : disconnect_err;
}

esp_err_t wifi_manager_forget_network(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_key(handle, WIFI_MANAGER_NVS_KEY_SSID);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_erase_key(handle, WIFI_MANAGER_NVS_KEY_PASSWORD);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        return err;
    }

    wifi_manager_lock();
    s_auto_reconnect = false;
    s_save_pending_config = false;
    memset(&s_pending_config, 0, sizeof(s_pending_config));
    memset(s_network_info.ssid, 0, sizeof(s_network_info.ssid));
    s_network_info.status = WIFI_MANAGER_STATUS_UNCONFIGURED;
    wifi_manager_unlock();
    return esp_wifi_disconnect();
}

esp_err_t wifi_manager_get_network_info(wifi_manager_network_info_t *info)
{
    if (!s_initialized || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    wifi_manager_lock();
    *info = s_network_info;
    wifi_manager_unlock();

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        info->rssi = ap_info.rssi;
    }
    return ESP_OK;
}

const char *wifi_manager_status_to_text(wifi_manager_status_t status)
{
    switch (status) {
    case WIFI_MANAGER_STATUS_DISABLED: return "Wi-Fi 已关闭";
    case WIFI_MANAGER_STATUS_UNCONFIGURED: return "未配置网络";
    case WIFI_MANAGER_STATUS_DISCONNECTED: return "连接已断开";
    case WIFI_MANAGER_STATUS_SCANNING: return "正在扫描热点";
    case WIFI_MANAGER_STATUS_CONNECTING: return "正在连接";
    case WIFI_MANAGER_STATUS_AUTH_FAILED: return "密码或认证失败";
    case WIFI_MANAGER_STATUS_NO_AP_FOUND: return "未找到热点";
    case WIFI_MANAGER_STATUS_CONNECTED_NO_IP: return "已连接，正在获取 IP";
    case WIFI_MANAGER_STATUS_CHECKING_INTERNET: return "正在验证网络";
    case WIFI_MANAGER_STATUS_LOCAL_NETWORK_READY: return "本地网络可用";
    case WIFI_MANAGER_STATUS_INTERNET_READY: return "互联网可用";
    case WIFI_MANAGER_STATUS_INTERNET_UNREACHABLE: return "已连接但无互联网";
    default: return "未知状态";
    }
}
