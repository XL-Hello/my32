/* SPDX-FileCopyrightText: 2026 XL
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#define LOG_TAG "monitor"

#include <stdint.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "glog.h"
#include "platform_log.h"

#include "monitor.h"

#define MONITOR_MAX_TASKS 32U
#define MONITOR_SIZE_TEXT_LENGTH 16U
#define MONITOR_KIB_THRESHOLD (10U * 1024U)
#define MONITOR_FREE_HEADER_FORMAT "%10s %10s %10s %10s %10s %6s %6s %s"
#define MONITOR_FREE_ROW_FORMAT "%10s %10s %10s %10s %10s %6u %6u %s"
#define MONITOR_PS_HEADER "PID PRI STATE      STACK_FREE    CPU COMMAND"
#define MONITOR_PS_ROW_FORMAT "%3u %3u %-10s %10s %3u.%u%% %s"

static TaskStatus_t s_task_status[MONITOR_MAX_TASKS];

static void monitor_format_size(size_t bytes, char *text, size_t text_length)
{
    if (bytes > MONITOR_KIB_THRESHOLD) {
        size_t kib = bytes / 1024U;
        size_t decimal = ((bytes % 1024U) * 10U) / 1024U;

        (void)snprintf(text, text_length, "%u.%u KiB", (unsigned int)kib, (unsigned int)decimal);
        return;
    }

    (void)snprintf(text, text_length, "%u B", (unsigned int)bytes);
}

static const char *monitor_task_state_name(eTaskState state)
{
    static const char *const state_names[] = {
        [eRunning] = "Running",
        [eReady] = "Ready",
        [eBlocked] = "Waiting",
        [eSuspended] = "Suspended",
        [eDeleted] = "Deleted",
        [eInvalid] = "Invalid",
    };

    if ((unsigned int)state >= (sizeof(state_names) / sizeof(state_names[0])) || state_names[state] == NULL) {
        return "Unknown";
    }

    return state_names[state];
}

int monitor_free_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
        multi_heap_info_t heap_info;
    char total[MONITOR_SIZE_TEXT_LENGTH];
    char used[MONITOR_SIZE_TEXT_LENGTH];
    char free[MONITOR_SIZE_TEXT_LENGTH];
    char maxused[MONITOR_SIZE_TEXT_LENGTH];
    char maxfree[MONITOR_SIZE_TEXT_LENGTH];

    heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);
    monitor_format_size(heap_info.total_allocated_bytes + heap_info.total_free_bytes, total, sizeof(total));
    monitor_format_size(heap_info.total_allocated_bytes, used, sizeof(used));
    monitor_format_size(heap_info.total_free_bytes, free, sizeof(free));
    monitor_format_size(heap_info.total_allocated_bytes + heap_info.total_free_bytes - heap_info.minimum_free_bytes,
                        maxused,
                        sizeof(maxused));
    monitor_format_size(heap_info.largest_free_block, maxfree, sizeof(maxfree));
    log_printf(MONITOR_FREE_HEADER_FORMAT,
               "total",
               "used",
               "free",
               "maxused",
               "maxfree",
               "nused",
               "nfree",
               "name");
    log_printf(MONITOR_FREE_ROW_FORMAT,
               total,
               used,
               free,
               maxused,
               maxfree,
               (unsigned int)heap_info.allocated_blocks,
               (unsigned int)heap_info.free_blocks,
               "8bit_heap");
    return 0;
}

int monitor_ps_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    uint32_t total_runtime = 0U;

    if (task_count > MONITOR_MAX_TASKS) {
        log_printf("任务数量=%u，超过监控上限=%u", (unsigned int)task_count, MONITOR_MAX_TASKS);
        return -1;
    }

    task_count = uxTaskGetSystemState(s_task_status, task_count, &total_runtime);
    log_printf(MONITOR_PS_HEADER);
    for (UBaseType_t index = 0U; index < task_count; ++index) {
        uint32_t cpu_tenths = 0U;
        char stack_free[MONITOR_SIZE_TEXT_LENGTH];

        if (total_runtime != 0U) {
            cpu_tenths = (uint32_t)(((uint64_t)s_task_status[index].ulRunTimeCounter * 1000U) / total_runtime);
        }

        monitor_format_size(s_task_status[index].usStackHighWaterMark, stack_free, sizeof(stack_free));
        log_printf(MONITOR_PS_ROW_FORMAT,
                   (unsigned int)s_task_status[index].xTaskNumber,
                   (unsigned int)s_task_status[index].uxCurrentPriority,
                   monitor_task_state_name(s_task_status[index].eCurrentState),
                   stack_free,
                   (unsigned int)(cpu_tenths / 10U),
                   (unsigned int)(cpu_tenths % 10U),
                   s_task_status[index].pcTaskName);
    }
    return 0;
}

int monitor_pool_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    glog_print_pool_stats();
    return 0;
}
