/**
 * @file wifi_manager.h
 * @brief ESP32-S3 STA 模式 Wi-Fi 配置、连接与状态查询服务。
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define WIFI_MANAGER_SSID_MAX_LEN 32
#define WIFI_MANAGER_PASSWORD_MAX_LEN 63
#define WIFI_MANAGER_MAX_SCAN_RESULTS 20

typedef enum {
    WIFI_MANAGER_STATUS_DISABLED,
    WIFI_MANAGER_STATUS_UNCONFIGURED,
    WIFI_MANAGER_STATUS_DISCONNECTED,
    WIFI_MANAGER_STATUS_SCANNING,
    WIFI_MANAGER_STATUS_CONNECTING,
    WIFI_MANAGER_STATUS_AUTH_FAILED,
    WIFI_MANAGER_STATUS_NO_AP_FOUND,
    WIFI_MANAGER_STATUS_CONNECTED_NO_IP,
    WIFI_MANAGER_STATUS_CHECKING_INTERNET,
    WIFI_MANAGER_STATUS_LOCAL_NETWORK_READY,
    WIFI_MANAGER_STATUS_INTERNET_READY,
    WIFI_MANAGER_STATUS_INTERNET_UNREACHABLE,
} wifi_manager_status_t;

typedef struct {
    char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
    int8_t rssi;
    uint8_t authmode;
} wifi_manager_ap_info_t;

typedef struct {
    wifi_manager_status_t status;
    char ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
    char ipv4[16];
    int8_t rssi;
    bool enabled;
    bool internet_available;
} wifi_manager_network_info_t;

/** 初始化 Wi-Fi、默认 STA 网络接口和凭据存储，并尝试自动连接已保存热点。 */
esp_err_t wifi_manager_init(void);

/** 启用或停用 Wi-Fi。停用不会删除已保存凭据。 */
esp_err_t wifi_manager_set_enabled(bool enabled);

/** 异步扫描附近热点；扫描结果可通过 wifi_manager_get_scan_results() 获取。 */
esp_err_t wifi_manager_start_scan(void);

/** 获取最近一次扫描结果。传入 NULL 可仅查询数量。 */
esp_err_t wifi_manager_get_scan_results(wifi_manager_ap_info_t *results,
                                        size_t results_capacity, size_t *result_count);

/** 当前是否正在扫描。 */
bool wifi_manager_is_scanning(void);

/** 连接热点；在获取 IPv4 地址成功后持久化凭据。 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/** 主动断开当前连接，但保留凭据以便用户随后重新连接。 */
esp_err_t wifi_manager_disconnect(void);

/** 删除保存的凭据并停止自动重连。 */
esp_err_t wifi_manager_forget_network(void);

/** 获取状态及当前网络信息。 */
esp_err_t wifi_manager_get_network_info(wifi_manager_network_info_t *info);

/** 将服务状态转换为适合界面展示的中文文本。 */
const char *wifi_manager_status_to_text(wifi_manager_status_t status);
