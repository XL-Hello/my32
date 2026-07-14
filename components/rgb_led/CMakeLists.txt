idf_component_register(SRCS "rgb_led.c" "rgb_led_driver.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES driver freertos platform)
