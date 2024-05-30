cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(task_synchronization)

pico_sdk_init()

add_executable(task_synchronization
    main.c
)

target_link_libraries(task_synchronization
    pico_stdlib
    pico_rtos
)

pico_add_extra_outputs(task_synchronization)

pico_enable_stdio_usb(task_synchronization 1)
pico_enable_stdio_uart(task_synchronization 0)