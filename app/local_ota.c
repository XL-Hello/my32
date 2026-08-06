#include "local_ota.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define LOG_TAG "local_ota"
#include "platform_log.h"

#define LOCAL_OTA_BUFFER_SIZE 4096U
#define LOCAL_OTA_YIELD_BYTES (16U * 1024U)
#define LOCAL_OTA_TASK_STACK_SIZE (12U * 1024U)
#define LOCAL_OTA_TASK_PRIORITY 5U

typedef struct { char file_name[128]; size_t size; } package_t;
typedef enum { OTA_ACTION_CHECK, OTA_ACTION_DOWNLOAD, OTA_ACTION_INSTALL } ota_action_t;

static local_ota_status_t s_status = {.state = LOCAL_OTA_STATE_NO_UPDATE};
static package_t s_package;
static const esp_partition_t *s_downloaded_partition;
static volatile bool s_task_running;

static void set_status(local_ota_state_t state, const char *detail)
{
    s_status.state = state;
    if (detail != NULL) {
        snprintf(s_status.detail, sizeof(s_status.detail), "%s", detail);
    }
}

static void set_failed(esp_err_t result)
{
    char detail[sizeof(s_status.detail)];
    snprintf(detail, sizeof(detail), "%s", esp_err_to_name(result));
    set_status(LOCAL_OTA_STATE_FAILED, detail);
    log_error("HTTP OTA failed: %s", detail);
}

static esp_err_t check_package_url(package_t *package)
{
    esp_http_client_config_t config = {
        .url = CONFIG_LOCAL_OTA_HTTP_PACKAGE_URL,
        .timeout_ms = CONFIG_LOCAL_OTA_HTTP_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = esp_http_client_open(client, 0);
    const int content_length = result == ESP_OK ? esp_http_client_fetch_headers(client) : -1;
    const int status_code = result == ESP_OK ? esp_http_client_get_status_code(client) : 0;
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (result != ESP_OK) return result;
    if (status_code == 404) return ESP_ERR_NOT_FOUND;
    if (status_code != 200 || content_length <= 0) return ESP_ERR_INVALID_RESPONSE;
    package->size = (size_t)content_length;
    const char *last_slash = strrchr(CONFIG_LOCAL_OTA_HTTP_PACKAGE_URL, '/');
    snprintf(package->file_name, sizeof(package->file_name), "%s",
             last_slash == NULL ? "hello_world.bin" : last_slash + 1);
    return ESP_OK;
}

static esp_err_t download_to_candidate(const package_t *package)
{
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (target == NULL || package->size > target->size) return ESP_ERR_INVALID_SIZE;
    esp_http_client_config_t config = {.url = CONFIG_LOCAL_OTA_HTTP_PACKAGE_URL, .timeout_ms = CONFIG_LOCAL_OTA_HTTP_TIMEOUT_MS};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = esp_http_client_open(client, 0);
    if (result != ESP_OK || esp_http_client_fetch_headers(client) < 0 || esp_http_client_get_status_code(client) != 200 || esp_http_client_get_content_length(client) != (int)package->size) { esp_http_client_cleanup(client); return result == ESP_OK ? ESP_FAIL : result; }
    uint8_t *buffer = malloc(LOCAL_OTA_BUFFER_SIZE);
    esp_ota_handle_t handle = 0;
    bool begun = false;
    if (buffer == NULL) { esp_http_client_close(client); esp_http_client_cleanup(client); return ESP_ERR_NO_MEM; }
    if ((result = esp_ota_begin(target, package->size, &handle)) != ESP_OK) goto cleanup;
    begun = true;
    size_t downloaded = 0, yielded = 0;
    while (downloaded < package->size) {
        int bytes = esp_http_client_read(client, (char *)buffer, (int)((package->size - downloaded) < LOCAL_OTA_BUFFER_SIZE ? (package->size - downloaded) : LOCAL_OTA_BUFFER_SIZE));
        if (bytes <= 0) { result = ESP_ERR_INVALID_SIZE; break; }
        if ((result = esp_ota_write(handle, buffer, bytes)) != ESP_OK) break;
        downloaded += (size_t)bytes;
        s_status.received_bytes = downloaded;
        yielded += (size_t)bytes;
        if (yielded >= LOCAL_OTA_YIELD_BYTES) { yielded = 0; vTaskDelay(1); }
    }
    if (result == ESP_OK && downloaded != package->size) result = ESP_ERR_INVALID_SIZE;
    if (result == ESP_OK) { result = esp_ota_end(handle); begun = false; }
    if (result == ESP_OK) { esp_image_metadata_t metadata; esp_partition_pos_t position = {.offset = target->address, .size = target->size}; result = esp_image_verify(ESP_IMAGE_VERIFY, &position, &metadata); }
    if (result == ESP_OK) s_downloaded_partition = target;
cleanup:
    if (begun) esp_ota_abort(handle);
    free(buffer); esp_http_client_close(client); esp_http_client_cleanup(client);
    return result;
}

static void ota_task(void *parameter)
{
    ota_action_t action = (ota_action_t)(intptr_t)parameter;
    esp_err_t result = ESP_OK;
    if (action == OTA_ACTION_CHECK) {
        result = check_package_url(&s_package);
        if (result == ESP_OK) { s_status.total_bytes = s_package.size; s_status.received_bytes = 0; snprintf(s_status.file_name, sizeof(s_status.file_name), "%s", s_package.file_name); set_status(LOCAL_OTA_STATE_UPDATE_AVAILABLE, "轻触下载"); }
        else if (result == ESP_ERR_NOT_FOUND) { s_status.file_name[0] = '\0'; s_status.total_bytes = 0; s_status.received_bytes = 0; set_status(LOCAL_OTA_STATE_NO_UPDATE, "无更新 · 轻触检查"); result = ESP_OK; }
    } else if (action == OTA_ACTION_DOWNLOAD) {
        result = download_to_candidate(&s_package);
        if (result == ESP_OK) set_status(LOCAL_OTA_STATE_READY_TO_INSTALL, "轻触安装");
    } else {
        if (s_downloaded_partition == NULL) result = ESP_ERR_INVALID_STATE;
        else result = esp_ota_set_boot_partition(s_downloaded_partition);
        if (result == ESP_OK) esp_restart();
    }
    if (result != ESP_OK) set_failed(result);
    s_task_running = false;
    vTaskDelete(NULL);
}

static bool start_action(ota_action_t action, local_ota_state_t pending)
{
    if (s_task_running) return false;
    s_task_running = true;
    set_status(pending, pending == LOCAL_OTA_STATE_CHECKING ? "正在检查" : pending == LOCAL_OTA_STATE_DOWNLOADING ? "正在下载" : "请勿断电");
    if (xTaskCreate(ota_task, "local_ota", LOCAL_OTA_TASK_STACK_SIZE, (void *)(intptr_t)action, LOCAL_OTA_TASK_PRIORITY, NULL) != pdPASS) { s_task_running = false; set_failed(ESP_ERR_NO_MEM); return false; }
    return true;
}

bool local_ota_check_async(void) { return start_action(OTA_ACTION_CHECK, LOCAL_OTA_STATE_CHECKING); }
bool local_ota_download_async(void) { return s_status.state == LOCAL_OTA_STATE_UPDATE_AVAILABLE && start_action(OTA_ACTION_DOWNLOAD, LOCAL_OTA_STATE_DOWNLOADING); }
bool local_ota_install_async(void) { return s_status.state == LOCAL_OTA_STATE_READY_TO_INSTALL && start_action(OTA_ACTION_INSTALL, LOCAL_OTA_STATE_INSTALLING); }
void local_ota_get_status(local_ota_status_t *status) { if (status != NULL) *status = s_status; }

void local_ota_confirm_running_app(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
        if (result != ESP_OK) { log_error("Failed to confirm new app: %s", esp_err_to_name(result)); esp_ota_mark_app_invalid_rollback_and_reboot(); }
    }
}
