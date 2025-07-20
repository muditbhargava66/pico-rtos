cmake_minimum_required(VERSION 3.13)

# Check for pico_sdk_import.cmake in current directory first
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pico_sdk_import.cmake)
    include(pico_sdk_import.cmake)
else()
    # Otherwise try to find it in the parent directory
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_sdk_import.cmake)
endif()

project(power_management)

pico_sdk_init()

add_executable(power_management
    main.c
)

target_link_libraries(power_management
    pico_stdlib
    pico_rtos
    hardware_adc
    hardware_clocks
    hardware_gpio
    hardware_irq
)

pico_add_extra_outputs(power_management)

pico_enable_stdio_usb(power_management 1)
pico_enable_stdio_uart(power_management 0)