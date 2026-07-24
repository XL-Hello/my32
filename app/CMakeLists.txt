idf_component_register(SRCS "main.c"
                    "display/lvgl_adapt/lvgl_port.c"
                    "display/lvgl_adapt/lv_port_indev.c"
                    "display/lvgl_adapt/lv_port_disp.c"
                    "ui/home/home_ui.c"
                    "ui/developerMode/touch_calibration_ui.c"
                    "services/environment_sensor/environment_sensor.c"
                    "services/system_time/system_time.c"
                    "services/cpu_usage/cpu_usage.c"
                    INCLUDE_DIRS "" "display" "display/lvgl_adapt" "ui/home"
                                 "ui/developerMode" "services/environment_sensor"
                                 "services/system_time" "services/cpu_usage"
                    REQUIRES lvgl esp_timer LCD WS2812B rgb_led spi_flash platform aht20 sim_uart)

idf_component_get_property(lvgl_component lvgl COMPONENT_LIB)

target_compile_definitions(${lvgl_component} PUBLIC
    "LV_CONF_PATH=${CMAKE_CURRENT_LIST_DIR}/display/lvgl_adapt/lv_conf.h"
)
