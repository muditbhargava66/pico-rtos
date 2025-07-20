# Pico-RTOS Logging System Usage Guide

This guide provides comprehensive information on using the Pico-RTOS debug logging system, including configuration, usage patterns, and best practices.

## Overview

The Pico-RTOS logging system provides configurable debug logging with the following key features:

- **Zero Overhead When Disabled**: All logging macros compile to nothing when logging is disabled
- **Configurable Log Levels**: Filter messages by severity (ERROR, WARN, INFO, DEBUG)
- **Subsystem Filtering**: Enable/disable logging for specific RTOS subsystems
- **Flexible Output**: Custom output functions for different environments
- **Context Information**: Automatic capture of timestamp, task ID, and source location
- **Thread-Safe**: Safe to use from multiple tasks and interrupt contexts

## Configuration

### Build-Time Configuration

Enable logging through CMake configuration:

```cmake
# Enable the logging system
option(PICO_RTOS_ENABLE_LOGGING "Enable debug logging system" ON)

# Optional: Configure message buffer size
set(PICO_RTOS_LOG_MESSAGE_MAX_LENGTH "128" CACHE STRING "Maximum log message length")

# Optional: Set default log level
add_compile_definitions(PICO_RTOS_DEFAULT_LOG_LEVEL=PICO_RTOS_LOG_LEVEL_INFO)

# Optional: Set default enabled subsystems
add_compile_definitions(PICO_RTOS_DEFAULT_LOG_SUBSYSTEMS=PICO_RTOS_LOG_SUBSYSTEM_ALL)
```

### Conditional Compilation

Use conditional compilation to include logging code only when needed:

```c
#include "pico_rtos/logging.h"

void my_function(void) {
#if PICO_RTOS_ENABLE_LOGGING
    // Logging-specific code
    pico_rtos_log_init(pico_rtos_log_default_output);
    pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_DEBUG);
#endif
    
    // Regular function code
    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_CORE, "Function called");
}
```

## Initialization

### Basic Initialization

Initialize the logging system during application startup:

```c
#include "pico_rtos/logging.h"

int main(void) {
    // Initialize hardware
    stdio_init_all();
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        return -1;
    }
    
#if PICO_RTOS_ENABLE_LOGGING
    // Initialize logging with default output
    pico_rtos_log_init(pico_rtos_log_default_output);
    
    // Set desired log level
    pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_INFO);
    
    // Enable specific subsystems
    pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_TASK | 
                                   PICO_RTOS_LOG_SUBSYSTEM_MUTEX);
#endif
    
    // Create tasks and start scheduler
    // ...
    
    return 0;
}
```

### Custom Output Function

Implement custom log output for specific requirements:

```c
// Custom output function for UART
void uart_log_output(const pico_rtos_log_entry_t *entry) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "[%08lu] %s:%s - %s\r\n",
             entry->timestamp,
             pico_rtos_log_level_to_string(entry->level),
             pico_rtos_log_subsystem_to_string(entry->subsystem),
             entry->message);
    
    // Send to UART
    uart_puts(uart0, buffer);
}

// Custom output function for file logging
void file_log_output(const pico_rtos_log_entry_t *entry) {
    static FILE *log_file = NULL;
    
    if (!log_file) {
        log_file = fopen("system.log", "a");
        if (!log_file) return;
    }
    
    fprintf(log_file, "[%08lu] T%lu %s:%s - %s\n",
            entry->timestamp,
            entry->task_id,
            pico_rtos_log_level_to_string(entry->level),
            pico_rtos_log_subsystem_to_string(entry->subsystem),
            entry->message);
    
    fflush(log_file);
}

// Initialize with custom output
pico_rtos_log_init(uart_log_output);
```

## Log Levels

### Available Log Levels

```c
typedef enum {
    PICO_RTOS_LOG_LEVEL_NONE = 0,   // No logging
    PICO_RTOS_LOG_LEVEL_ERROR = 1,  // Error conditions only
    PICO_RTOS_LOG_LEVEL_WARN = 2,   // Warnings and errors
    PICO_RTOS_LOG_LEVEL_INFO = 3,   // Informational, warnings, and errors
    PICO_RTOS_LOG_LEVEL_DEBUG = 4   // All messages including debug
} pico_rtos_log_level_t;
```

### Setting Log Levels

```c
// Set log level at runtime
pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_DEBUG);

// Get current log level
pico_rtos_log_level_t current_level = pico_rtos_log_get_level();

// Dynamic log level adjustment
void adjust_log_level_based_on_load(void) {
    uint32_t cpu_usage = get_cpu_usage_percentage();
    
    if (cpu_usage > 90) {
        // Reduce logging overhead under high load
        pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_ERROR);
    } else if (cpu_usage < 50) {
        // Enable more verbose logging when system is idle
        pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_DEBUG);
    }
}
```

## Subsystem Filtering

### Available Subsystems

```c
typedef enum {
    PICO_RTOS_LOG_SUBSYSTEM_CORE = 0x01,      // Core scheduler functions
    PICO_RTOS_LOG_SUBSYSTEM_TASK = 0x02,      // Task management
    PICO_RTOS_LOG_SUBSYSTEM_MUTEX = 0x04,     // Mutex operations
    PICO_RTOS_LOG_SUBSYSTEM_QUEUE = 0x08,     // Queue operations
    PICO_RTOS_LOG_SUBSYSTEM_TIMER = 0x10,     // Timer operations
    PICO_RTOS_LOG_SUBSYSTEM_MEMORY = 0x20,    // Memory management
    PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE = 0x40, // Semaphore operations
    PICO_RTOS_LOG_SUBSYSTEM_ALL = 0xFF        // All subsystems
} pico_rtos_log_subsystem_t;
```

### Subsystem Management

```c
// Enable specific subsystems
pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_TASK | 
                               PICO_RTOS_LOG_SUBSYSTEM_MUTEX);

// Disable specific subsystems
pico_rtos_log_disable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_TIMER);

// Check if subsystem is enabled
if (pico_rtos_log_is_subsystem_enabled(PICO_RTOS_LOG_SUBSYSTEM_MEMORY)) {
    // Memory logging is enabled
}

// Dynamic subsystem control
void configure_logging_for_debug_session(void) {
    // Enable all subsystems for comprehensive debugging
    pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_ALL);
    pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_DEBUG);
}

void configure_logging_for_production(void) {
    // Only enable error logging for critical subsystems
    pico_rtos_log_disable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_ALL);
    pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_CORE | 
                                   PICO_RTOS_LOG_SUBSYSTEM_MEMORY);
    pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_ERROR);
}
```

## Using Logging Macros

### General Logging Macros

```c
// General logging macros with explicit subsystem
PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TASK, "Task creation failed: %s", task_name);
PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory usage high: %lu bytes", usage);
PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_CORE, "System initialized successfully");
PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_QUEUE, "Queue operation: send item %d", item);
```

### Subsystem-Specific Convenience Macros

```c
// Task subsystem logging
PICO_RTOS_LOG_TASK_ERROR("Failed to create task: %s", name);
PICO_RTOS_LOG_TASK_WARN("Task priority changed: %d -> %d", old_prio, new_prio);
PICO_RTOS_LOG_TASK_INFO("Task '%s' started", task_name);
PICO_RTOS_LOG_TASK_DEBUG("Task switch: %s -> %s", from_task, to_task);

// Memory subsystem logging
PICO_RTOS_LOG_MEMORY_ERROR("Memory allocation failed: %zu bytes", size);
PICO_RTOS_LOG_MEMORY_WARN("Memory fragmentation detected");
PICO_RTOS_LOG_MEMORY_INFO("Memory stats: %lu used, %lu free", used, free);
PICO_RTOS_LOG_MEMORY_DEBUG("Allocated %zu bytes at %p", size, ptr);

// Mutex subsystem logging
PICO_RTOS_LOG_MUTEX_ERROR("Mutex deadlock detected");
PICO_RTOS_LOG_MUTEX_WARN("Mutex timeout: %s", mutex_name);
PICO_RTOS_LOG_MUTEX_INFO("Mutex created: %s", mutex_name);
PICO_RTOS_LOG_MUTEX_DEBUG("Mutex acquired by task %d", task_id);
```

## Practical Usage Examples

### Task Debugging

```c
void my_task(void *param) {
    const char *task_name = (const char *)param;
    
    PICO_RTOS_LOG_TASK_INFO("Task '%s' starting", task_name);
    
    while (1) {
        PICO_RTOS_LOG_TASK_DEBUG("Task '%s' iteration", task_name);
        
        // Simulate work
        if (do_work() != 0) {
            PICO_RTOS_LOG_TASK_ERROR("Task '%s' work failed", task_name);
            break;
        }
        
        PICO_RTOS_LOG_TASK_DEBUG("Task '%s' work completed", task_name);
        pico_rtos_task_delay(1000);
    }
    
    PICO_RTOS_LOG_TASK_WARN("Task '%s' exiting", task_name);
}
```

### Memory Management Debugging

```c
void *safe_malloc(size_t size) {
    PICO_RTOS_LOG_MEMORY_DEBUG("Allocating %zu bytes", size);
    
    void *ptr = pico_rtos_malloc(size);
    if (!ptr) {
        PICO_RTOS_LOG_MEMORY_ERROR("Allocation failed: %zu bytes", size);
        
        // Log memory statistics
        uint32_t current, peak, allocations;
        pico_rtos_get_memory_stats(&current, &peak, &allocations);
        PICO_RTOS_LOG_MEMORY_ERROR("Memory stats: %lu current, %lu peak, %lu allocs",
                                   current, peak, allocations);
        return NULL;
    }
    
    PICO_RTOS_LOG_MEMORY_DEBUG("Allocated %zu bytes at %p", size, ptr);
    return ptr;
}

void safe_free(void *ptr, size_t size) {
    if (!ptr) {
        PICO_RTOS_LOG_MEMORY_WARN("Attempted to free NULL pointer");
        return;
    }
    
    PICO_RTOS_LOG_MEMORY_DEBUG("Freeing %zu bytes at %p", size, ptr);
    pico_rtos_free(ptr, size);
}
```

### Synchronization Debugging

```c
bool safe_mutex_lock(pico_rtos_mutex_t *mutex, const char *mutex_name, uint32_t timeout) {
    PICO_RTOS_LOG_MUTEX_DEBUG("Attempting to lock mutex '%s'", mutex_name);
    
    uint32_t start_time = pico_rtos_get_tick_count();
    bool result = pico_rtos_mutex_lock(mutex, timeout);
    uint32_t elapsed = pico_rtos_get_tick_count() - start_time;
    
    if (result) {
        PICO_RTOS_LOG_MUTEX_DEBUG("Mutex '%s' locked in %lu ms", mutex_name, elapsed);
    } else {
        PICO_RTOS_LOG_MUTEX_WARN("Mutex '%s' lock timeout after %lu ms", mutex_name, elapsed);
    }
    
    return result;
}

void safe_mutex_unlock(pico_rtos_mutex_t *mutex, const char *mutex_name) {
    PICO_RTOS_LOG_MUTEX_DEBUG("Unlocking mutex '%s'", mutex_name);
    
    if (!pico_rtos_mutex_unlock(mutex)) {
        PICO_RTOS_LOG_MUTEX_ERROR("Failed to unlock mutex '%s' - not owned by current task", mutex_name);
    }
}
```

### System Monitoring

```c
void system_monitor_task(void *param) {
    PICO_RTOS_LOG_CORE_INFO("System monitor started");
    
    while (1) {
        // Log system statistics
        pico_rtos_system_stats_t stats;
        pico_rtos_get_system_stats(&stats);
        
        PICO_RTOS_LOG_CORE_INFO("System stats: %lu tasks (%lu ready, %lu blocked)",
                                stats.total_tasks, stats.ready_tasks, stats.blocked_tasks);
        
        PICO_RTOS_LOG_MEMORY_INFO("Memory: %lu current, %lu peak (%lu allocations)",
                                  stats.current_memory, stats.peak_memory, stats.total_allocations);
        
        // Check for potential issues
        if (stats.current_memory > (stats.peak_memory * 0.9)) {
            PICO_RTOS_LOG_MEMORY_WARN("Memory usage approaching peak: %lu/%lu",
                                      stats.current_memory, stats.peak_memory);
        }
        
        if (stats.blocked_tasks > (stats.total_tasks / 2)) {
            PICO_RTOS_LOG_CORE_WARN("High number of blocked tasks: %lu/%lu",
                                    stats.blocked_tasks, stats.total_tasks);
        }
        
        pico_rtos_task_delay(5000); // Monitor every 5 seconds
    }
}
```

## Performance Considerations

### Logging Overhead

When logging is enabled, consider the performance impact:

```c
// Measure logging overhead
void measure_logging_overhead(void) {
    uint32_t start_time, end_time;
    const int iterations = 1000;
    
    // Measure with logging enabled
    start_time = pico_rtos_get_tick_count();
    for (int i = 0; i < iterations; i++) {
        PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_CORE, "Test message %d", i);
    }
    end_time = pico_rtos_get_tick_count();
    
    printf("Logging overhead: %lu ms for %d messages\n", 
           end_time - start_time, iterations);
}
```

### Optimizing Performance

```c
// Use appropriate log levels in performance-critical sections
void performance_critical_function(void) {
    // Only log errors in performance-critical code
    if (critical_operation() != 0) {
        PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_CORE, "Critical operation failed");
    }
    
    // Avoid debug logging in tight loops
    for (int i = 0; i < 10000; i++) {
        // Don't do this in performance-critical loops:
        // PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_CORE, "Loop iteration %d", i);
        
        process_item(i);
    }
}

// Use conditional logging for expensive operations
void expensive_debug_logging(void) {
    // Only perform expensive logging operations when debug level is enabled
    if (pico_rtos_log_get_level() >= PICO_RTOS_LOG_LEVEL_DEBUG) {
        char *expensive_debug_info = generate_debug_info(); // Expensive operation
        PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_CORE, "Debug info: %s", expensive_debug_info);
        free(expensive_debug_info);
    }
}
```

## Best Practices

### 1. Use Appropriate Log Levels

```c
// ERROR: For conditions that prevent normal operation
PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TASK, "Task creation failed - out of memory");

// WARN: For unusual conditions that don't prevent operation
PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory usage above 80%%");

// INFO: For important operational information
PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_CORE, "System initialization complete");

// DEBUG: For detailed diagnostic information
PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_QUEUE, "Queue item processed: ID=%d", item_id);
```

### 2. Include Relevant Context

```c
// Good: Include relevant context information
PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TASK, 
                    "Task '%s' stack overflow detected (size: %lu bytes)", 
                    task_name, stack_size);

// Poor: Vague error message
PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TASK, "Stack overflow");
```

### 3. Use Subsystem-Specific Macros

```c
// Preferred: Use subsystem-specific macros
PICO_RTOS_LOG_TASK_ERROR("Task creation failed: %s", error_msg);

// Acceptable: Use general macro with explicit subsystem
PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TASK, "Task creation failed: %s", error_msg);
```

### 4. Implement Structured Logging

```c
// Create structured log entries for easier parsing
void log_task_event(const char *task_name, const char *event, const char *details) {
    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_TASK, 
                       "TASK_EVENT|name=%s|event=%s|details=%s", 
                       task_name, event, details);
}

// Usage
log_task_event("sensor_task", "STARTED", "priority=3,stack=1024");
log_task_event("sensor_task", "ERROR", "sensor_read_failed,code=0x42");
```

### 5. Configure for Different Environments

```c
void configure_logging_for_environment(void) {
#ifdef DEBUG_BUILD
    // Development environment: verbose logging
    pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_DEBUG);
    pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_ALL);
    pico_rtos_log_init(pico_rtos_log_default_output);
    
#elif defined(TESTING_BUILD)
    // Testing environment: structured logging
    pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_INFO);
    pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_ALL);
    pico_rtos_log_init(structured_log_output);
    
#else
    // Production environment: minimal logging
    pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_ERROR);
    pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_CORE | 
                                   PICO_RTOS_LOG_SUBSYSTEM_MEMORY);
    pico_rtos_log_init(production_log_output);
#endif
}
```

## Troubleshooting

### Common Issues

1. **Logging Not Working**
   - Check if `PICO_RTOS_ENABLE_LOGGING` is defined
   - Verify logging initialization
   - Check log level and subsystem filters

2. **Performance Issues**
   - Reduce log level in performance-critical sections
   - Disable verbose subsystems
   - Use efficient output functions

3. **Memory Issues**
   - Monitor memory usage with logging enabled
   - Use compact output format
   - Implement log rotation for file output

### Debug Logging Configuration

```c
void debug_logging_configuration(void) {
    printf("Logging Configuration:\n");
    printf("  Enabled: %s\n", PICO_RTOS_ENABLE_LOGGING ? "Yes" : "No");
    
#if PICO_RTOS_ENABLE_LOGGING
    printf("  Current Level: %s\n", pico_rtos_log_level_to_string(pico_rtos_log_get_level()));
    printf("  Message Buffer Size: %d bytes\n", PICO_RTOS_LOG_MESSAGE_MAX_LENGTH);
    
    // Check subsystem status
    const char *subsystems[] = {"CORE", "TASK", "MUTEX", "QUEUE", "TIMER", "MEMORY", "SEMAPHORE"};
    pico_rtos_log_subsystem_t subsystem_flags[] = {
        PICO_RTOS_LOG_SUBSYSTEM_CORE,
        PICO_RTOS_LOG_SUBSYSTEM_TASK,
        PICO_RTOS_LOG_SUBSYSTEM_MUTEX,
        PICO_RTOS_LOG_SUBSYSTEM_QUEUE,
        PICO_RTOS_LOG_SUBSYSTEM_TIMER,
        PICO_RTOS_LOG_SUBSYSTEM_MEMORY,
        PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE
    };
    
    printf("  Enabled Subsystems:\n");
    for (int i = 0; i < 7; i++) {
        printf("    %s: %s\n", subsystems[i], 
               pico_rtos_log_is_subsystem_enabled(subsystem_flags[i]) ? "Yes" : "No");
    }
#endif
}
```

The logging system provides powerful debugging capabilities while maintaining excellent performance characteristics when properly configured. Use it to gain insights into your RTOS application behavior and troubleshoot issues effectively.