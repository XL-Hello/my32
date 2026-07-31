idf_component_register(SRCS "environment_sensor.c"
                       INCLUDE_DIRS "."
                       REQUIRES aht20 freertos platform_log)
