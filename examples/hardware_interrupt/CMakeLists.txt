cmake_minimum_required(VERSION 3.13)

# Check for pico_sdk_import.cmake in current directory first
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pico_sdk_import.cmake)
    include(pico_sdk_import.cmake)
else()
    # Otherwise try to find it in the parent directory
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_sdk_import.cmake)
endif()

project(hardware_interrupt)

pico_sdk_init()

add_executable(hardware_interrupt
    main.c
)

target_link_libraries(hardware_interrupt
    pico_stdlib
    pico_rtos
    hardware_gpio
    hardware_irq
)

pico_add_extra_outputs(hardware_interrupt)

pico_enable_stdio_usb(hardware_interrupt 1)
pico_enable_stdio_uart(hardware_interrupt 0)