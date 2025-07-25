cmake_minimum_required(VERSION 3.13)

# Include the Pico SDK
include(pico_sdk_import.cmake)

project(event_group_coordination)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable
add_executable(event_group_coordination
    main.c
)

# Link libraries
target_link_libraries(event_group_coordination
    pico_stdlib
    pico_rtos
)

# Enable USB output
pico_enable_stdio_usb(event_group_coordination 1)
pico_enable_stdio_uart(event_group_coordination 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(event_group_coordination)