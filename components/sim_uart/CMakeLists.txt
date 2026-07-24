idf_component_register(SRCS "sim_uart.c" "sim_uart_test.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_rom freertos log esp_ringbuf platform)
