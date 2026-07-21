idf_component_register(SRCS "lcd.c" "touch.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_adc esp_lcd esp_lcd_ili9341 platform)
