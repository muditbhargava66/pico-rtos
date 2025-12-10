# Pico-RTOS Error Code Reference

This document provides a comprehensive reference for all error codes in Pico-RTOS v0.3.1, including their meanings, common causes, and recommended solutions.

## Error Code Categories

Error codes are organized into categories with specific numeric ranges:

- **0**: No error
- **100-199**: Task-related errors
- **200-299**: Memory-related errors
- **300-399**: Synchronization-related errors
- **400-499**: System-related errors
- **500-599**: Hardware-related errors
- **600-699**: Configuration-related errors

## Task Errors (100-199)

### PICO_RTOS_ERROR_TASK_INVALID_PRIORITY (100)
**Description**: Task priority is outside the valid range.

**Common Causes**:
- Priority value is 0 (reserved for idle task)
- Priority value exceeds maximum allowed priority
- Negative priority value passed

**Solutions**:
- Use priority values from 1 to maximum configured priority
- Check task creation parameters
- Verify priority constants are correctly defined

**Example**:
```c
// Incorrect - priority 0 is reserved
pico_rtos_task_create(&task, "MyTask", task_func, NULL, 1024, 0);

// Correct - use valid priority range
pico_rtos_task_create(&task, "MyTask", task_func, NULL, 1024, 3);
```

### PICO_RTOS_ERROR_TASK_STACK_OVERFLOW (101)
**Description**: Task stack has overflowed its allocated space.

**Common Causes**:
- Stack size too small for task requirements
- Deep recursion or large local variables
- Stack corruption due to buffer overruns

**Solutions**:
- Increase task stack size
- Reduce local variable usage
- Check for buffer overruns in task code
- Enable stack checking for early detection

**Example**:
```c
// Increase stack size for tasks with large requirements
pico_rtos_task_create(&task, "BigTask", task_func, NULL, 2048, 3); // Increased from 1024
```

### PICO_RTOS_ERROR_TASK_INVALID_STATE (102)
**Description**: Operation attempted on task in invalid state.

**Common Causes**:
- Trying to resume a task that's not suspended
- Attempting to suspend an already suspended task
- Operating on a terminated task

**Solutions**:
- Check task state before operations
- Use proper task state management
- Implement state tracking in application

### PICO_RTOS_ERROR_TASK_INVALID_PARAMETER (103)
**Description**: Invalid parameter passed to task function.

**Common Causes**:
- NULL task pointer
- NULL task function pointer
- Invalid stack size (too small)

**Solutions**:
- Validate all parameters before task creation
- Ensure minimum stack size requirements
- Check for NULL pointers

### PICO_RTOS_ERROR_TASK_STACK_TOO_SMALL (104)
**Description**: Specified stack size is below minimum requirements.

**Common Causes**:
- Stack size less than minimum (typically 128 bytes)
- Incorrect stack size calculation

**Solutions**:
- Use minimum stack size of 256 bytes or more
- Calculate stack requirements based on task complexity

### PICO_RTOS_ERROR_TASK_MAX_TASKS_EXCEEDED (109)
**Description**: Maximum number of tasks has been reached.

**Common Causes**:
- Creating more tasks than configured maximum
- Not properly cleaning up terminated tasks

**Solutions**:
- Increase PICO_RTOS_MAX_TASKS configuration
- Implement proper task lifecycle management
- Delete unused tasks

## Memory Errors (200-299)

### PICO_RTOS_ERROR_OUT_OF_MEMORY (200)
**Description**: System has run out of available memory.

**Common Causes**:
- Memory leaks in application code
- Insufficient heap size configuration
- Too many concurrent allocations

**Solutions**:
- Increase heap size
- Fix memory leaks
- Use memory tracking to monitor usage
- Implement memory pooling for frequent allocations

**Example**:
```c
// Check memory statistics
uint32_t current, peak, allocations;
pico_rtos_get_memory_stats(&current, &peak, &allocations);
printf("Memory: %lu current, %lu peak, %lu allocations\n", current, peak, allocations);
```

### PICO_RTOS_ERROR_INVALID_POINTER (201)
**Description**: NULL or invalid pointer passed to function.

**Common Causes**:
- Passing NULL pointers to functions expecting valid pointers
- Using uninitialized pointers
- Pointer corruption

**Solutions**:
- Always validate pointers before use
- Initialize pointers properly
- Use defensive programming practices

### PICO_RTOS_ERROR_MEMORY_CORRUPTION (202)
**Description**: Memory corruption detected.

**Common Causes**:
- Buffer overruns
- Use after free
- Double free operations
- Stack overflow affecting heap

**Solutions**:
- Use bounds checking
- Implement proper memory management
- Enable stack overflow protection
- Use memory debugging tools

## Synchronization Errors (300-399)

### PICO_RTOS_ERROR_MUTEX_TIMEOUT (300)
**Description**: Mutex acquisition timed out.

**Common Causes**:
- Deadlock conditions
- Mutex held too long by another task
- Insufficient timeout value

**Solutions**:
- Increase timeout value
- Implement deadlock detection
- Reduce critical section duration
- Use priority inheritance (enabled by default)

**Example**:
```c
// Use appropriate timeout values
if (!pico_rtos_mutex_lock(&mutex, 1000)) {  // 1 second timeout
    PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MUTEX_TIMEOUT, 0);
    // Handle timeout condition
}
```

### PICO_RTOS_ERROR_MUTEX_NOT_OWNED (301)
**Description**: Attempt to unlock mutex not owned by current task.

**Common Causes**:
- Task trying to unlock mutex it doesn't own
- Mutex ownership tracking error
- Incorrect mutex usage pattern

**Solutions**:
- Only unlock mutexes owned by current task
- Implement proper mutex ownership tracking
- Use RAII patterns where possible

### PICO_RTOS_ERROR_QUEUE_FULL (320)
**Description**: Queue is full and cannot accept more items.

**Common Causes**:
- Producer faster than consumer
- Consumer task blocked or terminated
- Queue size too small

**Solutions**:
- Increase queue size
- Balance producer/consumer rates
- Use timeouts for queue operations
- Implement flow control

### PICO_RTOS_ERROR_QUEUE_EMPTY (321)
**Description**: Queue is empty and no items available.

**Common Causes**:
- Consumer faster than producer
- Producer task not running
- Timing issues in communication

**Solutions**:
- Use appropriate timeouts
- Implement proper producer/consumer synchronization
- Check queue state before operations

## System Errors (400-499)

### PICO_RTOS_ERROR_SYSTEM_NOT_INITIALIZED (400)
**Description**: RTOS system not initialized before use.

**Common Causes**:
- Calling RTOS functions before pico_rtos_init()
- Initialization failure not detected

**Solutions**:
- Always call pico_rtos_init() before other RTOS functions
- Check initialization return value
- Implement proper startup sequence

**Example**:
```c
int main() {
    // Always initialize first
    if (!pico_rtos_init()) {
        printf("RTOS initialization failed!\n");
        return -1;
    }
    
    // Now safe to use other RTOS functions
    // ... create tasks, etc.
    
    pico_rtos_start();
    return 0;
}
```

### PICO_RTOS_ERROR_INVALID_CONFIGURATION (402)
**Description**: Invalid system configuration detected.

**Common Causes**:
- Conflicting configuration options
- Invalid parameter values
- Missing required configuration

**Solutions**:
- Validate configuration at build time
- Use recommended configuration values
- Check configuration dependencies

## Hardware Errors (500-599)

### PICO_RTOS_ERROR_HARDWARE_FAULT (500)
**Description**: General hardware fault detected.

**Common Causes**:
- Hardware malfunction
- Incorrect hardware configuration
- Power supply issues

**Solutions**:
- Check hardware connections
- Verify power supply stability
- Review hardware configuration
- Implement hardware fault handlers

### PICO_RTOS_ERROR_HARDWARE_TIMER_FAULT (501)
**Description**: Hardware timer fault or misconfiguration.

**Common Causes**:
- Timer hardware not properly initialized
- Clock configuration issues
- Timer overflow conditions

**Solutions**:
- Verify timer initialization
- Check clock configuration
- Implement timer overflow handling

## Configuration Errors (600-699)

### PICO_RTOS_ERROR_CONFIG_INVALID_TICK_RATE (600)
**Description**: Invalid system tick rate configuration.

**Common Causes**:
- Tick rate outside valid range (10-10000 Hz)
- Tick rate not supported by hardware
- Clock configuration conflicts

**Solutions**:
- Use predefined tick rate constants
- Verify hardware timer capabilities
- Check clock configuration

**Example**:
```c
// Use predefined constants
#define PICO_RTOS_TICK_RATE_HZ PICO_RTOS_TICK_RATE_HZ_1000  // 1000 Hz

// Or validate custom values
#if PICO_RTOS_TICK_RATE_HZ < 10 || PICO_RTOS_TICK_RATE_HZ > 10000
#error "Invalid tick rate configuration"
#endif
```

### PICO_RTOS_ERROR_CONFIG_FEATURE_DISABLED (603)
**Description**: Attempt to use disabled feature.

**Common Causes**:
- Using logging functions when logging disabled
- Using error history when feature disabled
- Feature-specific functions called without enabling feature

**Solutions**:
- Enable required features in configuration
- Use conditional compilation for optional features
- Check feature availability at runtime

## Error Handling Best Practices

### 1. Always Check Return Values
```c
// Good practice - check return values
if (!pico_rtos_task_create(&task, "MyTask", task_func, NULL, 1024, 3)) {
    pico_rtos_error_info_t error;
    if (pico_rtos_get_last_error(&error)) {
        printf("Task creation failed: %s\n", error.description);
    }
    return false;
}
```

### 2. Use Error Reporting Macros
```c
// Report errors with context
if (priority > MAX_PRIORITY) {
    PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_TASK_INVALID_PRIORITY, priority);
    return false;
}
```

### 3. Implement Error Callbacks
```c
void my_error_handler(const pico_rtos_error_info_t *error) {
    printf("Error %d at %s:%d - %s\n", 
           error->code, error->file, error->line, error->description);
    
    // Implement recovery logic based on error type
    switch (error->code) {
        case PICO_RTOS_ERROR_OUT_OF_MEMORY:
            // Trigger garbage collection or reduce memory usage
            break;
        case PICO_RTOS_ERROR_TASK_STACK_OVERFLOW:
            // Log stack usage and restart task with larger stack
            break;
        default:
            // General error handling
            break;
    }
}

// Register error callback
pico_rtos_set_error_callback(my_error_handler);
```

### 4. Monitor Error Statistics
```c
void monitor_errors(void) {
    pico_rtos_error_stats_t stats;
    pico_rtos_get_error_stats(&stats);
    
    if (stats.total_errors > 0) {
        printf("Error Summary:\n");
        printf("  Total: %lu\n", stats.total_errors);
        printf("  Task: %lu, Memory: %lu, Sync: %lu\n",
               stats.task_errors, stats.memory_errors, stats.sync_errors);
        printf("  System: %lu, Hardware: %lu, Config: %lu\n",
               stats.system_errors, stats.hardware_errors, stats.config_errors);
    }
}
```

## Debugging with Error History

When error history is enabled, you can track error patterns:

```c
void analyze_error_patterns(void) {
    pico_rtos_error_info_t errors[PICO_RTOS_ERROR_HISTORY_SIZE];
    size_t count;
    
    if (pico_rtos_get_error_history(errors, PICO_RTOS_ERROR_HISTORY_SIZE, &count)) {
        printf("Error History (%zu entries):\n", count);
        
        for (size_t i = 0; i < count; i++) {
            printf("  [%lu] %s at %s:%d (task %lu)\n",
                   errors[i].timestamp,
                   errors[i].description,
                   errors[i].file,
                   errors[i].line,
                   errors[i].task_id);
        }
        
        // Analyze patterns
        // - Frequent errors from same location
        // - Error clusters in time
        // - Task-specific error patterns
    }
}
```

This comprehensive error handling system helps you build robust applications with proper error detection, reporting, and recovery mechanisms.