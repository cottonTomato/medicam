# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(BOARD disco_l475_iot1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(medicam)

option(USE_UART "Enable UART communication" OFF)
option(USE_WIFI "Enable Wi-Fi communication" OFF)

if(USE_UART AND USE_WIFI)
    message(
        FATAL_ERROR
        "Cannot enable both USE_UART and USE_WIFI at the same time."
    )
endif()

if(NOT USE_UART AND NOT USE_WIFI)
    message(STATUS "No communication option selected, defaulting to UART.")
    set(USE_UART ON)
endif()

set(app_sources src/main.c src/camera.c)

if(USE_UART)
    add_definitions(-DUSE_UART)
    list(APPEND app_sources src/uart.c)
endif()

if(USE_WIFI)
    add_definitions(-DUSE_WIFI)
    list(APPEND app_sources src/wifi.c)
endif()

target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE include)
