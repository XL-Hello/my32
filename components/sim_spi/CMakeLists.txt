idf_component_register(SRCS "sim_spi.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_rom freertos log esp_ringbuf platform)
