#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    LOCAL_OTA_STATE_NO_UPDATE,
    LOCAL_OTA_STATE_CHECKING,
    LOCAL_OTA_STATE_UPDATE_AVAILABLE,
    LOCAL_OTA_STATE_DOWNLOADING,
    LOCAL_OTA_STATE_READY_TO_INSTALL,
    LOCAL_OTA_STATE_INSTALLING,
    LOCAL_OTA_STATE_FAILED,
} local_ota_state_t;

typedef struct {
    local_ota_state_t state;
    size_t received_bytes;
    size_t total_bytes;
    char file_name[128];
    char detail[96];
} local_ota_status_t;

/** 异步访问固件 URL；不会写入 OTA 分区。 */
bool local_ota_check_async(void);

/** 下载、校验并写入候选 OTA 分区，但不会切换启动分区。 */
bool local_ota_download_async(void);

/** 切换到已校验的候选分区并立即重启。 */
bool local_ota_install_async(void);

/** 获取可由 UI 线程轮询的状态快照。 */
void local_ota_get_status(local_ota_status_t *status);

/** 在启动自检成功后确认当前待验证的新固件，取消回滚资格。 */
void local_ota_confirm_running_app(void);
