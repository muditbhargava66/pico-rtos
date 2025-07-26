cmake_minimum_required(VERSION 3.13)

# Include the Pico SDK
include(../../pico_sdk_import.cmake)

project(stream_buffer_data_streaming)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable
add_executable(stream_buffer_data_streaming
    main.c
)

# Link libraries
target_link_libraries(stream_buffer_data_streaming
    pico_stdlib
    pico_rtos
)

# Enable USB output
pico_enable_stdio_usb(stream_buffer_data_streaming 1)
pico_enable_stdio_uart(stream_buffer_data_streaming 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(stream_buffer_data_streaming)