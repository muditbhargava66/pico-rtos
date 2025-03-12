cmake_minimum_required(VERSION 3.13)

# Check if PICO_SDK_PATH is defined
if(DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    message("Using Pico SDK path from environment: ${PICO_SDK_PATH}")
else()
    # Check if pico-sdk exists as a submodule
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/pico-sdk/CMakeLists.txt)
        set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/extern/pico-sdk)
        message("Using Pico SDK from submodule at: ${PICO_SDK_PATH}")
    else()
        message(FATAL_ERROR "
        Pico SDK not found. Please either:
        1. Clone the Pico SDK into extern/pico-sdk: 
           git submodule add -b master https://github.com/raspberrypi/pico-sdk.git extern/pico-sdk
           git submodule update --init --recursive
        2. Run the setup-pico-sdk.sh script: 
           ./setup-pico-sdk.sh
        3. Set the PICO_SDK_PATH environment variable to your Pico SDK installation.")
    endif()
endif()

# Include the Pico SDK cmake file
include(pico_sdk_import.cmake)

project(pico_rtos C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Version information
set(PICO_RTOS_VERSION_MAJOR 0)
set(PICO_RTOS_VERSION_MINOR 1)
set(PICO_RTOS_VERSION_PATCH 0)

# Configuration options
option(PICO_RTOS_BUILD_EXAMPLES "Build the example projects" ON)
option(PICO_RTOS_BUILD_TESTS "Build the test projects" ON)
option(PICO_RTOS_ENABLE_DEBUG "Enable debug output" ON)
option(PICO_RTOS_ENABLE_INSTALL "Enable installation targets" OFF)

# Add compile definitions based on options
if(PICO_RTOS_ENABLE_DEBUG)
    add_compile_definitions(PICO_RTOS_DEBUG=1)
endif()

# Create the RTOS library
add_library(pico_rtos STATIC
    src/mutex.c
    src/queue.c
    src/semaphore.c
    src/task.c
    src/timer.c
    src/core.c
)

# Set up include directories - use generator expressions to handle install correctly
target_include_directories(pico_rtos PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link with Pico SDK
target_link_libraries(pico_rtos
    pico_stdlib
    hardware_timer
)

# Build examples if enabled
if(PICO_RTOS_BUILD_EXAMPLES)
    add_subdirectory(examples/led_blinking)
    add_subdirectory(examples/task_synchronization)
endif()

# Build tests if enabled
if(PICO_RTOS_BUILD_TESTS)
    # Add test executables
    add_executable(mutex_test tests/mutex_test.c)
    target_link_libraries(mutex_test pico_rtos pico_stdlib)
    pico_add_extra_outputs(mutex_test)
    pico_enable_stdio_usb(mutex_test 1)
    pico_enable_stdio_uart(mutex_test 0)
    
    add_executable(queue_test tests/queue_test.c)
    target_link_libraries(queue_test pico_rtos pico_stdlib)
    pico_add_extra_outputs(queue_test)
    pico_enable_stdio_usb(queue_test 1)
    pico_enable_stdio_uart(queue_test 0)
    
    add_executable(semaphore_test tests/semaphore_test.c)
    target_link_libraries(semaphore_test pico_rtos pico_stdlib)
    pico_add_extra_outputs(semaphore_test)
    pico_enable_stdio_usb(semaphore_test 1)
    pico_enable_stdio_uart(semaphore_test 0)
    
    add_executable(task_test tests/task_test.c)
    target_link_libraries(task_test pico_rtos pico_stdlib)
    pico_add_extra_outputs(task_test)
    pico_enable_stdio_usb(task_test 1)
    pico_enable_stdio_uart(task_test 0)
    
    add_executable(timer_test tests/timer_test.c)
    target_link_libraries(timer_test pico_rtos pico_stdlib)
    pico_add_extra_outputs(timer_test)
    pico_enable_stdio_usb(timer_test 1)
    pico_enable_stdio_uart(timer_test 0)
endif()

# Installation configuration (only if enabled)
if(PICO_RTOS_ENABLE_INSTALL)
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/pico_rtos-config-version.cmake"
        VERSION ${PICO_RTOS_VERSION_MAJOR}.${PICO_RTOS_VERSION_MINOR}.${PICO_RTOS_VERSION_PATCH}
        COMPATIBILITY SameMajorVersion
    )

    install(TARGETS pico_rtos
        EXPORT pico_rtos-targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )

    install(
        EXPORT pico_rtos-targets
        FILE pico_rtos-targets.cmake
        NAMESPACE pico_rtos::
        DESTINATION lib/cmake/pico_rtos
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Pico-RTOS Configuration:")
message(STATUS "  Version: ${PICO_RTOS_VERSION_MAJOR}.${PICO_RTOS_VERSION_MINOR}.${PICO_RTOS_VERSION_PATCH}")
message(STATUS "  Build examples: ${PICO_RTOS_BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${PICO_RTOS_BUILD_TESTS}")
message(STATUS "  Debug enabled: ${PICO_RTOS_ENABLE_DEBUG}")
message(STATUS "  Install enabled: ${PICO_RTOS_ENABLE_INSTALL}")
message(STATUS "")