#ifndef GLOG_PLATFORM_H
#define GLOG_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLOG_OK = 0,
    GLOG_ERR
} glog_status_t;

//延时
#define glog_delay_ms(ms) vTaskDelay(pdMS_TO_TICKS(ms))

/* 获取系统 RTC 时间戳，单位为 Unix Epoch 毫秒。 */
uint64_t glog_rtc_time_ms(void);

// 堆申请
#define glog_ps_malloc(size) ps_malloc(size)
#define glog_ps_calloc(count, size) ps_calloc(count, size)
#define glog_free(ptr) free(ptr)

//任务创建
typedef void (*task_cb)(void *arg);
glog_status_t glog_task_create(char *task_name, task_cb cb, void *arg, void **task_handle);

/* 计数型任务通知：用于唤醒等待数据的工作任务。 */
glog_status_t glog_task_notify(void *task_handle);
/* 仅可在 ISR 中调用；若唤醒更高优先级任务，会在返回前请求任务切换。 */
glog_status_t glog_task_notify_from_isr(void *task_handle, bool *higher_priority_task_woken);
/* 仅可在任务上下文调用；timeout_ms 为 UINT32_MAX 时无限等待。 */
uint32_t glog_task_notify_wait(uint32_t timeout_ms);

//互斥锁
void* glog_mutex_create(void);
glog_status_t glog_mutex_destroy(void *lock);
glog_status_t glog_mutex_lock(void *lock);
glog_status_t glog_mutex_unlock(void *lock);

#ifdef __cplusplus
}
#endif

#endif /* GLOG_PLATFORM_H */
