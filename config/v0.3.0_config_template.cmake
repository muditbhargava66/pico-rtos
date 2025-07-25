# Pico-RTOS v0.3.0 Configuration Template
# This file provides a template for configuring v0.3.0 features
# Copy and modify this file to customize your build configuration

# =============================================================================
# v0.3.0 Advanced Synchronization Primitives
# =============================================================================

# Event Groups - Advanced task synchronization
set(PICO_RTOS_ENABLE_EVENT_GROUPS ON CACHE BOOL "Enable event groups")
set(PICO_RTOS_EVENT_GROUPS_MAX_COUNT 8 CACHE STRING "Maximum number of event groups")

# Stream Buffers - Efficient message passing
set(PICO_RTOS_ENABLE_STREAM_BUFFERS ON CACHE BOOL "Enable stream buffers")
set(PICO_RTOS_STREAM_BUFFERS_MAX_COUNT 4 CACHE STRING "Maximum number of stream buffers")
set(PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD 256 CACHE STRING "Zero-copy threshold in bytes")

# =============================================================================
# v0.3.0 Enhanced Memory Management
# =============================================================================

# Memory Pools - O(1) fixed-size allocation
set(PICO_RTOS_ENABLE_MEMORY_POOLS ON CACHE BOOL "Enable memory pools")
set(PICO_RTOS_MEMORY_POOLS_MAX_COUNT 4 CACHE STRING "Maximum number of memory pools")
set(PICO_RTOS_MEMORY_POOL_STATISTICS ON CACHE BOOL "Enable memory pool statistics")

# Memory Protection Unit (MPU) support
set(PICO_RTOS_ENABLE_MPU_SUPPORT OFF CACHE BOOL "Enable MPU support")
set(PICO_RTOS_MPU_REGIONS_MAX 4 CACHE STRING "Maximum MPU regions")

# =============================================================================
# v0.3.0 Debugging and Profiling Tools
# =============================================================================

# Task Inspection - Runtime debugging
set(PICO_RTOS_ENABLE_TASK_INSPECTION ON CACHE BOOL "Enable runtime task inspection")

# Execution Profiling - Performance analysis
set(PICO_RTOS_ENABLE_EXECUTION_PROFILING OFF CACHE BOOL "Enable execution time profiling")
set(PICO_RTOS_PROFILING_MAX_ENTRIES 64 CACHE STRING "Maximum profiling entries")

# System Tracing - Event capture
set(PICO_RTOS_ENABLE_SYSTEM_TRACING OFF CACHE BOOL "Enable system event tracing")
set(PICO_RTOS_TRACE_BUFFER_SIZE 256 CACHE STRING "Trace buffer size in events")
set(PICO_RTOS_TRACE_OVERFLOW_WRAP ON CACHE BOOL "Trace buffer wrap-around behavior")

# Enhanced Assertions - Better error handling
set(PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS ON CACHE BOOL "Enable enhanced assertion handling")
set(PICO_RTOS_ASSERTION_HANDLER_CONFIGURABLE OFF CACHE BOOL "Enable configurable assertion handlers")

# =============================================================================
# v0.3.0 Multi-Core Support
# =============================================================================

# Multi-Core Support - Dual-core RP2040 utilization
set(PICO_RTOS_ENABLE_MULTI_CORE ON CACHE BOOL "Enable multi-core support")
set(PICO_RTOS_ENABLE_LOAD_BALANCING ON CACHE BOOL "Enable automatic load balancing")
set(PICO_RTOS_LOAD_BALANCE_THRESHOLD 75 CACHE STRING "Load balancing threshold percentage")

# Core Affinity - Task-to-core binding
set(PICO_RTOS_ENABLE_CORE_AFFINITY ON CACHE BOOL "Enable task core affinity")

# Inter-Core Communication - IPC channels
set(PICO_RTOS_ENABLE_IPC_CHANNELS ON CACHE BOOL "Enable inter-core communication channels")
set(PICO_RTOS_IPC_CHANNEL_BUFFER_SIZE 512 CACHE STRING "IPC channel buffer size in bytes")

# Core Failure Detection - Reliability
set(PICO_RTOS_ENABLE_CORE_FAILURE_DETECTION ON CACHE BOOL "Enable core failure detection")

# =============================================================================
# v0.3.0 Advanced System Extensions
# =============================================================================

# I/O Abstraction - Thread-safe I/O
set(PICO_RTOS_ENABLE_IO_ABSTRACTION OFF CACHE BOOL "Enable thread-safe I/O abstraction")
set(PICO_RTOS_IO_DEVICES_MAX_COUNT 4 CACHE STRING "Maximum I/O devices")

# High-Resolution Timers - Microsecond precision
set(PICO_RTOS_ENABLE_HIRES_TIMERS OFF CACHE BOOL "Enable high-resolution software timers")
set(PICO_RTOS_HIRES_TIMERS_MAX_COUNT 8 CACHE STRING "Maximum high-resolution timers")

# Universal Timeouts - Consistent timeout handling
set(PICO_RTOS_ENABLE_UNIVERSAL_TIMEOUTS ON CACHE BOOL "Enable universal timeout support")

# Enhanced Logging - Advanced logging features
set(PICO_RTOS_ENABLE_ENHANCED_LOGGING OFF CACHE BOOL "Enable enhanced multi-level logging")
set(PICO_RTOS_ENHANCED_LOG_LEVELS 6 CACHE STRING "Number of enhanced log levels")

# =============================================================================
# v0.3.0 Production Quality Assurance
# =============================================================================

# Deadlock Detection - Resource cycle detection
set(PICO_RTOS_ENABLE_DEADLOCK_DETECTION OFF CACHE BOOL "Enable deadlock detection")
set(PICO_RTOS_DEADLOCK_DETECTION_MAX_RESOURCES 16 CACHE STRING "Maximum tracked resources for deadlock detection")

# System Health Monitoring - Comprehensive metrics
set(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING ON CACHE BOOL "Enable system health monitoring")
set(PICO_RTOS_HEALTH_MONITORING_INTERVAL_MS 1000 CACHE STRING "Health monitoring interval in ms")

# Watchdog Integration - Hardware reliability
set(PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION ON CACHE BOOL "Enable hardware watchdog integration")
set(PICO_RTOS_WATCHDOG_TIMEOUT_MS 5000 CACHE STRING "Watchdog timeout in ms")

# Alert System - Threshold-based notifications
set(PICO_RTOS_ENABLE_ALERT_SYSTEM OFF CACHE BOOL "Enable configurable alert and notification system")
set(PICO_RTOS_ALERT_THRESHOLDS_MAX 8 CACHE STRING "Maximum alert thresholds")

# =============================================================================
# v0.3.0 Backward Compatibility
# =============================================================================

# Backward Compatibility - v0.2.1 compatibility
set(PICO_RTOS_ENABLE_BACKWARD_COMPATIBILITY ON CACHE BOOL "Enable v0.2.1 backward compatibility mode")
set(PICO_RTOS_ENABLE_MIGRATION_WARNINGS ON CACHE BOOL "Enable migration warnings for deprecated features")

# =============================================================================
# Configuration Presets
# =============================================================================

# Minimal Configuration - Smallest footprint
function(pico_rtos_config_minimal)
    set(PICO_RTOS_ENABLE_EVENT_GROUPS OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_STREAM_BUFFERS OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_MEMORY_POOLS OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_MULTI_CORE OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_TASK_INSPECTION OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION OFF PARENT_SCOPE)
endfunction()

# Development Configuration - Full debugging features
function(pico_rtos_config_development)
    set(PICO_RTOS_ENABLE_EXECUTION_PROFILING ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_SYSTEM_TRACING ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_DEADLOCK_DETECTION ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_ENHANCED_LOGGING ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_ALERT_SYSTEM ON PARENT_SCOPE)
endfunction()

# Production Configuration - Optimized for deployment
function(pico_rtos_config_production)
    set(PICO_RTOS_ENABLE_EXECUTION_PROFILING OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_SYSTEM_TRACING OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_DEADLOCK_DETECTION OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_ENHANCED_LOGGING OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_ALERT_SYSTEM OFF PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_WATCHDOG_INTEGRATION ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_SYSTEM_HEALTH_MONITORING ON PARENT_SCOPE)
endfunction()

# High-Performance Configuration - Multi-core optimized
function(pico_rtos_config_high_performance)
    set(PICO_RTOS_ENABLE_MULTI_CORE ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_LOAD_BALANCING ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_CORE_AFFINITY ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_IPC_CHANNELS ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_HIRES_TIMERS ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_MEMORY_POOLS ON PARENT_SCOPE)
    set(PICO_RTOS_ENABLE_STREAM_BUFFERS ON PARENT_SCOPE)
endfunction()