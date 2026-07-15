idf_component_register(SRCS "lcd.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_lcd esp_lcd_ili9341 platform)
