idf_component_register(SRCS "w25q64_test.c"
                       INCLUDE_DIRS "include"
                       REQUIRES w25q64 soft_spi freertos platform_log)
