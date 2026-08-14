/* SPDX-FileCopyrightText: 2026 XL
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_console.h"

#include "console.h"
#include "monitor.h"


void console_free(void)
{
    const esp_console_cmd_t free_command = {
        .command = "free",
        .help = "显示系统堆内存状态",
        .func = monitor_free_command,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&free_command));
}

void console_ps(void)
{
    const esp_console_cmd_t ps_command = {
        .command = "ps",
        .help = "显示 FreeRTOS 任务状态",
        .func = monitor_ps_command,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&ps_command));
}

void console_pool(void)
{
    const esp_console_cmd_t pool_command = {
        .command = "pool",
        .help = "显示 greatlogger 内存池状态",
        .func = monitor_pool_command,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&pool_command));
}

void console_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    repl_config.prompt = "my32> ";
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    console_free();
    console_ps();
    console_pool();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
