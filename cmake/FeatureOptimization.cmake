# Pico-RTOS v0.3.0 Feature Optimization System
# Provides fine-grained control over feature compilation and size optimization

# Feature profiles for different use cases
set(PICO_RTOS_FEATURE_PROFILES
    "minimal"
    "basic"
    "standard"
    "full"
    "debug"
    "custom"
)

# Default profile
if(NOT DEFINED PICO_RTOS_FEATURE_PROFILE)
    set(PICO_RTOS_FEATURE_PROFILE "standard" CACHE STRING "Feature profile to use")
endif()

set_property(CACHE PICO_RTOS_FEATURE_PROFILE PROPERTY STRINGS ${PICO_RTOS_FEATURE_PROFILES})

# Feature categories and their dependencies
set(PICO_RTOS_CORE_FEATURES
    "TASK_MANAGEMENT"
    "SCHEDULER"
    "CONTEXT_SWITCHING"
    "BLOCKING_SYSTEM"
    "ERROR_HANDLING"
)

set(PICO_RTOS_SYNC_FEATURES
    "MUTEXES"
    "SEMAPHORES"
    "QUEUES"
    "EVENT_GROUPS"
    "STREAM_BUFFERS"
)

set(PICO_RTOS_MEMORY_FEATURES
    "MEMORY_TRACKING"
    "MEMORY_POOLS"
    "STACK_CHECKING"
    "MPU_SUPPORT"
)

set(PICO_RTOS_DEBUG_FEATURES
    "LOGGING"
    "TASK_INSPECTION"
    "EXECUTION_PROFILING"
    "SYSTEM_TRACING"
    "ENHANCED_ASSERTIONS"
)

set(PICO_RTOS_MULTICORE_FEATURES
    "SMP_SCHEDULER"
    "LOAD_BALANCING"
    "CORE_AFFINITY"
    "IPC_CHANNELS"
    "CORE_FAILURE_DETECTION"
)

set(PICO_RTOS_SYSTEM_FEATURES
    "TIMERS"
    "IO_ABSTRACTION"
    "HIRES_TIMERS"
    "UNIVERSAL_TIMEOUTS"
    "WATCHDOG_INTEGRATION"
)

set(PICO_RTOS_QUALITY_FEATURES
    "DEADLOCK_DETECTION"
    "HEALTH_MONITORING"
    "ALERT_SYSTEM"
    "RUNTIME_STATS"
)

# Feature dependencies
set(PICO_RTOS_FEATURE_DEPENDENCIES
    # Core dependencies (always required)
    "TASK_MANAGEMENT;SCHEDULER;CONTEXT_SWITCHING;BLOCKING_SYSTEM;ERROR_HANDLING"
    
    # Synchronization dependencies
    "EVENT_GROUPS;BLOCKING_SYSTEM"
    "STREAM_BUFFERS;BLOCKING_SYSTEM"
    
    # Memory dependencies
    "MEMORY_POOLS;MEMORY_TRACKING"
    "MPU_SUPPORT;MEMORY_TRACKING"
    
    # Debug dependencies
    "TASK_INSPECTION;MEMORY_TRACKING"
    "EXECUTION_PROFILING;MEMORY_TRACKING"
    "SYSTEM_TRACING;MEMORY_TRACKING"
    
    # Multi-core dependencies
    "SMP_SCHEDULER;TASK_MANAGEMENT"
    "LOAD_BALANCING;SMP_SCHEDULER;RUNTIME_STATS"
    "CORE_AFFINITY;SMP_SCHEDULER"
    "IPC_CHANNELS;SMP_SCHEDULER"
    "CORE_FAILURE_DETECTION;SMP_SCHEDULER;HEALTH_MONITORING"
    
    # System dependencies
    "HIRES_TIMERS;TIMERS"
    "UNIVERSAL_TIMEOUTS;TIMERS"
    
    # Quality dependencies
    "DEADLOCK_DETECTION;MEMORY_TRACKING;RUNTIME_STATS"
    "HEALTH_MONITORING;MEMORY_TRACKING;RUNTIME_STATS"
    "ALERT_SYSTEM;HEALTH_MONITORING"
)

# Function to apply feature profile
function(pico_rtos_apply_feature_profile profile)
    message(STATUS "Applying feature profile: ${profile}")
    
    # Reset all features to OFF first
    foreach(category CORE SYNC MEMORY DEBUG MULTICORE SYSTEM QUALITY)
        foreach(feature ${PICO_RTOS_${category}_FEATURES})
            set(PICO_RTOS_ENABLE_${feature} OFF PARENT_SCOPE)
        endforeach()
    endforeach()
    
    # Apply profile-specific settings
    if(profile STREQUAL "minimal")
        # Only core features
        foreach(feature ${PICO_RTOS_CORE_FEATURES})
            set(PICO_RTOS_ENABLE_${feature} ON PARENT_SCOPE)
        endforeach()
        
        # Basic synchronization
        set(PICO_RTOS_ENABLE_MUTEXES ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_SEMAPHORES ON PARENT_SCOPE)
        
        # Minimal memory features
        set(PICO_RTOS_ENABLE_STACK_CHECKING ON PARENT_SCOPE)
        
    elseif(profile STREQUAL "basic")
        # Core + basic sync + basic memory
        foreach(feature ${PICO_RTOS_CORE_FEATURES})
            set(PICO_RTOS_ENABLE_${feature} ON PARENT_SCOPE)
        endforeach()
        
        set(PICO_RTOS_ENABLE_MUTEXES ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_SEMAPHORES ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_QUEUES ON PARENT_SCOPE)
        
        set(PICO_RTOS_ENABLE_MEMORY_TRACKING ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_STACK_CHECKING ON PARENT_SCOPE)
        
        set(PICO_RTOS_ENABLE_TIMERS ON PARENT_SCOPE)
        
    elseif(profile STREQUAL "standard")
        # Core + most sync + memory + basic debug
        foreach(feature ${PICO_RTOS_CORE_FEATURES})
            set(PICO_RTOS_ENABLE_${feature} ON PARENT_SCOPE)
        endforeach()
        
        foreach(feature ${PICO_RTOS_SYNC_FEATURES})
            set(PICO_RTOS_ENABLE_${feature} ON PARENT_SCOPE)
        endforeach()
        
        set(PICO_RTOS_ENABLE_MEMORY_TRACKING ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_MEMORY_POOLS ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_STACK_CHECKING ON PARENT_SCOPE)
        
        set(PICO_RTOS_ENABLE_LOGGING ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_TASK_INSPECTION ON PARENT_SCOPE)
        
        set(PICO_RTOS_ENABLE_TIMERS ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_UNIVERSAL_TIMEOUTS ON PARENT_SCOPE)
        
        set(PICO_RTOS_ENABLE_RUNTIME_STATS ON PARENT_SCOPE)
        
    elseif(profile STREQUAL "full")
        # All features except debug-heavy ones
        foreach(category CORE SYNC MEMORY MULTICORE SYSTEM QUALITY)
            foreach(feature ${PICO_RTOS_${category}_FEATURES})
                set(PICO_RTOS_ENABLE_${feature} ON PARENT_SCOPE)
            endforeach()
        endforeach()
        
        # Enable basic debug features
        set(PICO_RTOS_ENABLE_LOGGING ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_TASK_INSPECTION ON PARENT_SCOPE)
        set(PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS ON PARENT_SCOPE)
        
    elseif(profile STREQUAL "debug")
        # Full features + all debug features
        foreach(category CORE SYNC MEMORY DEBUG MULTICORE SYSTEM QUALITY)
            foreach(feature ${PICO_RTOS_${category}_FEATURES})
                set(PICO_RTOS_ENABLE_${feature} ON PARENT_SCOPE)
            endforeach()
        endforeach()
        
    elseif(profile STREQUAL "custom")
        # Don't change anything - let user configure manually
        message(STATUS "Custom profile selected - manual configuration required")
    endif()
endfunction()

# Function to resolve feature dependencies
function(pico_rtos_resolve_dependencies)
    message(STATUS "Resolving feature dependencies...")
    
    set(changed TRUE)
    set(iterations 0)
    
    # Iteratively resolve dependencies until no changes
    while(changed AND iterations LESS 10)
        set(changed FALSE)
        math(EXPR iterations "${iterations} + 1")
        
        foreach(dep_rule ${PICO_RTOS_FEATURE_DEPENDENCIES})
            string(REPLACE ";" " " dep_rule_str "${dep_rule}")
            string(REGEX MATCHALL "[A-Z_]+" features "${dep_rule_str}")
            
            list(GET features 0 main_feature)
            list(REMOVE_AT features 0)
            
            # If main feature is enabled, enable its dependencies
            if(PICO_RTOS_ENABLE_${main_feature})
                foreach(dep_feature ${features})
                    if(NOT PICO_RTOS_ENABLE_${dep_feature})
                        message(STATUS "  Enabling ${dep_feature} (required by ${main_feature})")
                        set(PICO_RTOS_ENABLE_${dep_feature} ON PARENT_SCOPE)
                        set(changed TRUE)
                    endif()
                endforeach()
            endif()
        endforeach()
    endwhile()
    
    if(iterations EQUAL 10)
        message(WARNING "Dependency resolution may not be complete (max iterations reached)")
    endif()
endfunction()

# Function to calculate feature footprint
function(pico_rtos_calculate_footprint)
    message(STATUS "Calculating feature footprint...")
    
    # Estimated code size impact (in bytes) for each feature
    set(FEATURE_SIZES
        "TASK_MANAGEMENT;2048"
        "SCHEDULER;1024"
        "CONTEXT_SWITCHING;512"
        "BLOCKING_SYSTEM;1536"
        "ERROR_HANDLING;256"
        "MUTEXES;512"
        "SEMAPHORES;384"
        "QUEUES;768"
        "EVENT_GROUPS;1024"
        "STREAM_BUFFERS;1280"
        "MEMORY_TRACKING;512"
        "MEMORY_POOLS;896"
        "STACK_CHECKING;256"
        "MPU_SUPPORT;640"
        "LOGGING;1024"
        "TASK_INSPECTION;768"
        "EXECUTION_PROFILING;1536"
        "SYSTEM_TRACING;2048"
        "ENHANCED_ASSERTIONS;384"
        "SMP_SCHEDULER;1792"
        "LOAD_BALANCING;512"
        "CORE_AFFINITY;256"
        "IPC_CHANNELS;1024"
        "CORE_FAILURE_DETECTION;640"
        "TIMERS;896"
        "IO_ABSTRACTION;1280"
        "HIRES_TIMERS;768"
        "UNIVERSAL_TIMEOUTS;384"
        "WATCHDOG_INTEGRATION;256"
        "DEADLOCK_DETECTION;1536"
        "HEALTH_MONITORING;1024"
        "ALERT_SYSTEM;640"
        "RUNTIME_STATS;512"
    )
    
    set(total_size 0)
    set(enabled_features "")
    
    foreach(size_entry ${FEATURE_SIZES})
        string(REPLACE ";" " " size_entry_str "${size_entry}")
        string(REGEX MATCH "([A-Z_]+) ([0-9]+)" match "${size_entry_str}")
        set(feature ${CMAKE_MATCH_1})
        set(size ${CMAKE_MATCH_2})
        
        if(PICO_RTOS_ENABLE_${feature})
            math(EXPR total_size "${total_size} + ${size}")
            list(APPEND enabled_features ${feature})
        endif()
    endforeach()
    
    message(STATUS "Estimated code size: ${total_size} bytes")
    message(STATUS "Enabled features: ${enabled_features}")
    
    set(PICO_RTOS_ESTIMATED_SIZE ${total_size} PARENT_SCOPE)
    set(PICO_RTOS_ENABLED_FEATURES ${enabled_features} PARENT_SCOPE)
endfunction()

# Function to generate feature report
function(pico_rtos_generate_feature_report)
    set(report_file "${CMAKE_BINARY_DIR}/feature-report.txt")
    
    file(WRITE ${report_file} "Pico-RTOS Feature Configuration Report\n")
    file(APPEND ${report_file} "Generated: ${CMAKE_CURRENT_TIMESTAMP}\n")
    file(APPEND ${report_file} "Profile: ${PICO_RTOS_FEATURE_PROFILE}\n")
    file(APPEND ${report_file} "Build Type: ${CMAKE_BUILD_TYPE}\n")
    file(APPEND ${report_file} "\n")
    
    file(APPEND ${report_file} "Enabled Features:\n")
    foreach(category CORE SYNC MEMORY DEBUG MULTICORE SYSTEM QUALITY)
        file(APPEND ${report_file} "\n${category} Features:\n")
        foreach(feature ${PICO_RTOS_${category}_FEATURES})
            if(PICO_RTOS_ENABLE_${feature})
                file(APPEND ${report_file} "  ✓ ${feature}\n")
            else()
                file(APPEND ${report_file} "  ✗ ${feature}\n")
            endif()
        endforeach()
    endforeach()
    
    file(APPEND ${report_file} "\nEstimated Code Size: ${PICO_RTOS_ESTIMATED_SIZE} bytes\n")
    
    message(STATUS "Feature report generated: ${report_file}")
endfunction()

# Function to validate feature configuration
function(pico_rtos_validate_configuration)
    message(STATUS "Validating feature configuration...")
    
    set(validation_errors "")
    
    # Check that core features are enabled
    foreach(feature ${PICO_RTOS_CORE_FEATURES})
        if(NOT PICO_RTOS_ENABLE_${feature})
            list(APPEND validation_errors "Core feature ${feature} is disabled")
        endif()
    endforeach()
    
    # Check for conflicting configurations
    if(PICO_RTOS_ENABLE_MPU_SUPPORT AND NOT PICO_RTOS_ENABLE_MEMORY_TRACKING)
        list(APPEND validation_errors "MPU support requires memory tracking")
    endif()
    
    if(PICO_RTOS_ENABLE_LOAD_BALANCING AND NOT PICO_RTOS_ENABLE_SMP_SCHEDULER)
        list(APPEND validation_errors "Load balancing requires SMP scheduler")
    endif()
    
    if(PICO_RTOS_ENABLE_DEADLOCK_DETECTION AND NOT PICO_RTOS_ENABLE_RUNTIME_STATS)
        list(APPEND validation_errors "Deadlock detection requires runtime statistics")
    endif()
    
    # Report validation results
    if(validation_errors)
        message(FATAL_ERROR "Configuration validation failed:\n  ${validation_errors}")
    else()
        message(STATUS "Configuration validation passed")
    endif()
endfunction()

# Function to apply size optimizations
function(pico_rtos_apply_size_optimizations target)
    message(STATUS "Applying size optimizations to ${target}")
    
    # Compiler-specific optimizations
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE
            -Os                          # Optimize for size
            -ffunction-sections          # Place functions in separate sections
            -fdata-sections             # Place data in separate sections
            -fno-common                 # Don't use common symbols
            -fmerge-all-constants       # Merge identical constants
            -fno-exceptions             # Disable exceptions
            -fno-rtti                   # Disable RTTI
            -fno-threadsafe-statics     # Disable thread-safe statics
        )
        
        target_link_options(${target} PRIVATE
            -Wl,--gc-sections           # Remove unused sections
            -Wl,--strip-all             # Strip all symbols
            -Wl,--as-needed             # Only link needed libraries
        )
        
    elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${target} PRIVATE
            -Oz                         # Optimize aggressively for size
            -ffunction-sections
            -fdata-sections
            -fno-common
            -fmerge-all-constants
            -fno-exceptions
            -fno-rtti
            -fno-threadsafe-statics
        )
        
        target_link_options(${target} PRIVATE
            -Wl,--gc-sections
            -Wl,--strip-all
        )
    endif()
    
    # Feature-specific optimizations
    if(NOT PICO_RTOS_ENABLE_LOGGING)
        target_compile_definitions(${target} PRIVATE PICO_RTOS_NO_LOGGING=1)
    endif()
    
    if(NOT PICO_RTOS_ENABLE_ENHANCED_ASSERTIONS)
        target_compile_definitions(${target} PRIVATE PICO_RTOS_NO_ASSERTIONS=1)
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        target_compile_definitions(${target} PRIVATE PICO_RTOS_SIZE_OPTIMIZED=1)
    endif()
endfunction()

# Main configuration function
function(pico_rtos_configure_features)
    message(STATUS "=== Pico-RTOS Feature Configuration ===")
    
    # Apply feature profile
    pico_rtos_apply_feature_profile(${PICO_RTOS_FEATURE_PROFILE})
    
    # Resolve dependencies
    pico_rtos_resolve_dependencies()
    
    # Calculate footprint
    pico_rtos_calculate_footprint()
    
    # Validate configuration
    pico_rtos_validate_configuration()
    
    # Generate report
    pico_rtos_generate_feature_report()
    
    message(STATUS "Feature configuration complete")
    message(STATUS "Profile: ${PICO_RTOS_FEATURE_PROFILE}")
    message(STATUS "Estimated size: ${PICO_RTOS_ESTIMATED_SIZE} bytes")
    message(STATUS "========================================")
endfunction()

# Export functions and variables to parent scope
set(PICO_RTOS_FEATURE_OPTIMIZATION_LOADED TRUE PARENT_SCOPE)