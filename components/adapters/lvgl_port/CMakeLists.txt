idf_component_register(SRCS "lvgl_port.c"
                            "lv_port_indev.c"
                            "lv_port_disp.c"
                       INCLUDE_DIRS "."
                       REQUIRES lvgl esp_timer ili9341 xpt2046 platform_log)

idf_component_get_property(lvgl_component lvgl COMPONENT_LIB)

target_compile_definitions(${lvgl_component} PUBLIC
    "LV_CONF_PATH=${CMAKE_CURRENT_LIST_DIR}/lv_conf.h"
)
