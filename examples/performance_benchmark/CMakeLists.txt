cmake_minimum_required(VERSION 3.13)

# Check for pico_sdk_import.cmake in current directory first
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pico_sdk_import.cmake)
    include(pico_sdk_import.cmake)
else()
    # Otherwise try to find it in the parent directory
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_sdk_import.cmake)
endif()

project(performance_benchmark)

pico_sdk_init()

add_executable(performance_benchmark
    main.c
)

target_link_libraries(performance_benchmark
    pico_stdlib
    pico_rtos
    hardware_gpio
    hardware_irq
    hardware_timer
    hardware_sync
    hardware_clocks
)

pico_add_extra_outputs(performance_benchmark)

pico_enable_stdio_usb(performance_benchmark 1)
pico_enable_stdio_uart(performance_benchmark 0)