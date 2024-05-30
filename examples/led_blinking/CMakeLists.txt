cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(led_blinking)

pico_sdk_init()

add_executable(led_blinking
    main.c
)

target_link_libraries(led_blinking
    pico_stdlib
    pico_rtos
)

pico_add_extra_outputs(led_blinking)

pico_enable_stdio_usb(led_blinking 1)
pico_enable_stdio_uart(led_blinking 0)