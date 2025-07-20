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
set(PICO_RTOS_VERSION_MINOR 2)
set(PICO_RTOS_VERSION_PATCH 1)

# Build configuration options
option(PICO_RTOS_BUILD_EXAMPLES "Build the example projects" ON)
option(PICO_RTOS_BUILD_TESTS "Build the test projects" ON)
option(PICO_RTOS_ENABLE_DEBUG "Enable debug output and assertions" ON)
option(PICO_RTOS_ENABLE_INSTALL "Enable installation targets" OFF)

# System timing configuration
set(PICO_RTOS_TICK_RATE_HZ "1000" CACHE STRING "System tick frequency in Hz (affects timing precision and power consumption)")
set_property(CACHE PICO_RTOS_TICK_RATE_HZ PROPERTY STRINGS "100;250;500;1000;2000")

# System resource limits configuration
set(PICO_RTOS_MAX_TASKS "16" CACHE STRING "Maximum number of concurrent tasks (affects memory usage)")
set(PICO_RTOS_MAX_TIMERS "8" CACHE STRING "Maximum number of software timers (affects memory usage)")
set(PICO_RTOS_TASK_STACK_SIZE_DEFAULT "1024" CACHE STRING "Default task stack size in bytes")
set(PICO_RTOS_IDLE_STACK_SIZE "256" CACHE STRING "Idle task stack size in bytes")

# Feature toggles
option(PICO_RTOS_ENABLE_STACK_CHECKING "Enable stack overflow checking" ON)
option(PICO_RTOS_ENABLE_MEMORY_TRACKING "Enable memory usage tracking" ON)
option(PICO_RTOS_ENABLE_RUNTIME_STATS "Enable runtime statistics collection" ON)

# Debug and logging options
option(PICO_RTOS_ENABLE_LOGGING "Enable debug logging system" OFF)
set(PICO_RTOS_LOG_LEVEL "3" CACHE STRING "Default log level (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)")
set_property(CACHE PICO_RTOS_LOG_LEVEL PROPERTY STRINGS "0;1;2;3;4")
set(PICO_RTOS_LOG_MESSAGE_MAX_LENGTH "128" CACHE STRING "Maximum length of log messages")
set(PICO_RTOS_LOG_SUBSYSTEM_MASK "0xFF" CACHE STRING "Bitmask for enabled log subsystems (0xFF=all)")

# Error handling options
option(PICO_RTOS_ENABLE_ERROR_HISTORY "Enable error history tracking" ON)
set(PICO_RTOS_ERROR_HISTORY_SIZE "10" CACHE STRING "Number of errors to keep in history")

# Validate configuration options
if(NOT PICO_RTOS_TICK_RATE_HZ MATCHES "^[0-9]+$")
    message(FATAL_ERROR "PICO_RTOS_TICK_RATE_HZ must be a positive integer")
endif()

if(PICO_RTOS_TICK_RATE_HZ LESS 10 OR PICO_RTOS_TICK_RATE_HZ GREATER 10000)
    message(FATAL_ERROR "PICO_RTOS_TICK_RATE_HZ must be between 10 and 10000 Hz")
endif()

if(NOT PICO_RTOS_MAX_TASKS MATCHES "^[0-9]+$" OR PICO_RTOS_MAX_TASKS LESS 1)
    message(FATAL_ERROR "PICO_RTOS_MAX_TASKS must be a positive integer")
endif()

if(NOT PICO_RTOS_MAX_TIMERS MATCHES "^[0-9]+$" OR PICO_RTOS_MAX_TIMERS LESS 1)
    message(FATAL_ERROR "PICO_RTOS_MAX_TIMERS must be a positive integer")
endif()

if(NOT PICO_RTOS_TASK_STACK_SIZE_DEFAULT MATCHES "^[0-9]+$" OR PICO_RTOS_TASK_STACK_SIZE_DEFAULT LESS 256)
    message(FATAL_ERROR "PICO_RTOS_TASK_STACK_SIZE_DEFAULT must be at least 256 bytes")
endif()

if(NOT PICO_RTOS_IDLE_STACK_SIZE MATCHES "^[0-9]+$" OR PICO_RTOS_IDLE_STACK_SIZE LESS 128)
    message(FATAL_ERROR "PICO_RTOS_IDLE_STACK_SIZE must be at least 128 bytes")
endif()

# Warn about potentially problematic configurations
if(PICO_RTOS_MAX_TASKS GREATER 32)
    message(WARNING "PICO_RTOS_MAX_TASKS > 32 may consume significant memory and affect performance")
endif()

if(PICO_RTOS_TASK_STACK_SIZE_DEFAULT GREATER 4096)
    message(WARNING "Large default stack size may consume significant memory")
endif()

if(NOT PICO_RTOS_ERROR_HISTORY_SIZE MATCHES "^[0-9]+$" OR PICO_RTOS_ERROR_HISTORY_SIZE LESS 1)
    message(FATAL_ERROR "PICO_RTOS_ERROR_HISTORY_SIZE must be a positive integer")
endif()

# Validate logging configuration
if(PICO_RTOS_ENABLE_LOGGING)
    if(NOT PICO_RTOS_LOG_LEVEL MATCHES "^[0-4]$")
        message(FATAL_ERROR "PICO_RTOS_LOG_LEVEL must be between 0 and 4 (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)")
    endif()
    
    if(NOT PICO_RTOS_LOG_MESSAGE_MAX_LENGTH MATCHES "^[0-9]+$" OR PICO_RTOS_LOG_MESSAGE_MAX_LENGTH LESS 32)
        message(FATAL_ERROR "PICO_RTOS_LOG_MESSAGE_MAX_LENGTH must be at least 32 characters")
    endif()
    
    if(PICO_RTOS_LOG_MESSAGE_MAX_LENGTH GREATER 512)
        message(WARNING "PICO_RTOS_LOG_MESSAGE_MAX_LENGTH > 512 may consume significant memory")
    endif()
    
    if(NOT PICO_RTOS_LOG_SUBSYSTEM_MASK MATCHES "^0x[0-9A-Fa-f]+$" AND NOT PICO_RTOS_LOG_SUBSYSTEM_MASK MATCHES "^[0-9]+$")
        message(FATAL_ERROR "PICO_RTOS_LOG_SUBSYSTEM_MASK must be a valid hexadecimal (0x...) or decimal number")
    endif()
endif()

# Validate dependencies
if(PICO_RTOS_ENABLE_ERROR_HISTORY AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
    message(WARNING "Error history requires memory tracking to be enabled for optimal operation")
endif()

# Additional dependency validations
if(PICO_RTOS_ENABLE_RUNTIME_STATS AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
    message(WARNING "Runtime statistics work best with memory tracking enabled")
endif()

if(PICO_RTOS_ENABLE_LOGGING AND PICO_RTOS_LOG_LEVEL GREATER 2 AND NOT PICO_RTOS_ENABLE_DEBUG)
    message(WARNING "High log levels (INFO/DEBUG) are most useful with debug mode enabled")
endif()

# Add compile definitions based on options
if(PICO_RTOS_ENABLE_DEBUG)
    add_compile_definitions(PICO_RTOS_DEBUG=1)
endif()

# Add configuration compile definitions
add_compile_definitions(
    PICO_RTOS_TICK_RATE_HZ=${PICO_RTOS_TICK_RATE_HZ}
    PICO_RTOS_MAX_TASKS=${PICO_RTOS_MAX_TASKS}
    PICO_RTOS_MAX_TIMERS=${PICO_RTOS_MAX_TIMERS}
    PICO_RTOS_TASK_STACK_SIZE_DEFAULT=${PICO_RTOS_TASK_STACK_SIZE_DEFAULT}
    PICO_RTOS_IDLE_STACK_SIZE=${PICO_RTOS_IDLE_STACK_SIZE}
    PICO_RTOS_ERROR_HISTORY_SIZE=${PICO_RTOS_ERROR_HISTORY_SIZE}
)

# Feature-based conditional compilation
if(PICO_RTOS_ENABLE_STACK_CHECKING)
    add_compile_definitions(PICO_RTOS_ENABLE_STACK_CHECKING=1)
endif()

if(PICO_RTOS_ENABLE_MEMORY_TRACKING)
    add_compile_definitions(PICO_RTOS_ENABLE_MEMORY_TRACKING=1)
endif()

if(PICO_RTOS_ENABLE_RUNTIME_STATS)
    add_compile_definitions(PICO_RTOS_ENABLE_RUNTIME_STATS=1)
endif()

if(PICO_RTOS_ENABLE_ERROR_HISTORY)
    add_compile_definitions(PICO_RTOS_ENABLE_ERROR_HISTORY=1)
endif()

# Logging system configuration
if(PICO_RTOS_ENABLE_LOGGING)
    add_compile_definitions(
        PICO_RTOS_ENABLE_LOGGING=1
        PICO_RTOS_LOG_LEVEL=${PICO_RTOS_LOG_LEVEL}
        PICO_RTOS_LOG_MESSAGE_MAX_LENGTH=${PICO_RTOS_LOG_MESSAGE_MAX_LENGTH}
        PICO_RTOS_LOG_SUBSYSTEM_MASK=${PICO_RTOS_LOG_SUBSYSTEM_MASK}
    )
endif()

# Create the RTOS library
add_library(pico_rtos STATIC
    src/mutex.c
    src/queue.c
    src/semaphore.c
    src/task.c
    src/timer.c
    src/core.c
    src/context_switch.c
    src/context_switch.S
    src/blocking.c
    src/error.c
    src/logging.c
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
    add_subdirectory(examples/task_communication)
    add_subdirectory(examples/system_test)
    add_subdirectory(examples/hardware_interrupt)
    add_subdirectory(examples/power_management)
    add_subdirectory(examples/performance_benchmark)
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

# Function to display configuration help
function(pico_rtos_print_config_help)
    message(STATUS "")
    message(STATUS "=== Pico-RTOS Configuration Options ===")
    message(STATUS "")
    message(STATUS "Build Options:")
    message(STATUS "  -DPICO_RTOS_BUILD_EXAMPLES=ON/OFF     Build example projects")
    message(STATUS "  -DPICO_RTOS_BUILD_TESTS=ON/OFF        Build test projects")
    message(STATUS "  -DPICO_RTOS_ENABLE_DEBUG=ON/OFF       Enable debug output and assertions")
    message(STATUS "  -DPICO_RTOS_ENABLE_INSTALL=ON/OFF     Enable installation targets")
    message(STATUS "")
    message(STATUS "System Configuration:")
    message(STATUS "  -DPICO_RTOS_TICK_RATE_HZ=<value>      System tick frequency (100,250,500,1000,2000)")
    message(STATUS "  -DPICO_RTOS_MAX_TASKS=<value>         Maximum concurrent tasks (default: 16)")
    message(STATUS "  -DPICO_RTOS_MAX_TIMERS=<value>        Maximum software timers (default: 8)")
    message(STATUS "  -DPICO_RTOS_TASK_STACK_SIZE_DEFAULT=<value>  Default task stack size (default: 1024)")
    message(STATUS "  -DPICO_RTOS_IDLE_STACK_SIZE=<value>   Idle task stack size (default: 256)")
    message(STATUS "")
    message(STATUS "Feature Options:")
    message(STATUS "  -DPICO_RTOS_ENABLE_STACK_CHECKING=ON/OFF     Stack overflow detection")
    message(STATUS "  -DPICO_RTOS_ENABLE_MEMORY_TRACKING=ON/OFF    Memory usage tracking")
    message(STATUS "  -DPICO_RTOS_ENABLE_RUNTIME_STATS=ON/OFF      Runtime statistics collection")
    message(STATUS "  -DPICO_RTOS_ENABLE_ERROR_HISTORY=ON/OFF      Error history tracking")
    message(STATUS "  -DPICO_RTOS_ERROR_HISTORY_SIZE=<value>       Error history buffer size")
    message(STATUS "")
    message(STATUS "Logging Options:")
    message(STATUS "  -DPICO_RTOS_ENABLE_LOGGING=ON/OFF            Enable debug logging")
    message(STATUS "  -DPICO_RTOS_LOG_LEVEL=<0-4>                  Log level (0=NONE,1=ERROR,2=WARN,3=INFO,4=DEBUG)")
    message(STATUS "  -DPICO_RTOS_LOG_MESSAGE_MAX_LENGTH=<value>   Maximum log message length")
    message(STATUS "  -DPICO_RTOS_LOG_SUBSYSTEM_MASK=<value>       Subsystem logging mask")
    message(STATUS "")
endfunction()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Pico-RTOS Configuration Summary ===")
message(STATUS "")
message(STATUS "Version: ${PICO_RTOS_VERSION_MAJOR}.${PICO_RTOS_VERSION_MINOR}.${PICO_RTOS_VERSION_PATCH}")
message(STATUS "")
message(STATUS "Build Configuration:")
message(STATUS "  Examples: ${PICO_RTOS_BUILD_EXAMPLES}")
message(STATUS "  Tests: ${PICO_RTOS_BUILD_TESTS}")
message(STATUS "  Debug: ${PICO_RTOS_ENABLE_DEBUG}")
message(STATUS "  Install: ${PICO_RTOS_ENABLE_INSTALL}")
message(STATUS "")
message(STATUS "System Configuration:")
message(STATUS "  Tick rate: ${PICO_RTOS_TICK_RATE_HZ} Hz")
message(STATUS "  Max tasks: ${PICO_RTOS_MAX_TASKS}")
message(STATUS "  Max timers: ${PICO_RTOS_MAX_TIMERS}")
message(STATUS "  Default task stack: ${PICO_RTOS_TASK_STACK_SIZE_DEFAULT} bytes")
message(STATUS "  Idle task stack: ${PICO_RTOS_IDLE_STACK_SIZE} bytes")
message(STATUS "")
message(STATUS "Feature Configuration:")
message(STATUS "  Stack checking: ${PICO_RTOS_ENABLE_STACK_CHECKING}")
message(STATUS "  Memory tracking: ${PICO_RTOS_ENABLE_MEMORY_TRACKING}")
message(STATUS "  Runtime stats: ${PICO_RTOS_ENABLE_RUNTIME_STATS}")
message(STATUS "  Error history: ${PICO_RTOS_ENABLE_ERROR_HISTORY} (size: ${PICO_RTOS_ERROR_HISTORY_SIZE})")
message(STATUS "")
message(STATUS "Debug and Logging Configuration:")
message(STATUS "  Debug logging: ${PICO_RTOS_ENABLE_LOGGING}")
if(PICO_RTOS_ENABLE_LOGGING)
    message(STATUS "  Log level: ${PICO_RTOS_LOG_LEVEL} (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)")
    message(STATUS "  Max message length: ${PICO_RTOS_LOG_MESSAGE_MAX_LENGTH}")
    message(STATUS "  Subsystem mask: ${PICO_RTOS_LOG_SUBSYSTEM_MASK}")
endif()
message(STATUS "")

# Add option to show configuration help
option(PICO_RTOS_SHOW_CONFIG_HELP "Show configuration options help" OFF)
if(PICO_RTOS_SHOW_CONFIG_HELP)
    pico_rtos_print_config_help()
endif()