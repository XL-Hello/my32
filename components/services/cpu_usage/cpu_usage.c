#include "cpu_usage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CPU_USAGE_SAMPLE_PERIOD_MS 1000
#define CPU_USAGE_TASK_STACK_SIZE 3072
#define CPU_USAGE_TASK_PRIORITY 2
#define CPU_USAGE_MAX_TASKS 32

static volatile uint8_t s_cpu_usage[portNUM_PROCESSORS];
static volatile uint8_t s_total_cpu_usage;
static uint32_t s_previous_total_runtime;
static uint32_t s_previous_idle_runtime[portNUM_PROCESSORS];

static void cpu_usage_sample(void)
{
    TaskStatus_t task_status[CPU_USAGE_MAX_TASKS];
    uint32_t total_runtime = 0;
    const UBaseType_t task_count = uxTaskGetSystemState(task_status, CPU_USAGE_MAX_TASKS,
                                                        &total_runtime);
    if (task_count == 0 || task_count > CPU_USAGE_MAX_TASKS) {
        return;
    }

    uint32_t idle_runtime[portNUM_PROCESSORS] = {0};
    TaskHandle_t idle_task_handles[portNUM_PROCESSORS];
    for (UBaseType_t core_id = 0; core_id < portNUM_PROCESSORS; ++core_id) {
        idle_task_handles[core_id] = xTaskGetIdleTaskHandleForCPU(core_id);
    }

    for (UBaseType_t index = 0; index < task_count; ++index) {
        for (UBaseType_t core_id = 0; core_id < portNUM_PROCESSORS; ++core_id) {
            if (task_status[index].xHandle == idle_task_handles[core_id]) {
                idle_runtime[core_id] = task_status[index].ulRunTimeCounter;
            }
        }
    }

    if (s_previous_total_runtime != 0) {
        const uint32_t total_runtime_delta = total_runtime - s_previous_total_runtime;
        uint64_t idle_runtime_delta = 0;
        for (UBaseType_t core_id = 0; core_id < portNUM_PROCESSORS; ++core_id) {
            uint32_t idle_runtime_delta_for_core =
                idle_runtime[core_id] - s_previous_idle_runtime[core_id];
            if (idle_runtime_delta_for_core > total_runtime_delta) {
                idle_runtime_delta_for_core = total_runtime_delta;
            }
            idle_runtime_delta += idle_runtime_delta_for_core;
            if (total_runtime_delta > 0) {
                s_cpu_usage[core_id] = (uint8_t)(100U -
                    (uint64_t)idle_runtime_delta_for_core * 100U / total_runtime_delta);
            }
        }

        /*
         * total_runtime_delta 是 ESP Timer 的墙钟时间增量。两个核心可并行执行，
         * 每个核心的占用率由其 Idle 时间相对墙钟时间的比例计算；两个核心
         * 的合并占用率仍可由各核心 Idle 时间之和计算得到。
         */
        const uint64_t available_runtime = (uint64_t)total_runtime_delta * portNUM_PROCESSORS;
        if (available_runtime > 0) {
            if (idle_runtime_delta > available_runtime) {
                idle_runtime_delta = available_runtime;
            }
            s_total_cpu_usage = (uint8_t)(100U - idle_runtime_delta * 100U / available_runtime);
        }
    }

    s_previous_total_runtime = total_runtime;
    for (UBaseType_t core_id = 0; core_id < portNUM_PROCESSORS; ++core_id) {
        s_previous_idle_runtime[core_id] = idle_runtime[core_id];
    }
}

static void cpu_usage_task(void *arg)
{
    (void)arg;
    while (true) {
        cpu_usage_sample();
        vTaskDelay(pdMS_TO_TICKS(CPU_USAGE_SAMPLE_PERIOD_MS));
    }
}

esp_err_t cpu_usage_init(void)
{
    static bool initialized;
    if (initialized) {
        return ESP_OK;
    }

    BaseType_t task_created = xTaskCreate(cpu_usage_task, "cpu_usage",
                                          CPU_USAGE_TASK_STACK_SIZE, NULL,
                                          CPU_USAGE_TASK_PRIORITY, NULL);
    if (task_created != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    initialized = true;
    return ESP_OK;
}

uint8_t cpu_usage_get_percent(void)
{
    return s_total_cpu_usage;
}

uint8_t cpu_usage_get_core_percent(uint32_t core_id)
{
    if (core_id >= portNUM_PROCESSORS) {
        return 0;
    }
    return s_cpu_usage[core_id];
}
