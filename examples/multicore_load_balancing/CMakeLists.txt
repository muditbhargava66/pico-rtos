cmake_minimum_required(VERSION 3.13)

# Include the Pico SDK
include(../../pico_sdk_import.cmake)

project(multicore_load_balancing)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable
add_executable(multicore_load_balancing
    main.c
)

# Link libraries
target_link_libraries(multicore_load_balancing
    pico_stdlib
    pico_rtos
)

# Enable USB output
pico_enable_stdio_usb(multicore_load_balancing 1)
pico_enable_stdio_uart(multicore_load_balancing 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(multicore_load_balancing)