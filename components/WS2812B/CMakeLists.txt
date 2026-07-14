idf_component_register(SRCS "ws2812b.c"
                       INCLUDE_DIRS "include"
                       REQUIRES led_strip platform)

set(WS2812B_BACKEND "RMT" CACHE STRING "WS2812B backend: RMT or SPI")
set_property(CACHE WS2812B_BACKEND PROPERTY STRINGS RMT SPI)

if(WS2812B_BACKEND STREQUAL "RMT")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WS2812B_BACKEND=WS2812B_BACKEND_RMT)
elseif(WS2812B_BACKEND STREQUAL "SPI")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WS2812B_BACKEND=WS2812B_BACKEND_SPI)
else()
    message(FATAL_ERROR "WS2812B_BACKEND must be RMT or SPI, got: ${WS2812B_BACKEND}")
endif()
