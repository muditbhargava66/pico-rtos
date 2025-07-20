# Pico-RTOS User Guide

This user guide provides an overview of the Pico-RTOS features and how to use them in your projects.

## Introduction

Pico-RTOS is a production-ready, lightweight real-time operating system specifically designed for the Raspberry Pi Pico board. Version 0.2.1 provides comprehensive features including:

- Complete ARM Cortex-M0+ context switching
- Priority inheritance to prevent priority inversion
- Stack overflow protection with dual canaries
- Memory management with tracking and leak detection
- Real-time system monitoring and diagnostics
- Thread-safe operations throughout
- Configurable system tick frequency for different timing requirements
- Enhanced error reporting system with detailed error codes and history
- Optional debug logging system with configurable levels and subsystems
- Comprehensive examples demonstrating hardware integration patterns

## Configuration Options

Pico-RTOS v0.2.1 provides extensive configuration options to customize the system for your specific needs. These options can be set through CMake configuration or by defining them before including the RTOS headers.

### System Tick Frequency

The system tick frequency determines the timing resolution and affects system overhead:

```cmake
# CMake configuration options
set(PICO_RTOS_TICK_RATE_HZ "1000" CACHE STRING "System tick frequency in Hz")
set_property(CACHE PICO_RTOS_TICK_RATE_HZ PROPERTY STRINGS "100;250;500;1000;2000")
```

Available predefined frequencies:
- 100 Hz (10ms tick period) - Low overhead, coarse timing
- 250 Hz (4ms tick period) - Balanced performance
- 500 Hz (2ms tick period) - Good timing resolution
- 1000 Hz (1ms tick period) - Default, excellent timing
- 2000 Hz (0.5ms tick period) - High resolution, higher overhead

### Feature Configuration

Enable or disable specific features based on your requirements:

```cmake
# Debug and development features
option(PICO_RTOS_ENABLE_LOGGING "Enable debug logging system" OFF)
option(PICO_RTOS_ENABLE_ERROR_HISTORY "Enable error history tracking" ON)
set(PICO_RTOS_ERROR_HISTORY_SIZE "10" CACHE STRING "Number of errors to keep in history")

# System monitoring and protection
option(PICO_RTOS_ENABLE_STACK_CHECKING "Enable stack overflow checking" ON)
option(PICO_RTOS_ENABLE_MEMORY_TRACKING "Enable memory usage tracking" ON)
option(PICO_RTOS_ENABLE_RUNTIME_STATS "Enable runtime statistics collection" ON)

# System limits
set(PICO_RTOS_MAX_TASKS "16" CACHE STRING "Maximum number of tasks")
set(PICO_RTOS_MAX_TIMERS "8" CACHE STRING "Maximum number of software timers")
```

### Runtime Configuration

Query system configuration at runtime:

```c
// Get current tick frequency
uint32_t tick_rate = pico_rtos_get_tick_rate_hz();
printf("System tick rate: %lu Hz\n", tick_rate);

// Check if features are enabled
#if PICO_RTOS_ENABLE_LOGGING
printf("Debug logging is enabled\n");
#endif

#if PICO_RTOS_ENABLE_ERROR_HISTORY
printf("Error history size: %d entries\n", PICO_RTOS_ERROR_HISTORY_SIZE);
#endif
```

## Getting Started

Before using any RTOS functions, you must initialize the system:

```c
#include "pico_rtos.h"

int main() {
    // Initialize RTOS
    if (!pico_rtos_init()) {
        // Handle initialization failure
        return -1;
    }
    
    // Create your tasks here
    
    // Start the scheduler (this function does not return)
    pico_rtos_start();
    
    return 0;
}
```

## Tasks

Tasks are the basic units of execution in Pico-RTOS. Each task represents an independent thread of control with its own stack and priority.

To create a task, use the `pico_rtos_task_create` function:

```c
pico_rtos_task_t task;
pico_rtos_task_create(&task, "Task Name", task_function, NULL, stack_size, priority);
```

The task function should have the following signature:

```c
void task_function(void *param)
```

## Queues

Queues provide a mechanism for inter-task communication and data exchange. Tasks can send and receive messages through queues.

To create a queue, use the `pico_rtos_queue_init` function:

```c
pico_rtos_queue_t queue;
uint8_t queue_buffer[QUEUE_SIZE];
pico_rtos_queue_init(&queue, queue_buffer, sizeof(uint8_t), QUEUE_SIZE);
```

To send and receive messages, use the `pico_rtos_queue_send` and `pico_rtos_queue_receive` functions:

```c
uint8_t item = 42;
pico_rtos_queue_send(&queue, &item, 0);

uint8_t received_item;
pico_rtos_queue_receive(&queue, &received_item, PICO_RTOS_WAIT_FOREVER);
```

## Semaphores

Semaphores provide a mechanism for synchronization and resource management between tasks.

To create a semaphore, use the `pico_rtos_semaphore_init` function:

```c
pico_rtos_semaphore_t semaphore;
pico_rtos_semaphore_init(&semaphore, initial_count, max_count);
```

To acquire and release a semaphore, use the `pico_rtos_semaphore_take` and `pico_rtos_semaphore_give` functions:

```c
pico_rtos_semaphore_take(&semaphore, PICO_RTOS_WAIT_FOREVER);
// Critical section
pico_rtos_semaphore_give(&semaphore);
```

## Mutexes

Mutexes provide mutual exclusion and synchronization between tasks accessing shared resources.

To create a mutex, use the `pico_rtos_mutex_init` function:

```c
pico_rtos_mutex_t mutex;
pico_rtos_mutex_init(&mutex);
```

To acquire and release a mutex, use the `pico_rtos_mutex_lock` and `pico_rtos_mutex_unlock` functions:

```c
pico_rtos_mutex_lock(&mutex);
// Critical section
pico_rtos_mutex_unlock(&mutex);
```

## Timers

Timers allow you to schedule periodic or one-shot callbacks.

To create a timer, use the `pico_rtos_timer_init` function:

```c
pico_rtos_timer_t timer;
pico_rtos_timer_init(&timer, "Timer", timer_callback, NULL, period, auto_reload);
```

To start and stop a timer, use the `pico_rtos_timer_start` and `pico_rtos_timer_stop` functions:

```c
pico_rtos_timer_start(&timer);
// Timer running
pico_rtos_timer_stop(&timer);
```

The timer callback function should have the following signature:

```c
void timer_callback(void *param)
```

## Error Handling and Reporting

Pico-RTOS v0.2.1 includes a comprehensive error reporting system with detailed error codes and optional error history tracking.

### Error Codes

The system provides specific error codes organized by category:

```c
#include "pico_rtos/error.h"

// Task-related errors (100-199)
PICO_RTOS_ERROR_TASK_INVALID_PRIORITY
PICO_RTOS_ERROR_TASK_STACK_OVERFLOW
PICO_RTOS_ERROR_TASK_INVALID_STATE

// Memory-related errors (200-299)
PICO_RTOS_ERROR_OUT_OF_MEMORY
PICO_RTOS_ERROR_INVALID_POINTER
PICO_RTOS_ERROR_MEMORY_CORRUPTION

// Synchronization errors (300-399)
PICO_RTOS_ERROR_MUTEX_TIMEOUT
PICO_RTOS_ERROR_SEMAPHORE_TIMEOUT
PICO_RTOS_ERROR_QUEUE_FULL

// System errors (400-499)
PICO_RTOS_ERROR_SYSTEM_NOT_INITIALIZED
PICO_RTOS_ERROR_INVALID_CONFIGURATION
```

### Error Reporting

Report errors with context information:

```c
// Report an error with context
PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_TASK_INVALID_PRIORITY, task_priority);

// Get error description
const char *desc = pico_rtos_get_error_description(PICO_RTOS_ERROR_OUT_OF_MEMORY);
printf("Error: %s\n", desc);

// Get last error information
pico_rtos_error_info_t error_info;
if (pico_rtos_get_last_error(&error_info)) {
    printf("Last error: %s at %s:%d\n", 
           error_info.description, error_info.file, error_info.line);
}
```

### Error History

When enabled, the system maintains a history of errors for debugging:

```c
#if PICO_RTOS_ENABLE_ERROR_HISTORY
// Get error history
pico_rtos_error_info_t errors[10];
size_t count;
if (pico_rtos_get_error_history(errors, 10, &count)) {
    printf("Found %zu errors in history:\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("  %lu: %s\n", errors[i].timestamp, errors[i].description);
    }
}

// Get error statistics
pico_rtos_error_stats_t stats;
pico_rtos_get_error_stats(&stats);
printf("Total errors: %lu, Task errors: %lu, Memory errors: %lu\n",
       stats.total_errors, stats.task_errors, stats.memory_errors);
#endif
```

## Debug Logging

The optional debug logging system provides configurable logging with minimal overhead when disabled.

### Enabling Logging

Enable logging through CMake configuration:

```cmake
option(PICO_RTOS_ENABLE_LOGGING "Enable debug logging system" ON)
```

### Log Levels

Configure logging verbosity:

```c
#include "pico_rtos/logging.h"

// Set log level (only messages at or below this level are processed)
pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_INFO);

// Available levels:
// PICO_RTOS_LOG_LEVEL_NONE   - No logging
// PICO_RTOS_LOG_LEVEL_ERROR  - Error conditions only
// PICO_RTOS_LOG_LEVEL_WARN   - Warnings and errors
// PICO_RTOS_LOG_LEVEL_INFO   - Informational messages, warnings, and errors
// PICO_RTOS_LOG_LEVEL_DEBUG  - All messages including debug information
```

### Subsystem Filtering

Enable logging for specific RTOS subsystems:

```c
// Enable logging for specific subsystems
pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_TASK | 
                               PICO_RTOS_LOG_SUBSYSTEM_MUTEX);

// Disable logging for specific subsystems
pico_rtos_log_disable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_TIMER);

// Available subsystems:
// PICO_RTOS_LOG_SUBSYSTEM_CORE      - Core scheduler functions
// PICO_RTOS_LOG_SUBSYSTEM_TASK      - Task management
// PICO_RTOS_LOG_SUBSYSTEM_MUTEX     - Mutex operations
// PICO_RTOS_LOG_SUBSYSTEM_QUEUE     - Queue operations
// PICO_RTOS_LOG_SUBSYSTEM_TIMER     - Timer operations
// PICO_RTOS_LOG_SUBSYSTEM_MEMORY    - Memory management
// PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE - Semaphore operations
```

### Using Logging Macros

Use logging macros in your application:

```c
// General logging macros
PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_TASK, "Task creation failed: %s", task_name);
PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory usage high: %lu bytes", usage);
PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_CORE, "System initialized successfully");
PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_QUEUE, "Queue operation: send item %d", item);

// Convenience macros for common subsystems
PICO_RTOS_LOG_TASK_ERROR("Failed to create task: %s", name);
PICO_RTOS_LOG_MUTEX_DEBUG("Mutex acquired by task %d", task_id);
PICO_RTOS_LOG_MEMORY_WARN("Low memory: %lu bytes remaining", free_memory);
```

### Custom Log Output

Implement custom log output handling:

```c
// Custom log output function
void my_log_output(const pico_rtos_log_entry_t *entry) {
    printf("[%lu] %s:%s - %s\n",
           entry->timestamp,
           pico_rtos_log_level_to_string(entry->level),
           pico_rtos_log_subsystem_to_string(entry->subsystem),
           entry->message);
}

// Initialize logging with custom output
pico_rtos_log_init(my_log_output);

// Or use built-in output functions
pico_rtos_log_init(pico_rtos_log_default_output);    // Full format
pico_rtos_log_init(pico_rtos_log_compact_output);    // Compact format
```

## Examples and Learning Resources

Pico-RTOS v0.2.1 includes comprehensive examples demonstrating real-world usage patterns:

### Hardware Interrupt Handling

The `examples/hardware_interrupt/` example demonstrates:
- Proper interrupt service routine implementation with RTOS integration
- Safe communication between interrupt context and task context using queues
- Interrupt priority configuration and management
- Maintaining system responsiveness during interrupt processing

```c
// Key concepts from the hardware interrupt example
void gpio_interrupt_handler(uint gpio, uint32_t events) {
    // RTOS-aware interrupt handling
    pico_rtos_interrupt_enter();
    
    // Send interrupt data to handler task via queue
    interrupt_event_t event = {
        .timestamp = pico_rtos_get_tick_count(),
        .gpio_pin = gpio,
        .event_type = events
    };
    
    // Safe queue send from interrupt context
    pico_rtos_queue_send(&interrupt_queue, &event, PICO_RTOS_NO_WAIT);
    
    pico_rtos_interrupt_exit();
}
```

### Multi-Task Communication Patterns

The `examples/task_communication/` example shows:
- Queue-based producer-consumer patterns
- Semaphore-based resource sharing
- Task notification lightweight messaging
- Proper error handling and timeout management

### Power Management

The `examples/power_management/` example demonstrates:
- Idle task hooks for power optimization
- Integration with Pico SDK sleep modes
- Wake-up event handling and task resumption
- Power consumption measurement techniques

### Performance Benchmarking

The `examples/performance_benchmark/` example provides:
- Context switch timing measurement
- Interrupt latency measurement
- Synchronization primitive overhead analysis
- Standardized benchmark output for comparison

For more detailed information on the Pico-RTOS API, refer to the [API Reference](api_reference.md).

## Best Practices

- Assign appropriate priorities to tasks based on their importance and real-time requirements.
- Use mutexes and semaphores judiciously to avoid deadlocks and priority inversion.
- Keep critical sections as short as possible to minimize blocking time.
- Use queues for inter-task communication instead of shared memory wherever possible.
- Handle timer callbacks efficiently and avoid performing time-consuming operations within them.

## Conclusion

Pico-RTOS provides a simple and efficient way to develop real-time applications on the Raspberry Pi Pico board. By leveraging its features and following best practices, you can create robust and responsive embedded systems.

For more information and examples, refer to the [Examples](examples/) directory in the Pico-RTOS repository.

Happy coding with Pico-RTOS!

---
## Priority Management

Pico-RTOS uses priority-based preemptive scheduling. Higher priority tasks preempt lower priority tasks. Tasks with the same priority use round-robin scheduling.

### Priority Inheritance

Mutexes support priority inheritance to prevent priority inversion. When a high-priority task blocks on a mutex owned by a lower-priority task, the owner's priority is temporarily boosted.

```c
// High priority task (priority 5)
void high_priority_task(void *param) {
    while (1) {
        if (pico_rtos_mutex_lock(&shared_mutex, 1000)) {
            // Critical section
            pico_rtos_mutex_unlock(&shared_mutex);
        }
        pico_rtos_task_delay(100);
    }
}

// Low priority task (priority 1) - will be boosted when high priority task waits
void low_priority_task(void *param) {
    while (1) {
        if (pico_rtos_mutex_lock(&shared_mutex, PICO_RTOS_WAIT_FOREVER)) {
            // Long critical section - priority will be boosted
            pico_rtos_task_delay(500);
            pico_rtos_mutex_unlock(&shared_mutex);
        }
        pico_rtos_task_delay(1000);
    }
}
```

### Dynamic Priority Changes

You can change task priorities at runtime:

```c
// Change current task priority
pico_rtos_task_set_priority(NULL, 3);

// Change another task's priority
pico_rtos_task_set_priority(&my_task, 5);
```

## Memory Management

Pico-RTOS provides tracked memory allocation to prevent leaks and monitor usage:

```c
// Use tracked allocation instead of malloc
void *buffer = pico_rtos_malloc(1024);

// Free with size tracking
pico_rtos_free(buffer, 1024);

// Get memory statistics
uint32_t current, peak, allocations;
pico_rtos_get_memory_stats(&current, &peak, &allocations);
printf("Memory: %lu current, %lu peak, %lu allocations\n", 
       current, peak, allocations);
```

## Stack Overflow Protection

All tasks are automatically protected with stack canaries:

```c
// Stack overflow is detected automatically
void task_with_deep_recursion(void *param) {
    char large_buffer[2048];  // This might cause overflow
    // ... stack overflow will be detected and handled
}

// Custom stack overflow handler (optional)
void pico_rtos_handle_stack_overflow(pico_rtos_task_t *task) {
    printf("Stack overflow in task: %s\n", task->name);
    // Custom handling logic
    pico_rtos_task_delete(task);  // Terminate the task
}
```

## System Monitoring

Get comprehensive system statistics:

```c
void monitor_task(void *param) {
    pico_rtos_system_stats_t stats;
    
    while (1) {
        pico_rtos_get_system_stats(&stats);
        
        printf("System Stats:\n");
        printf("  Tasks: %lu total, %lu ready, %lu blocked\n",
               stats.total_tasks, stats.ready_tasks, stats.blocked_tasks);
        printf("  Memory: %lu current, %lu peak\n",
               stats.current_memory, stats.peak_memory);
        printf("  Uptime: %lu ms\n", stats.system_uptime);
        
        // Calculate CPU usage (approximate)
        uint32_t idle_count = pico_rtos_get_idle_counter();
        // CPU usage calculation based on idle counter
        
        pico_rtos_task_delay(5000);  // Report every 5 seconds
    }
}
```

## Advanced Features

### Interrupt-Safe Operations

Use interrupt-safe context switching:

```c
void my_interrupt_handler(void) {
    pico_rtos_interrupt_enter();
    
    // Interrupt handling code
    
    // Request context switch if needed
    pico_rtos_request_context_switch();
    
    pico_rtos_interrupt_exit();
}
```

### Task Notifications

Use semaphores for lightweight task notifications:

```c
pico_rtos_semaphore_t notification;
pico_rtos_semaphore_init(&notification, 0, 1);  // Binary semaphore

// Notifying task
void sender_task(void *param) {
    while (1) {
        // Do work
        pico_rtos_semaphore_give(&notification);  // Notify
        pico_rtos_task_delay(1000);
    }
}

// Waiting task
void receiver_task(void *param) {
    while (1) {
        if (pico_rtos_semaphore_take(&notification, PICO_RTOS_WAIT_FOREVER)) {
            // Handle notification
            printf("Notification received!\n");
        }
    }
}
```

## Best Practices

### Task Design
- Keep task functions simple and focused
- Use appropriate stack sizes (minimum 128 bytes)
- Set priorities based on real-time requirements
- Use delays to prevent CPU hogging

### Synchronization
- Always use timeouts for blocking operations
- Prefer mutexes for mutual exclusion
- Use semaphores for signaling and counting
- Keep critical sections short

### Memory Management
- Use tracked allocation functions
- Monitor memory usage regularly
- Free resources in task cleanup
- Validate allocation success

### Error Handling
- Check return values of all RTOS functions
- Implement proper error recovery
- Use system statistics for debugging
- Monitor stack usage

## Performance Considerations

- Context switch time: ~50-100 CPU cycles
- Memory overhead: ~32 bytes per task + stack
- Interrupt latency: Minimal with proper nesting
- Timer resolution: 1ms system tick

## Debugging Tips

1. **Use System Statistics**: Monitor task states and memory usage
2. **Check Stack Usage**: Enable stack overflow protection
3. **Monitor Priorities**: Ensure proper priority assignment
4. **Validate Timeouts**: Use appropriate timeout values
5. **Test Edge Cases**: Test with maximum loads and edge conditions

## Migration from Other RTOSes

If migrating from FreeRTOS or other RTOSes:

- Task creation is similar but requires explicit stack size
- Priority inheritance is automatic in mutexes
- Memory management is tracked by default
- System monitoring is built-in
- Stack overflow protection is automatic

## Conclusion

Pico-RTOS v0.2.1 builds upon the solid foundation of v0.2.0 with enhanced developer experience through comprehensive examples, improved debugging capabilities, and flexible configuration options. The combination of real-time capabilities, safety features, monitoring tools, and extensive documentation makes it an excellent choice for both learning and professional embedded development on Raspberry Pi Pico.

Key improvements in v0.2.1:
- Configurable system tick frequency for different timing requirements
- Enhanced error reporting with detailed codes and optional history tracking
- Optional debug logging system with zero overhead when disabled
- Comprehensive examples demonstrating real-world integration patterns
- Improved build system with extensive configuration options
- Better documentation and learning resources