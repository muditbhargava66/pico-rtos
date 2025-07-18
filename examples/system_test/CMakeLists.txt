cmake_minimum_required(VERSION 3.13)

# Check for pico_sdk_import.cmake in current directory first
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pico_sdk_import.cmake)
    include(pico_sdk_import.cmake)
else()
    # Otherwise try to find it in the parent directory
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../pico_sdk_import.cmake)
endif()

project(system_test)

pico_sdk_init()

add_executable(system_test
    main.c
)

target_link_libraries(system_test
    pico_stdlib
    pico_rtos
)

pico_add_extra_outputs(system_test)

pico_enable_stdio_usb(system_test 1)
pico_enable_stdio_uart(system_test 0)