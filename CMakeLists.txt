cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/app"
    "${CMAKE_CURRENT_LIST_DIR}/components/platform"
    "${CMAKE_CURRENT_LIST_DIR}/components/platform/platform_log"
    "${CMAKE_CURRENT_LIST_DIR}/components/hal/common/ringbuffer"
    "${CMAKE_CURRENT_LIST_DIR}/components/hal/littlefs_vfs"
    "${CMAKE_CURRENT_LIST_DIR}/components/hal/soft_spi"
    "${CMAKE_CURRENT_LIST_DIR}/components/hal/soft_uart"
    "${CMAKE_CURRENT_LIST_DIR}/components/drivers/sensors/aht20"
    "${CMAKE_CURRENT_LIST_DIR}/components/drivers/storage/w25q64"
    "${CMAKE_CURRENT_LIST_DIR}/components/drivers/display/ili9341"
    "${CMAKE_CURRENT_LIST_DIR}/components/drivers/input/xpt2046"
    "${CMAKE_CURRENT_LIST_DIR}/components/drivers/lighting/ws2812b"
    "${CMAKE_CURRENT_LIST_DIR}/components/drivers/lighting/rgb_led"
    "${CMAKE_CURRENT_LIST_DIR}/components/services/environment_sensor"
    "${CMAKE_CURRENT_LIST_DIR}/components/services/system_time"
    "${CMAKE_CURRENT_LIST_DIR}/components/services/cpu_usage"
    "${CMAKE_CURRENT_LIST_DIR}/components/services/wifi_manager"
    "${CMAKE_CURRENT_LIST_DIR}/components/services/weather"
    "${CMAKE_CURRENT_LIST_DIR}/components/adapters/lvgl_port"
    "${CMAKE_CURRENT_LIST_DIR}/components/diagnostics/w25q64_test"
    "${CMAKE_CURRENT_LIST_DIR}/components/diagnostics/littlefs_test"
    "${CMAKE_CURRENT_LIST_DIR}/components/third_party/led_strip"
    "${CMAKE_CURRENT_LIST_DIR}/components/third_party/lvgl"
    "${CMAKE_CURRENT_LIST_DIR}/components/third_party/littlefs")

if(NOT DEFINED ENV{IDF_PATH} OR "$ENV{IDF_PATH}" STREQUAL "")
    message(FATAL_ERROR "IDF_PATH 未设置。请先运行 source ./build.sh env，或通过 ./build.sh build 构建。")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world)
