idf_component_register(SRCS "weather_service.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_client esp-tls json nvs_flash wifi_manager freertos platform_log)
