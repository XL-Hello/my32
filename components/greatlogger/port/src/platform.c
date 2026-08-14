#include "platform.h"

glog_status_t glog_task_create(char *task_name, task_cb cb, void *arg,
                               void **task_handle)
{
    TaskHandle_t handle = NULL;

    if (task_name == NULL || cb == NULL) {
        return GLOG_ERR;
    }

    if (xTaskCreate(cb, task_name, 4096, arg, 3, &handle) != pdPASS) {
        return GLOG_ERR;
    }

    if (task_handle != NULL) {
        *task_handle = (void *)handle;
    }
    return GLOG_OK;
}

glog_status_t glog_task_notify(void *task_handle)
{
    if (task_handle == NULL) {
        return GLOG_ERR;
    }

    return xTaskNotifyGive((TaskHandle_t)task_handle) == pdPASS
               ? GLOG_OK
               : GLOG_ERR;
}

glog_status_t glog_task_notify_from_isr(void *task_handle,
                                        bool *higher_priority_task_woken)
{
    BaseType_t task_woken = pdFALSE;

    if (task_handle == NULL) {
        return GLOG_ERR;
    }

    vTaskNotifyGiveFromISR((TaskHandle_t)task_handle, &task_woken);
    if (higher_priority_task_woken != NULL) {
        *higher_priority_task_woken = task_woken == pdTRUE;
    }
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    return GLOG_OK;
}

uint32_t glog_task_notify_wait(uint32_t timeout_ms)
{
    TickType_t timeout_ticks;

    timeout_ticks = timeout_ms == UINT32_MAX
                        ? portMAX_DELAY
                        : pdMS_TO_TICKS(timeout_ms);
    return ulTaskNotifyTake(pdTRUE, timeout_ticks);
}

void *glog_mutex_create(void)
{
    return (void *)xSemaphoreCreateMutex();
}

glog_status_t glog_mutex_destroy(void *lock)
{
    if (lock == NULL) {
        return GLOG_ERR;
    }

    vSemaphoreDelete((SemaphoreHandle_t)lock);
    return GLOG_OK;
}

glog_status_t glog_mutex_lock(void *lock)
{
    if (lock == NULL) {
        return GLOG_ERR;
    }

    return xSemaphoreTake((SemaphoreHandle_t)lock, portMAX_DELAY) == pdTRUE
               ? GLOG_OK
               : GLOG_ERR;
}

glog_status_t glog_mutex_unlock(void *lock)
{
    if (lock == NULL) {
        return GLOG_ERR;
    }

    return xSemaphoreGive((SemaphoreHandle_t)lock) == pdTRUE
               ? GLOG_OK
               : GLOG_ERR;
}
