idf_component_register(SRCS "littlefs_test.c"
                       INCLUDE_DIRS "include"
                       REQUIRES littlefs_vfs esp_timer platform_log)
