#pragma once

#include <stddef.h>

typedef enum {
    SYS_INFO_UPDATE_NO_UPDATE,
    SYS_INFO_UPDATE_CHECKING,
    SYS_INFO_UPDATE_AVAILABLE,
    SYS_INFO_UPDATE_DOWNLOADING,
    SYS_INFO_UPDATE_READY_TO_INSTALL,
    SYS_INFO_UPDATE_INSTALLING,
    SYS_INFO_UPDATE_FAILED,
} sys_info_update_state_t;

typedef struct {
    const char *model;
    const char *os_version;
    const char *storage;
} sys_info_device_info_t;

void sys_info_ui_create(void);
void sys_info_ui_set_device_info(const sys_info_device_info_t *device_info);
void sys_info_ui_update_progress(size_t received_bytes, size_t total_bytes);
void sys_info_ui_set_update_state(sys_info_update_state_t state, const char *file_name,
                                  const char *detail);
