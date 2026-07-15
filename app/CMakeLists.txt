idf_component_register(SRCS "main.c"
                    "display/lvgl_adapt/lvgl_port.c"
                    "display/lvgl_adapt/lv_port_disp.c"
                    "display/lvgl_hello_world.c"
                    INCLUDE_DIRS "" "display" "display/lvgl_adapt"
                    REQUIRES lvgl esp_timer LCD WS2812B rgb_led spi_flash platform)
