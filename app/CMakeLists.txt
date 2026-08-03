idf_component_register(SRCS "main.c"
                    "ui/home/home_ui.c"
                    "ui/statusbar/statusbar_ui.c"
                    "ui/controlcenter/controlcenter_ui.c"
                    "ui/developer_mode/touch_calibration_ui.c"
                    "ui/set/settings_ui.c"
                    "ui/set/wifi/wifi_settings_ui.c"
                    "ui/icon/icon.c"
                    "ui/front/ui_font.c"
                    INCLUDE_DIRS "" "ui/home" "ui/statusbar" "ui/controlcenter" "ui/developer_mode" "ui/set" "ui/set/wifi" "ui/front" "ui/icon"
                    REQUIRES lvgl lvgl_port ws2812b rgb_led spi_flash platform_log
                             environment_sensor system_time cpu_usage wifi_manager w25q64_test littlefs_vfs
                             littlefs_test)
