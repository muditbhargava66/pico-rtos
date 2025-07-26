cmake_minimum_required(VERSION 3.13)

# Include the Pico SDK
include(../../pico_sdk_import.cmake)

project(debugging_profiling_analysis)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable
add_executable(debugging_profiling_analysis
    main.c
)

# Link libraries
target_link_libraries(debugging_profiling_analysis
    pico_stdlib
    pico_rtos
)

# Enable USB output
pico_enable_stdio_usb(debugging_profiling_analysis 1)
pico_enable_stdio_uart(debugging_profiling_analysis 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(debugging_profiling_analysis)