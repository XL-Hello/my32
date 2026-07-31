idf_component_register(SRCS "main.c"
                    "ui/home/home_ui.c"
                    "ui/developer_mode/touch_calibration_ui.c"
                    INCLUDE_DIRS "" "ui/home" "ui/developer_mode"
                    REQUIRES lvgl lvgl_port ws2812b rgb_led spi_flash platform_log
                             environment_sensor system_time cpu_usage w25q64_test littlefs_vfs
                             littlefs_test)
