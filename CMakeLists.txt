cmake_minimum_required(VERSION 3.13)

# Toolchain configuration options
option(PICO_RTOS_AUTO_DETECT_TOOLCHAIN "Automatically detect and configure toolchain" ON)
set(PICO_RTOS_TOOLCHAIN "" CACHE STRING "Specify toolchain to use (arm-none-eabi-gcc, arm-none-eabi-clang)")
option(PICO_RTOS_ENABLE_STACK_PROTECTION "Enable stack protection if supported by toolchain" OFF)
option(PICO_RTOS_ENABLE_SANITIZERS "Enable sanitizers if supported by toolchain (debug builds only)" OFF)

# Include toolchain configuration before project()
include(cmake/PicoRTOSToolchain.cmake)

# Include feature optimization system
include(cmake/FeatureOptimization.cmake)

# Configure toolchain if auto-detection is enabled
if(PICO_RTOS_AUTO_DETECT_TOOLCHAIN AND NOT CMAKE_TOOLCHAIN_FILE)
    pico_rtos_configure_toolchain()
endif()

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

# Validate and print toolchain information
if(PICO_RTOS_AUTO_DETECT_TOOLCHAIN)
    pico_rtos_validate_toolchain()
    pico_rtos_print_toolchain_info()
endif()

# Configure features based on profile
pico_rtos_configure_features()

# Pico-RTOS v0.3.1 - Bug fix release
set(PICO_RTOS_VERSION_MAJOR 0)
set(PICO_RTOS_VERSION_MINOR 3)
set(PICO_RTOS_VERSION_PATCH 1)
set(PICO_RTOS_VERSION_STRING "${PICO_RTOS_VERSION_MAJOR}.${PICO_RTOS_VERSION_MINOR}.${PICO_RTOS_VERSION_PATCH}")

# Add version information to compile definitions
add_compile_definitions(
    PICO_RTOS_VERSION_MAJOR=${PICO_RTOS_VERSION_MAJOR}
    PICO_RTOS_VERSION_MINOR=${PICO_RTOS_VERSION_MINOR}
    PICO_RTOS_VERSION_PATCH=${PICO_RTOS_VERSION_PATCH}
    PICO_RTOS_VERSION_STRING="${PICO_RTOS_VERSION_STRING}"
)

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

# v0.3.0 Advanced Synchronization Primitives
option(PICO_RTOS_ENABLE_EVENT_GROUPS "Enable event groups" ON)
set(PICO_RTOS_EVENT_GROUPS_MAX_COUNT "8" CACHE STRING "Maximum number of event groups")
option(PICO_RTOS_ENABLE_STREAM_BUFFERS "Enable stream buffers" ON)
set(PICO_RTOS_STREAM_BUFFERS_MAX_COUNT "4" CACHE STRING "Maximum number of stream buffers")
set(PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD "256" CACHE STRING "Zero-copy threshold in bytes")

# v0.3.0 Enhanced Memory Management
option(PICO_RTOS_ENABLE_MEMORY_POOLS "Enable memory pools" ON)
set(PICO_RTOS_MEMORY_POOLS_MAX_COUNT "4" CACHE STRING "Maximum number of memory pools")
option(PICO_RTOS_MEMORY_POOL_STATISTICS "Enable memory pool statistics" ON)
option(PICO_RTOS_ENABLE_MPU_SUPPORT "Enable Memory Protection Unit support" OFF)
set(PICO_RTOS_MPU_REGIONS_MAX "4" CACHE STRING "Maximum MPU regions")

# v0.3.0 Debugging and Profiling Tools
option(PICO_RTOS_ENABLE_TASK_INSPECTION "Enable runtime task inspection" ON)
option(PICO_RTOS_ENABLE_EXECUTION_PROFILING "Enable execution time profiling" OFF)
set(PICO_RTOS_PROFILING_MAX_ENTRIES "64" CACHE STRING "Maximum profiling entries")
option(PICO_RTOS_ENABLE_SYSTEM_TRACING "Enable system event tracing" OFF)
set(PICO_RTOS_TRACE_BUFFER_SIZE "256" CACHE STRING "Trace buffer size in events")
option(PICO_RTOS_TRACE_OVERFLOW_WRAP "Trace buffer wrap-around behavior" ON)
option(PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS "Enable enhanced assertion handling" ON)
option(PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE "Enable configurable assertion handlers" OFF)

# v0.3.0 Multi-Core Support
option(PICO_RTOS_ENABLE_MULTI_CORE "Enable multi-core support" ON)
option(PICO_RTOS_ENABLE_LOAD_BALANCING "Enable automatic load balancing" ON)
set(PICO_RTOS_LOAD_BALANCE_THRESHOLD "75" CACHE STRING "Load balancing threshold percentage")
option(PICO_RTOS_ENABLE_CORE_AFFINITY "Enable task core affinity" ON)
option(PICO_RTOS_ENABLE_IPC_CHANNELS "Enable inter-core communication channels" ON)
set(PICO_RTOS_IPC_CHANNEL_BUFFER_SIZE "512" CACHE STRING "IPC channel buffer size in bytes")
option(PICO_RTOS_ENABLE_CORE_FAILURE_DETECTION "Enable core failure detection" ON)

# v0.3.0 Advanced System Extensions
option(PICO_RTOS_ENABLE_IO_ABSTRACTION "Enable thread-safe I/O abstraction" OFF)
set(PICO_RTOS_IO_DEVICES_MAX_COUNT "4" CACHE STRING "Maximum I/O devices")
option(PICO_RTOS_ENABLE_HIRES_TIMERS "Enable high-resolution software timers" OFF)
set(PICO_RTOS_HIRES_TIMERS_MAX_COUNT "8" CACHE STRING "Maximum high-resolution timers")
option(PICO_RTOS_ENABLE_UNIVERSAL_TIMEOUTS "Enable universal timeout support" ON)
option(PICO_RTOS_ENABLE_ENHANCED_LOGGING "Enable enhanced multi-level logging" OFF)
set(PICO_RTOS_ENHANCED_LOG_LEVELS "6" CACHE STRING "Number of enhanced log levels")

# v0.3.0 Production Quality Assurance
option(PICO_RTOS_ENABLE_DEADLOCK_DETECTION "Enable deadlock detection" OFF)
set(PICO_RTOS_DEADLOCK_DETECTION_MAX_RESOURCES "16" CACHE STRING "Maximum tracked resources for deadlock detection")
option(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING "Enable system health monitoring" ON)
set(PICO_RTOS_HEALTH_MONITORING_INTERVAL_MS "1000" CACHE STRING "Health monitoring interval in ms")
option(PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION "Enable hardware watchdog integration" ON)
set(PICO_RTOS_WATCHDOG_TIMEOUT_MS "5000" CACHE STRING "Watchdog timeout in ms")
option(PICO_RTOS_ENABLE_ALERT_SYSTEM "Enable configurable alert and notification system" OFF)
set(PICO_RTOS_ALERT_THRESHOLDS_MAX "8" CACHE STRING "Maximum alert thresholds")

# v0.3.0 Backward Compatibility
option(PICO_RTOS_ENABLE_BACKWARD_COMPATIBILITY "Enable v0.2.1 backward compatibility mode" ON)
option(PICO_RTOS_ENABLE_MIGRATION_WARNINGS "Enable migration warnings for deprecated features" ON)

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

# v0.3.0 Configuration validation and dependencies
if(PICO_RTOS_ENABLE_MULTI_CORE AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
    message(WARNING "Multi-core support requires memory tracking for optimal operation and debugging")
endif()

if(PICO_RTOS_ENABLE_MULTI_CORE AND NOT PICO_RTOS_ENABLE_RUNTIME_STATS)
    message(WARNING "Multi-core support requires runtime statistics for load balancing and monitoring")
endif()

if(PICO_RTOS_ENABLE_MEMORY_POOLS AND PICO_RTOS_MEMORY_POOL_STATISTICS AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
    message(WARNING "Memory pool statistics require memory tracking for accurate reporting")
endif()

# Execution profiling uses RP2040 hardware timers which are always available

if(PICO_RTOS_ENABLE_SYSTEM_TRACING AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
    message(WARNING "System tracing works best with memory tracking enabled")
endif()

if(PICO_RTOS_ENABLE_DEADLOCK_DETECTION AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
    message(WARNING "Deadlock detection requires memory tracking for comprehensive resource tracking")
endif()

if(PICO_RTOS_ENABLE_DEADLOCK_DETECTION AND NOT PICO_RTOS_ENABLE_RUNTIME_STATS)
    message(WARNING "Deadlock detection requires runtime statistics for comprehensive resource tracking")
endif()

# High-resolution timers use RP2040 hardware timers which are always available

if(PICO_RTOS_ENABLE_ENHANCED_LOGGING AND NOT PICO_RTOS_ENABLE_LOGGING)
    message(FATAL_ERROR "Enhanced logging requires the basic logging system to be enabled")
endif()

if(PICO_RTOS_ENABLE_MPU_SUPPORT AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
    message(WARNING "MPU support works best with memory tracking enabled")
endif()

if(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
    message(WARNING "System health monitoring requires memory tracking for comprehensive metrics")
endif()

if(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING AND NOT PICO_RTOS_ENABLE_RUNTIME_STATS)
    message(WARNING "System health monitoring requires runtime statistics for comprehensive metrics")
endif()

if(PICO_RTOS_ENABLE_ALERT_SYSTEM AND NOT PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING)
    message(FATAL_ERROR "Alert system requires system health monitoring to provide meaningful alerts")
endif()

# Resource limit validations
if(PICO_RTOS_EVENT_GROUPS_MAX_COUNT GREATER 16)
    message(WARNING "High event group count (${PICO_RTOS_EVENT_GROUPS_MAX_COUNT}) may impact performance")
endif()

if(PICO_RTOS_STREAM_BUFFERS_MAX_COUNT GREATER 8)
    message(WARNING "High stream buffer count (${PICO_RTOS_STREAM_BUFFERS_MAX_COUNT}) may impact performance")
endif()

if(PICO_RTOS_MEMORY_POOLS_MAX_COUNT GREATER 8)
    message(WARNING "High memory pool count (${PICO_RTOS_MEMORY_POOLS_MAX_COUNT}) may impact performance")
endif()

# Performance impact warnings
if(PICO_RTOS_ENABLE_EXECUTION_PROFILING AND PICO_RTOS_ENABLE_SYSTEM_TRACING AND PICO_RTOS_ENABLE_DEADLOCK_DETECTION)
    message(WARNING "Multiple debugging features enabled simultaneously may impact real-time performance")
endif()

# Validate numeric parameters
if(NOT PICO_RTOS_LOAD_BALANCE_THRESHOLD MATCHES "^[0-9]+$" OR PICO_RTOS_LOAD_BALANCE_THRESHOLD LESS 10 OR PICO_RTOS_LOAD_BALANCE_THRESHOLD GREATER 90)
    message(FATAL_ERROR "PICO_RTOS_LOAD_BALANCE_THRESHOLD must be between 10 and 90")
endif()

if(NOT PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD MATCHES "^[0-9]+$" OR PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD LESS 64)
    message(FATAL_ERROR "PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD must be at least 64 bytes")
endif()

if(NOT PICO_RTOS_HEALTH_MONITORING_INTERVAL_MS MATCHES "^[0-9]+$" OR PICO_RTOS_HEALTH_MONITORING_INTERVAL_MS LESS 100)
    message(FATAL_ERROR "PICO_RTOS_HEALTH_MONITORING_INTERVAL_MS must be at least 100ms")
endif()

if(NOT PICO_RTOS_WATCHDOG_TIMEOUT_MS MATCHES "^[0-9]+$" OR PICO_RTOS_WATCHDOG_TIMEOUT_MS LESS 1000)
    message(FATAL_ERROR "PICO_RTOS_WATCHDOG_TIMEOUT_MS must be at least 1000ms")
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

# v0.3.0 feature compile definitions
add_compile_definitions(
    PICO_RTOS_EVENT_GROUPS_MAX_COUNT=${PICO_RTOS_EVENT_GROUPS_MAX_COUNT}
    PICO_RTOS_STREAM_BUFFERS_MAX_COUNT=${PICO_RTOS_STREAM_BUFFERS_MAX_COUNT}
    PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD=${PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD}
    PICO_RTOS_MEMORY_POOLS_MAX_COUNT=${PICO_RTOS_MEMORY_POOLS_MAX_COUNT}
    PICO_RTOS_MPU_REGIONS_MAX=${PICO_RTOS_MPU_REGIONS_MAX}
    PICO_RTOS_PROFILING_MAX_ENTRIES=${PICO_RTOS_PROFILING_MAX_ENTRIES}
    PICO_RTOS_TRACE_BUFFER_SIZE=${PICO_RTOS_TRACE_BUFFER_SIZE}
    PICO_RTOS_LOAD_BALANCE_THRESHOLD=${PICO_RTOS_LOAD_BALANCE_THRESHOLD}
    PICO_RTOS_IPC_CHANNEL_BUFFER_SIZE=${PICO_RTOS_IPC_CHANNEL_BUFFER_SIZE}
    PICO_RTOS_IO_DEVICES_MAX_COUNT=${PICO_RTOS_IO_DEVICES_MAX_COUNT}
    PICO_RTOS_HIRES_TIMERS_MAX_COUNT=${PICO_RTOS_HIRES_TIMERS_MAX_COUNT}
    PICO_RTOS_ENHANCED_LOG_LEVELS=${PICO_RTOS_ENHANCED_LOG_LEVELS}
    PICO_RTOS_DEADLOCK_DETECTION_MAX_RESOURCES=${PICO_RTOS_DEADLOCK_DETECTION_MAX_RESOURCES}
    PICO_RTOS_HEALTH_MONITORING_INTERVAL_MS=${PICO_RTOS_HEALTH_MONITORING_INTERVAL_MS}
    PICO_RTOS_WATCHDOG_TIMEOUT_MS=${PICO_RTOS_WATCHDOG_TIMEOUT_MS}
    PICO_RTOS_ALERT_THRESHOLDS_MAX=${PICO_RTOS_ALERT_THRESHOLDS_MAX}
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

# v0.3.0 Advanced Synchronization Primitives
if(PICO_RTOS_ENABLE_EVENT_GROUPS)
    add_compile_definitions(PICO_RTOS_ENABLE_EVENT_GROUPS=1)
endif()

if(PICO_RTOS_ENABLE_STREAM_BUFFERS)
    add_compile_definitions(PICO_RTOS_ENABLE_STREAM_BUFFERS=1)
endif()

# v0.3.0 Enhanced Memory Management
if(PICO_RTOS_ENABLE_MEMORY_POOLS)
    add_compile_definitions(PICO_RTOS_ENABLE_MEMORY_POOLS=1)
endif()

if(PICO_RTOS_MEMORY_POOL_STATISTICS)
    add_compile_definitions(PICO_RTOS_MEMORY_POOL_STATISTICS=1)
endif()

if(PICO_RTOS_ENABLE_MPU_SUPPORT)
    add_compile_definitions(PICO_RTOS_ENABLE_MPU_SUPPORT=1)
endif()

# v0.3.0 Debugging and Profiling Tools
if(PICO_RTOS_ENABLE_TASK_INSPECTION)
    add_compile_definitions(PICO_RTOS_ENABLE_TASK_INSPECTION=1)
endif()

if(PICO_RTOS_ENABLE_EXECUTION_PROFILING)
    add_compile_definitions(PICO_RTOS_ENABLE_EXECUTION_PROFILING=1)
endif()

if(PICO_RTOS_ENABLE_SYSTEM_TRACING)
    add_compile_definitions(PICO_RTOS_ENABLE_SYSTEM_TRACING=1)
endif()

if(PICO_RTOS_TRACE_OVERFLOW_WRAP)
    add_compile_definitions(PICO_RTOS_TRACE_OVERFLOW_WRAP=1)
endif()

if(PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS)
    add_compile_definitions(PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS=1)
endif()

if(PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE)
    add_compile_definitions(PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE=1)
endif()

# v0.3.0 Multi-Core Support
if(PICO_RTOS_ENABLE_MULTI_CORE)
    add_compile_definitions(PICO_RTOS_ENABLE_MULTI_CORE=1)
endif()

if(PICO_RTOS_ENABLE_LOAD_BALANCING)
    add_compile_definitions(PICO_RTOS_ENABLE_LOAD_BALANCING=1)
endif()

if(PICO_RTOS_ENABLE_CORE_AFFINITY)
    add_compile_definitions(PICO_RTOS_ENABLE_CORE_AFFINITY=1)
endif()

if(PICO_RTOS_ENABLE_IPC_CHANNELS)
    add_compile_definitions(PICO_RTOS_ENABLE_IPC_CHANNELS=1)
endif()

if(PICO_RTOS_ENABLE_CORE_FAILURE_DETECTION)
    add_compile_definitions(PICO_RTOS_ENABLE_CORE_FAILURE_DETECTION=1)
endif()

# v0.3.0 Advanced System Extensions
if(PICO_RTOS_ENABLE_IO_ABSTRACTION)
    add_compile_definitions(PICO_RTOS_ENABLE_IO_ABSTRACTION=1)
endif()

if(PICO_RTOS_ENABLE_HIRES_TIMERS)
    add_compile_definitions(PICO_RTOS_ENABLE_HIRES_TIMERS=1)
endif()

if(PICO_RTOS_ENABLE_UNIVERSAL_TIMEOUTS)
    add_compile_definitions(PICO_RTOS_ENABLE_UNIVERSAL_TIMEOUTS=1)
endif()

if(PICO_RTOS_ENABLE_ENHANCED_LOGGING)
    add_compile_definitions(PICO_RTOS_ENABLE_ENHANCED_LOGGING=1)
endif()

# v0.3.0 Production Quality Assurance
if(PICO_RTOS_ENABLE_DEADLOCK_DETECTION)
    add_compile_definitions(PICO_RTOS_ENABLE_DEADLOCK_DETECTION=1)
endif()

if(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING)
    add_compile_definitions(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING=1)
endif()

if(PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION)
    add_compile_definitions(PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION=1)
endif()

if(PICO_RTOS_ENABLE_ALERT_SYSTEM)
    add_compile_definitions(PICO_RTOS_ENABLE_ALERT_SYSTEM=1)
endif()

# v0.3.0 Backward Compatibility
if(PICO_RTOS_ENABLE_BACKWARD_COMPATIBILITY)
    add_compile_definitions(PICO_RTOS_ENABLE_BACKWARD_COMPATIBILITY=1)
endif()

if(PICO_RTOS_ENABLE_MIGRATION_WARNINGS)
    add_compile_definitions(PICO_RTOS_ENABLE_MIGRATION_WARNINGS=1)
endif()

# Create the RTOS library
set(PICO_RTOS_SOURCES
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
    src/deprecation.c
)

# v0.3.0 Conditional source files (will be created in future tasks)
# Note: These files will be added as they are implemented in subsequent tasks

# Helper function to conditionally add source files if they exist
function(add_source_if_exists source_list source_file)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        list(APPEND ${source_list} ${source_file})
        set(${source_list} ${${source_list}} PARENT_SCOPE)
    endif()
endfunction()

if(PICO_RTOS_ENABLE_EVENT_GROUPS)
    add_source_if_exists(PICO_RTOS_SOURCES src/event_group.c)
endif()

if(PICO_RTOS_ENABLE_STREAM_BUFFERS)
    add_source_if_exists(PICO_RTOS_SOURCES src/stream_buffer.c)
endif()

if(PICO_RTOS_ENABLE_MEMORY_POOLS)
    add_source_if_exists(PICO_RTOS_SOURCES src/memory_pool.c)
endif()

if(PICO_RTOS_ENABLE_MPU_SUPPORT)
    add_source_if_exists(PICO_RTOS_SOURCES src/mpu.c)
endif()

if(PICO_RTOS_ENABLE_TASK_INSPECTION OR PICO_RTOS_ENABLE_EXECUTION_PROFILING OR PICO_RTOS_ENABLE_SYSTEM_TRACING)
    add_source_if_exists(PICO_RTOS_SOURCES src/debug.c)
endif()

if(PICO_RTOS_ENABLE_EXECUTION_PROFILING)
    add_source_if_exists(PICO_RTOS_SOURCES src/profiler.c)
endif()

if(PICO_RTOS_ENABLE_SYSTEM_TRACING)
    add_source_if_exists(PICO_RTOS_SOURCES src/trace.c)
endif()

if(PICO_RTOS_ENABLE_MULTI_CORE)
    add_source_if_exists(PICO_RTOS_SOURCES src/smp.c)
endif()

if(PICO_RTOS_ENABLE_IPC_CHANNELS)
    add_source_if_exists(PICO_RTOS_SOURCES src/ipc.c)
endif()

if(PICO_RTOS_ENABLE_IO_ABSTRACTION)
    add_source_if_exists(PICO_RTOS_SOURCES src/io.c)
endif()

if(PICO_RTOS_ENABLE_HIRES_TIMERS)
    add_source_if_exists(PICO_RTOS_SOURCES src/hires_timer.c)
endif()

if(PICO_RTOS_ENABLE_DEADLOCK_DETECTION)
    add_source_if_exists(PICO_RTOS_SOURCES src/deadlock.c)
endif()

if(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING)
    add_source_if_exists(PICO_RTOS_SOURCES src/health_monitor.c)
endif()

if(PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION)
    add_source_if_exists(PICO_RTOS_SOURCES src/watchdog.c)
endif()

if(PICO_RTOS_ENABLE_ALERT_SYSTEM)
    add_source_if_exists(PICO_RTOS_SOURCES src/alert.c)
endif()

add_library(pico_rtos STATIC ${PICO_RTOS_SOURCES})

# Set up include directories - use generator expressions to handle install correctly
target_include_directories(pico_rtos PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Apply toolchain-specific flags
if(PICO_RTOS_AUTO_DETECT_TOOLCHAIN)
    pico_rtos_apply_toolchain_flags(pico_rtos)
endif()

# Apply size optimizations if requested
if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel" OR PICO_RTOS_OPTIMIZE_SIZE)
    pico_rtos_apply_size_optimizations(pico_rtos)
endif()

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
    add_subdirectory(examples/event_group_coordination)
    add_subdirectory(examples/stream_buffer_data_streaming)
    add_subdirectory(examples/multicore_load_balancing)
    add_subdirectory(examples/debugging_profiling_analysis)
endif()

# Build tests if enabled
if(PICO_RTOS_BUILD_TESTS)
    # Include comprehensive test suite
    include(tests/CMakeLists_comprehensive.txt)
    
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
    
    # v0.3.0 Debug System Tests
    if(PICO_RTOS_ENABLE_TASK_INSPECTION)
        add_executable(debug_test tests/debug_test.c)
        target_link_libraries(debug_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(debug_test)
        pico_enable_stdio_usb(debug_test 1)
        pico_enable_stdio_uart(debug_test 0)
    endif()
    
    # v0.3.0 Profiler Tests
    if(PICO_RTOS_ENABLE_EXECUTION_PROFILING)
        add_executable(profiler_test tests/profiler_test.c)
        target_link_libraries(profiler_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(profiler_test)
        pico_enable_stdio_usb(profiler_test 1)
        pico_enable_stdio_uart(profiler_test 0)
    endif()
    
    # v0.3.0 Trace System Tests
    if(PICO_RTOS_ENABLE_SYSTEM_TRACING)
        add_executable(trace_test tests/trace_test.c)
        target_link_libraries(trace_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(trace_test)
        pico_enable_stdio_usb(trace_test 1)
        pico_enable_stdio_uart(trace_test 0)
    endif()
    
    # v0.3.0 Assertion System Tests
    if(PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS)
        add_executable(assertion_test tests/assertion_test.c)
        target_link_libraries(assertion_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(assertion_test)
        pico_enable_stdio_usb(assertion_test 1)
        pico_enable_stdio_uart(assertion_test 0)
    endif()
    
    # v0.3.0 Event Group Tests
    if(PICO_RTOS_ENABLE_EVENT_GROUPS)
        add_executable(event_group_test tests/event_group_test.c)
        target_link_libraries(event_group_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(event_group_test)
        pico_enable_stdio_usb(event_group_test 1)
        pico_enable_stdio_uart(event_group_test 0)
    endif()
    
    # v0.3.0 Stream Buffer Tests
    if(PICO_RTOS_ENABLE_STREAM_BUFFERS)
        add_executable(stream_buffer_test tests/stream_buffer_test.c)
        target_link_libraries(stream_buffer_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(stream_buffer_test)
        pico_enable_stdio_usb(stream_buffer_test 1)
        pico_enable_stdio_uart(stream_buffer_test 0)
        
        add_executable(stream_buffer_concurrent_test tests/stream_buffer_concurrent_test.c)
        target_link_libraries(stream_buffer_concurrent_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(stream_buffer_concurrent_test)
        pico_enable_stdio_usb(stream_buffer_concurrent_test 1)
        pico_enable_stdio_uart(stream_buffer_concurrent_test 0)
        
        add_executable(stream_buffer_performance_test tests/stream_buffer_performance_test.c)
        target_link_libraries(stream_buffer_performance_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(stream_buffer_performance_test)
        pico_enable_stdio_usb(stream_buffer_performance_test 1)
        pico_enable_stdio_uart(stream_buffer_performance_test 0)
    endif()
    
    # v0.3.0 Memory Pool Tests
    if(PICO_RTOS_ENABLE_MEMORY_POOLS)
        add_executable(memory_pool_test tests/memory_pool_test.c)
        target_link_libraries(memory_pool_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(memory_pool_test)
        pico_enable_stdio_usb(memory_pool_test 1)
        pico_enable_stdio_uart(memory_pool_test 0)
        
        add_executable(memory_pool_performance_test tests/memory_pool_performance_test.c)
        target_link_libraries(memory_pool_performance_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(memory_pool_performance_test)
        pico_enable_stdio_usb(memory_pool_performance_test 1)
        pico_enable_stdio_uart(memory_pool_performance_test 0)
        
        add_executable(memory_pool_statistics_test tests/memory_pool_statistics_test.c)
        target_link_libraries(memory_pool_statistics_test pico_rtos pico_stdlib)
        pico_add_extra_outputs(memory_pool_statistics_test)
        pico_enable_stdio_usb(memory_pool_statistics_test 1)
        pico_enable_stdio_uart(memory_pool_statistics_test 0)
    endif()
    
    # v0.3.0 SMP Tests
    if(PICO_RTOS_ENABLE_MULTI_CORE)
        add_executable(smp_test tests/smp_test.c)
        target_link_libraries(smp_test pico_rtos pico_stdlib pico_multicore)
        pico_add_extra_outputs(smp_test)
        pico_enable_stdio_usb(smp_test 1)
        pico_enable_stdio_uart(smp_test 0)
        
        add_executable(inter_core_sync_test tests/inter_core_sync_test.c)
        target_link_libraries(inter_core_sync_test pico_rtos pico_stdlib pico_multicore)
        pico_add_extra_outputs(inter_core_sync_test)
        pico_enable_stdio_usb(inter_core_sync_test 1)
        pico_enable_stdio_uart(inter_core_sync_test 0)
    endif()
    
    # v0.3.0 Backward Compatibility Tests
    add_executable(compatibility_test tests/compatibility_test.c)
    target_link_libraries(compatibility_test pico_rtos pico_stdlib)
    pico_add_extra_outputs(compatibility_test)
    pico_enable_stdio_usb(compatibility_test 1)
    pico_enable_stdio_uart(compatibility_test 0)
    
    add_executable(deprecation_test tests/deprecation_test.c)
    target_link_libraries(deprecation_test pico_rtos pico_stdlib)
    pico_add_extra_outputs(deprecation_test)
    pico_enable_stdio_usb(deprecation_test 1)
    pico_enable_stdio_uart(deprecation_test 0)

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
message(STATUS "v0.3.0 Advanced Synchronization:")
message(STATUS "  Event groups: ${PICO_RTOS_ENABLE_EVENT_GROUPS} (max: ${PICO_RTOS_EVENT_GROUPS_MAX_COUNT})")
message(STATUS "  Stream buffers: ${PICO_RTOS_ENABLE_STREAM_BUFFERS} (max: ${PICO_RTOS_STREAM_BUFFERS_MAX_COUNT})")
message(STATUS "")
message(STATUS "v0.3.0 Enhanced Memory Management:")
message(STATUS "  Memory pools: ${PICO_RTOS_ENABLE_MEMORY_POOLS} (max: ${PICO_RTOS_MEMORY_POOLS_MAX_COUNT})")
message(STATUS "  Pool statistics: ${PICO_RTOS_MEMORY_POOL_STATISTICS}")
message(STATUS "  MPU support: ${PICO_RTOS_ENABLE_MPU_SUPPORT}")
message(STATUS "")
message(STATUS "v0.3.0 Debugging and Profiling:")
message(STATUS "  Task inspection: ${PICO_RTOS_ENABLE_TASK_INSPECTION}")
message(STATUS "  Execution profiling: ${PICO_RTOS_ENABLE_EXECUTION_PROFILING}")
message(STATUS "  System tracing: ${PICO_RTOS_ENABLE_SYSTEM_TRACING}")
message(STATUS "  Enhanced assertions: ${PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS}")
message(STATUS "")
message(STATUS "v0.3.0 Multi-Core Support:")
message(STATUS "  Multi-core: ${PICO_RTOS_ENABLE_MULTI_CORE}")
message(STATUS "  Load balancing: ${PICO_RTOS_ENABLE_LOAD_BALANCING}")
message(STATUS "  Core affinity: ${PICO_RTOS_ENABLE_CORE_AFFINITY}")
message(STATUS "  IPC channels: ${PICO_RTOS_ENABLE_IPC_CHANNELS}")
message(STATUS "")
message(STATUS "v0.3.0 System Extensions:")
message(STATUS "  I/O abstraction: ${PICO_RTOS_ENABLE_IO_ABSTRACTION}")
message(STATUS "  High-res timers: ${PICO_RTOS_ENABLE_HIRES_TIMERS}")
message(STATUS "  Universal timeouts: ${PICO_RTOS_ENABLE_UNIVERSAL_TIMEOUTS}")
message(STATUS "")
message(STATUS "v0.3.0 Quality Assurance:")
message(STATUS "  Deadlock detection: ${PICO_RTOS_ENABLE_DEADLOCK_DETECTION}")
message(STATUS "  Health monitoring: ${PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING}")
message(STATUS "  Watchdog integration: ${PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION}")
message(STATUS "  Alert system: ${PICO_RTOS_ENABLE_ALERT_SYSTEM}")
message(STATUS "")
message(STATUS "v0.3.0 Compatibility:")
message(STATUS "  Backward compatibility: ${PICO_RTOS_ENABLE_BACKWARD_COMPATIBILITY}")
message(STATUS "  Migration warnings: ${PICO_RTOS_ENABLE_MIGRATION_WARNINGS}")
message(STATUS "")
message(STATUS "Debug and Logging Configuration:")
message(STATUS "  Debug logging: ${PICO_RTOS_ENABLE_LOGGING}")
if(PICO_RTOS_ENABLE_LOGGING)
    message(STATUS "  Log level: ${PICO_RTOS_LOG_LEVEL} (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)")
    message(STATUS "  Max message length: ${PICO_RTOS_LOG_MESSAGE_MAX_LENGTH}")
    message(STATUS "  Subsystem mask: ${PICO_RTOS_LOG_SUBSYSTEM_MASK}")
    message(STATUS "  Enhanced logging: ${PICO_RTOS_ENABLE_ENHANCED_LOGGING}")
endif()
message(STATUS "")

# Add option to show configuration help
option(PICO_RTOS_SHOW_CONFIG_HELP "Show configuration options help" OFF)
if(PICO_RTOS_SHOW_CONFIG_HELP)
    pico_rtos_print_config_help()
endif()