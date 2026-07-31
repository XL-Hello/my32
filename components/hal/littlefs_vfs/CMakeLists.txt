idf_component_register(SRCS "littlefs_esp.c"
                       INCLUDE_DIRS "include"
                       REQUIRES littlefs esp_partition freertos vfs)
