cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/app"
    "${CMAKE_CURRENT_LIST_DIR}/components/platform"
    "${CMAKE_CURRENT_LIST_DIR}/components/aht20"
    "${CMAKE_CURRENT_LIST_DIR}/components/rgb_led"
    "${CMAKE_CURRENT_LIST_DIR}/components/WS2812B"
    "${CMAKE_CURRENT_LIST_DIR}/components/ESP/led_strip")

if(NOT DEFINED ENV{IDF_PATH} OR "$ENV{IDF_PATH}" STREQUAL "")
    message(FATAL_ERROR "IDF_PATH 未设置。请先运行 source ./build.sh env，或通过 ./build.sh build 构建。")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello_world)
