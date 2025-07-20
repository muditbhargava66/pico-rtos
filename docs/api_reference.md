# Pico-RTOS API Reference

This document provides a comprehensive reference for the Pico-RTOS API.

## Version 0.2.1 - Enhanced Developer Experience

This API reference covers all functions available in Pico-RTOS v0.2.1, which builds upon the production-ready v0.2.0 foundation with enhanced developer experience, comprehensive examples, configurable system options, and improved debugging capabilities.

## Core RTOS Functions

- `bool pico_rtos_init(void)`
  - Initialize the RTOS system.
  - Returns `true` if initialization was successful, `false` otherwise.

- `void pico_rtos_start(void)`
  - Start the RTOS scheduler. This function does not return unless the scheduler is stopped.

- `uint32_t pico_rtos_get_tick_count(void)`
  - Get the current system tick count (milliseconds since startup).
  - Returns the current system time in milliseconds.

- `uint32_t pico_rtos_get_uptime_ms(void)`
  - Get the system uptime in milliseconds.
  - Returns the system uptime in milliseconds.

- `const char *pico_rtos_get_version_string(void)`
  - Get the RTOS version string.
  - Returns the version string in format "X.Y.Z".

- `uint32_t pico_rtos_get_tick_rate_hz(void)`
  - Get the current system tick frequency.
  - Returns the system tick rate in Hz (configured at build time).

- `void pico_rtos_enter_critical(void)`
  - Enter a critical section (disable interrupts).

- `void pico_rtos_exit_critical(void)`
  - Exit a critical section (restore interrupts).

## Task Management

- `bool pico_rtos_task_create(pico_rtos_task_t *task, const char *name, pico_rtos_task_function_t function, void *param, uint32_t stack_size, uint32_t priority)`
  - Create a new task.
  - Parameters:
    - `task`: Pointer to task structure.
    - `name`: Name of the task (for debugging).
    - `function`: Task function to execute.
    - `param`: Parameter to pass to the task function.
    - `stack_size`: Size of the task's stack in bytes.
    - `priority`: Priority of the task (higher number = higher priority).
  - Returns `true` if task creation was successful, `false` otherwise.

- `void pico_rtos_task_suspend(pico_rtos_task_t *task)`
  - Suspend a task.
  - Parameters:
    - `task`: Task to suspend, or `NULL` for current task.

- `void pico_rtos_task_resume(pico_rtos_task_t *task)`
  - Resume a suspended task.
  - Parameters:
    - `task`: Task to resume.

- `void pico_rtos_task_delay(uint32_t ms)`
  - Delay the current task for the specified number of milliseconds.
  - Parameters:
    - `ms`: Number of milliseconds to delay.

- `pico_rtos_task_t *pico_rtos_get_current_task(void)`
  - Get the current running task.
  - Returns pointer to the current task.

- `const char *pico_rtos_task_get_state_string(pico_rtos_task_t *task)`
  - Get task state as a string (for debugging).
  - Parameters:
    - `task`: Task to get state for.
  - Returns string representation of the task state.

- `void pico_rtos_task_delete(pico_rtos_task_t *task)`
  - Delete a task and free its resources.
  - Parameters:
    - `task`: Task to delete.

- `void pico_rtos_task_yield(void)`
  - Yield the current task (give up CPU voluntarily).
  - This allows other tasks of the same priority to run.

- `bool pico_rtos_task_set_priority(pico_rtos_task_t *task, uint32_t new_priority)`
  - Change a task's priority.
  - Parameters:
    - `task`: Task to change priority for, or `NULL` for current task.
    - `new_priority`: New priority value.
  - Returns `true` if priority was changed successfully, `false` otherwise.

## Mutex

- `bool pico_rtos_mutex_init(pico_rtos_mutex_t *mutex)`
  - Initialize a mutex.
  - Parameters:
    - `mutex`: Pointer to mutex structure.
  - Returns `true` if initialization was successful, `false` otherwise.

- `bool pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex, uint32_t timeout)`
  - Acquire a mutex lock.
  - Parameters:
    - `mutex`: Pointer to mutex structure.
    - `timeout`: Timeout in milliseconds, `PICO_RTOS_WAIT_FOREVER` to wait forever, or `PICO_RTOS_NO_WAIT` for immediate return.
  - Returns `true` if mutex was acquired successfully, `false` if timeout occurred.

- `bool pico_rtos_mutex_try_lock(pico_rtos_mutex_t *mutex)`
  - Try to acquire a mutex without blocking.
  - Parameters:
    - `mutex`: Pointer to mutex structure.
  - Returns `true` if mutex was acquired successfully, `false` otherwise.

- `bool pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex)`
  - Release a mutex lock.
  - Parameters:
    - `mutex`: Pointer to mutex structure.
  - Returns `true` if mutex was released successfully, `false` if the calling task is not the owner of the mutex.

- `pico_rtos_task_t *pico_rtos_mutex_get_owner(pico_rtos_mutex_t *mutex)`
  - Get the current owner of the mutex.
  - Parameters:
    - `mutex`: Pointer to mutex structure.
  - Returns pointer to the task that owns the mutex, or `NULL` if not owned.

- `void pico_rtos_mutex_delete(pico_rtos_mutex_t *mutex)`
  - Delete a mutex and free associated resources.
  - Parameters:
    - `mutex`: Pointer to mutex structure.

## Queue

- `bool pico_rtos_queue_init(pico_rtos_queue_t *queue, void *buffer, size_t item_size, size_t max_items)`
  - Initialize a queue.
  - Parameters:
    - `queue`: Pointer to queue structure.
    - `buffer`: Buffer to store queue items.
    - `item_size`: Size of each item in bytes.
    - `max_items`: Maximum number of items in the queue.
  - Returns `true` if initialized successfully, `false` otherwise.

- `bool pico_rtos_queue_send(pico_rtos_queue_t *queue, const void *item, uint32_t timeout)`
  - Send an item to the queue.
  - Parameters:
    - `queue`: Pointer to queue structure.
    - `item`: Pointer to the item to send.
    - `timeout`: Timeout in milliseconds, `PICO_RTOS_WAIT_FOREVER` to wait forever, or `PICO_RTOS_NO_WAIT` for immediate return.
  - Returns `true` if item was sent successfully, `false` if timeout occurred.

- `bool pico_rtos_queue_receive(pico_rtos_queue_t *queue, void *item, uint32_t timeout)`
  - Receive an item from the queue.
  - Parameters:
    - `queue`: Pointer to queue structure.
    - `item`: Pointer to store the received item.
    - `timeout`: Timeout in milliseconds, `PICO_RTOS_WAIT_FOREVER` to wait forever, or `PICO_RTOS_NO_WAIT` for immediate return.
  - Returns `true` if item was received successfully, `false` if timeout occurred.

- `bool pico_rtos_queue_is_empty(pico_rtos_queue_t *queue)`
  - Check if a queue is empty.
  - Parameters:
    - `queue`: Pointer to queue structure.
  - Returns `true` if queue is empty, `false` otherwise.

- `bool pico_rtos_queue_is_full(pico_rtos_queue_t *queue)`
  - Check if a queue is full.
  - Parameters:
    - `queue`: Pointer to queue structure.
  - Returns `true` if queue is full, `false` otherwise.

- `void pico_rtos_queue_delete(pico_rtos_queue_t *queue)`
  - Delete a queue and free associated resources.
  - Parameters:
    - `queue`: Pointer to queue structure.

## Semaphore

- `bool pico_rtos_semaphore_init(pico_rtos_semaphore_t *semaphore, uint32_t initial_count, uint32_t max_count)`
  - Initialize a semaphore.
  - Parameters:
    - `semaphore`: Pointer to semaphore structure.
    - `initial_count`: Initial count value (0 for binary semaphore).
    - `max_count`: Maximum count value (1 for binary semaphore).
  - Returns `true` if initialized successfully, `false` otherwise.

- `bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *semaphore)`
  - Give (increment) a semaphore.
  - Parameters:
    - `semaphore`: Pointer to semaphore structure.
  - Returns `true` if semaphore was given successfully, `false` if at max_count.

- `bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *semaphore, uint32_t timeout)`
  - Take (decrement) a semaphore.
  - Parameters:
    - `semaphore`: Pointer to semaphore structure.
    - `timeout`: Timeout in milliseconds, `PICO_RTOS_WAIT_FOREVER` to wait forever, or `PICO_RTOS_NO_WAIT` for immediate return.
  - Returns `true` if semaphore was taken successfully, `false` if timeout occurred.

- `bool pico_rtos_semaphore_is_available(pico_rtos_semaphore_t *semaphore)`
  - Check if a semaphore can be taken without blocking.
  - Parameters:
    - `semaphore`: Pointer to semaphore structure.
  - Returns `true` if semaphore can be taken without blocking, `false` otherwise.

- `void pico_rtos_semaphore_delete(pico_rtos_semaphore_t *semaphore)`
  - Delete a semaphore and free associated resources.
  - Parameters:
    - `semaphore`: Pointer to semaphore structure.

## Timer

- `bool pico_rtos_timer_init(pico_rtos_timer_t *timer, const char *name, pico_rtos_timer_callback_t callback, void *param, uint32_t period, bool auto_reload)`
  - Initialize a timer.
  - Parameters:
    - `timer`: Pointer to timer structure.
    - `name`: Name of the timer (for debugging).
    - `callback`: Function to call when timer expires.
    - `param`: Parameter to pass to the callback function.
    - `period`: Period of the timer in milliseconds.
    - `auto_reload`: Whether the timer should automatically reload after expiration.
  - Returns `true` if initialization was successful, `false` otherwise.

- `bool pico_rtos_timer_start(pico_rtos_timer_t *timer)`
  - Start a timer.
  - Parameters:
    - `timer`: Pointer to timer structure.
  - Returns `true` if timer was started successfully, `false` otherwise.

- `bool pico_rtos_timer_stop(pico_rtos_timer_t *timer)`
  - Stop a timer.
  - Parameters:
    - `timer`: Pointer to timer structure.
  - Returns `true` if timer was stopped successfully, `false` otherwise.

- `bool pico_rtos_timer_reset(pico_rtos_timer_t *timer)`
  - Reset a timer.
  - Parameters:
    - `timer`: Pointer to timer structure.
  - Returns `true` if timer was reset successfully, `false` otherwise.

- `bool pico_rtos_timer_change_period(pico_rtos_timer_t *timer, uint32_t period)`
  - Change the period of a timer.
  - Parameters:
    - `timer`: Pointer to timer structure.
    - `period`: New period in milliseconds.
  - Returns `true` if period was changed successfully, `false` otherwise.

- `bool pico_rtos_timer_is_running(pico_rtos_timer_t *timer)`
  - Check if a timer is running.
  - Parameters:
    - `timer`: Pointer to timer structure.
  - Returns `true` if timer is running, `false` otherwise.

- `bool pico_rtos_timer_is_expired(pico_rtos_timer_t *timer)`
  - Check if a timer has expired.
  - Parameters:
    - `timer`: Pointer to timer structure.
  - Returns `true` if timer has expired, `false` otherwise.

- `uint32_t pico_rtos_timer_get_remaining_time(pico_rtos_timer_t *timer)`
  - Get remaining time until timer expiration.
  - Parameters:
    - `timer`: Pointer to timer structure.
  - Returns remaining time in milliseconds.

- `void pico_rtos_timer_delete(pico_rtos_timer_t *timer)`
  - Delete a timer and free associated resources.
  - Parameters:
    - `timer`: Pointer to timer structure.

## Memory Management

- `void *pico_rtos_malloc(size_t size)`
  - Allocate memory with tracking.
  - Parameters:
    - `size`: Size of memory to allocate in bytes.
  - Returns pointer to allocated memory, or `NULL` if allocation failed.

- `void pico_rtos_free(void *ptr, size_t size)`
  - Free previously allocated memory.
  - Parameters:
    - `ptr`: Pointer to memory to free.
    - `size`: Size of memory being freed.

- `void pico_rtos_get_memory_stats(uint32_t *current, uint32_t *peak, uint32_t *allocations)`
  - Get memory usage statistics.
  - Parameters:
    - `current`: Pointer to store current memory usage.
    - `peak`: Pointer to store peak memory usage.
    - `allocations`: Pointer to store total allocation count.

## System Monitoring

- `void pico_rtos_get_system_stats(pico_rtos_system_stats_t *stats)`
  - Get comprehensive system statistics.
  - Parameters:
    - `stats`: Pointer to structure to store system statistics.

- `uint32_t pico_rtos_get_idle_counter(void)`
  - Get idle task counter for CPU usage statistics.
  - Returns idle counter value.

## Stack Overflow Protection

- `void pico_rtos_check_stack_overflow(void)`
  - Check all tasks for stack overflow.
  - Called automatically by idle task.

- `void pico_rtos_handle_stack_overflow(pico_rtos_task_t *task)`
  - Handle stack overflow (weak function, can be overridden).
  - Parameters:
    - `task`: Task that experienced stack overflow.

## Interrupt Handling

- `void pico_rtos_interrupt_enter(void)`
  - Called when entering an interrupt.
  - Tracks interrupt nesting level.

- `void pico_rtos_interrupt_exit(void)`
  - Called when exiting an interrupt.
  - Handles deferred context switches.

- `void pico_rtos_request_context_switch(void)`
  - Request a context switch (interrupt-safe).
  - Defers context switch if in interrupt context.

## Enhanced Error Reporting

- `bool pico_rtos_error_init(void)`
  - Initialize the error reporting system.
  - Returns `true` if initialization successful, `false` otherwise.

- `void pico_rtos_report_error_detailed(pico_rtos_error_t code, const char *file, int line, const char *function, uint32_t context_data)`
  - Report an error with detailed context information.
  - Typically called through the `PICO_RTOS_REPORT_ERROR` macro.
  - Parameters:
    - `code`: Error code to report.
    - `file`: Source file name where error occurred.
    - `line`: Line number where error occurred.
    - `function`: Function name where error occurred.
    - `context_data`: Additional context-specific data.

- `const char *pico_rtos_get_error_description(pico_rtos_error_t code)`
  - Get human-readable description for an error code.
  - Parameters:
    - `code`: Error code to get description for.
  - Returns pointer to static string containing error description.

- `bool pico_rtos_get_last_error(pico_rtos_error_info_t *error_info)`
  - Get information about the last error that occurred.
  - Parameters:
    - `error_info`: Pointer to structure to fill with error information.
  - Returns `true` if error information was available, `false` if no errors occurred.

- `void pico_rtos_clear_last_error(void)`
  - Clear the last error information.
  - Resets the last error to `PICO_RTOS_ERROR_NONE`.

- `void pico_rtos_get_error_stats(pico_rtos_error_stats_t *stats)`
  - Get comprehensive error statistics.
  - Parameters:
    - `stats`: Pointer to structure to fill with error statistics.

- `void pico_rtos_set_error_callback(pico_rtos_error_callback_t callback)`
  - Set error callback function for custom error handling.
  - Parameters:
    - `callback`: Pointer to callback function, or `NULL` to disable callbacks.

### Error History Functions (if enabled)

- `bool pico_rtos_get_error_history(pico_rtos_error_info_t *errors, size_t max_count, size_t *actual_count)`
  - Get error history from the circular buffer.
  - Parameters:
    - `errors`: Array to store error information.
    - `max_count`: Maximum number of errors to retrieve.
    - `actual_count`: Pointer to store actual number of errors retrieved.
  - Returns `true` if successful, `false` if invalid parameters.

- `size_t pico_rtos_get_error_count(void)`
  - Get the number of errors currently stored in history.
  - Returns number of errors in history buffer.

- `void pico_rtos_clear_error_history(void)`
  - Clear all error history.
  - Removes all errors from the history buffer.

## Debug Logging System

### Logging Configuration Functions

- `void pico_rtos_log_init(pico_rtos_log_output_func_t output_func)`
  - Initialize the logging system.
  - Parameters:
    - `output_func`: Function to handle log output (can be `NULL` for no output).

- `void pico_rtos_log_set_level(pico_rtos_log_level_t level)`
  - Set the current log level.
  - Parameters:
    - `level`: New log level (only messages at or below this level will be processed).

- `pico_rtos_log_level_t pico_rtos_log_get_level(void)`
  - Get the current log level.
  - Returns current log level.

- `void pico_rtos_log_enable_subsystem(uint32_t subsystem_mask)`
  - Enable logging for specific subsystems.
  - Parameters:
    - `subsystem_mask`: Bitwise OR of subsystems to enable.

- `void pico_rtos_log_disable_subsystem(uint32_t subsystem_mask)`
  - Disable logging for specific subsystems.
  - Parameters:
    - `subsystem_mask`: Bitwise OR of subsystems to disable.

- `bool pico_rtos_log_is_subsystem_enabled(pico_rtos_log_subsystem_t subsystem)`
  - Check if a subsystem is enabled for logging.
  - Parameters:
    - `subsystem`: Subsystem to check.
  - Returns `true` if enabled, `false` otherwise.

### Logging Output Functions

- `void pico_rtos_log(pico_rtos_log_level_t level, pico_rtos_log_subsystem_t subsystem, const char *file, int line, const char *format, ...)`
  - Core logging function (typically not called directly).
  - Parameters:
    - `level`: Log level.
    - `subsystem`: Originating subsystem.
    - `file`: Source file name.
    - `line`: Source line number.
    - `format`: Printf-style format string.
    - `...`: Format arguments.

- `void pico_rtos_log_default_output(const pico_rtos_log_entry_t *entry)`
  - Default log output function that prints to stdout.
  - Parameters:
    - `entry`: Pointer to the log entry.

- `void pico_rtos_log_compact_output(const pico_rtos_log_entry_t *entry)`
  - Compact log output function for resource-constrained environments.
  - Parameters:
    - `entry`: Pointer to the log entry.

### Logging Utility Functions

- `const char *pico_rtos_log_level_to_string(pico_rtos_log_level_t level)`
  - Get string representation of log level.
  - Parameters:
    - `level`: Log level.
  - Returns string representation.

- `const char *pico_rtos_log_subsystem_to_string(pico_rtos_log_subsystem_t subsystem)`
  - Get string representation of subsystem.
  - Parameters:
    - `subsystem`: Subsystem.
  - Returns string representation.

## Idle Task Hook Support

- `void pico_rtos_set_idle_hook(pico_rtos_idle_hook_t hook)`
  - Set idle task hook function for power management.
  - Parameters:
    - `hook`: Function to call during idle periods.

- `void pico_rtos_clear_idle_hook(void)`
  - Clear the idle task hook function.
  - Removes any previously set idle hook.

## System Statistics Structure

```c
typedef struct {
    uint32_t total_tasks;        // Total number of tasks
    uint32_t ready_tasks;        // Number of ready tasks
    uint32_t blocked_tasks;      // Number of blocked tasks
    uint32_t suspended_tasks;    // Number of suspended tasks
    uint32_t terminated_tasks;   // Number of terminated tasks
    uint32_t current_memory;     // Current memory usage
    uint32_t peak_memory;        // Peak memory usage
    uint32_t total_allocations;  // Total allocations made
    uint32_t idle_counter;       // Idle task counter
    uint32_t system_uptime;      // System uptime in ms
} pico_rtos_system_stats_t;
```

## Error Information Structure

```c
typedef struct {
    pico_rtos_error_t code;          // Error code
    uint32_t timestamp;              // System tick when error occurred
    uint32_t task_id;                // ID of task where error occurred
    const char *file;                // Source file where error was reported
    int line;                        // Line number where error was reported
    const char *function;            // Function name where error occurred
    const char *description;         // Human-readable error description
    uint32_t context_data;           // Additional context-specific data
} pico_rtos_error_info_t;
```

## Log Entry Structure

```c
typedef struct {
    uint32_t timestamp;                                     // System timestamp in ticks
    pico_rtos_log_level_t level;                           // Log level
    pico_rtos_log_subsystem_t subsystem;                   // Originating subsystem
    uint32_t task_id;                                      // Task ID (0 for ISR context)
    char message[PICO_RTOS_LOG_MESSAGE_MAX_LENGTH];        // Formatted message
} pico_rtos_log_entry_t;
```

## Constants

- `PICO_RTOS_WAIT_FOREVER`: Wait indefinitely for a resource.
- `PICO_RTOS_NO_WAIT`: Do not wait for a resource.

## Types

- `pico_rtos_task_function_t`: Type for task functions.
- `pico_rtos_timer_callback_t`: Type for timer callback functions.
- `pico_rtos_system_stats_t`: System statistics structure.

## Version Information

- `PICO_RTOS_VERSION_MAJOR`: Major version number (0).
- `PICO_RTOS_VERSION_MINOR`: Minor version number (2).
- `PICO_RTOS_VERSION_PATCH`: Patch version number (1).

## Configuration Constants

### System Tick Frequencies
- `PICO_RTOS_TICK_RATE_HZ_100`: 100 Hz tick rate
- `PICO_RTOS_TICK_RATE_HZ_250`: 250 Hz tick rate
- `PICO_RTOS_TICK_RATE_HZ_500`: 500 Hz tick rate
- `PICO_RTOS_TICK_RATE_HZ_1000`: 1000 Hz tick rate (default)
- `PICO_RTOS_TICK_RATE_HZ_2000`: 2000 Hz tick rate

### Log Levels
- `PICO_RTOS_LOG_LEVEL_NONE`: No logging
- `PICO_RTOS_LOG_LEVEL_ERROR`: Error conditions only
- `PICO_RTOS_LOG_LEVEL_WARN`: Warnings and errors
- `PICO_RTOS_LOG_LEVEL_INFO`: Informational messages, warnings, and errors
- `PICO_RTOS_LOG_LEVEL_DEBUG`: All messages including debug information

### Log Subsystems
- `PICO_RTOS_LOG_SUBSYSTEM_CORE`: Core scheduler functions
- `PICO_RTOS_LOG_SUBSYSTEM_TASK`: Task management
- `PICO_RTOS_LOG_SUBSYSTEM_MUTEX`: Mutex operations
- `PICO_RTOS_LOG_SUBSYSTEM_QUEUE`: Queue operations
- `PICO_RTOS_LOG_SUBSYSTEM_TIMER`: Timer operations
- `PICO_RTOS_LOG_SUBSYSTEM_MEMORY`: Memory management
- `PICO_RTOS_LOG_SUBSYSTEM_SEMAPHORE`: Semaphore operations
- `PICO_RTOS_LOG_SUBSYSTEM_ALL`: All subsystems

## Error Reporting Macros

- `PICO_RTOS_REPORT_ERROR(code, context_data)`: Report an error with full context information
- `PICO_RTOS_REPORT_ERROR_SIMPLE(code)`: Report an error with simple context

## Logging Macros

### General Logging Macros
- `PICO_RTOS_LOG_ERROR(subsystem, format, ...)`: Log an error message
- `PICO_RTOS_LOG_WARN(subsystem, format, ...)`: Log a warning message
- `PICO_RTOS_LOG_INFO(subsystem, format, ...)`: Log an informational message
- `PICO_RTOS_LOG_DEBUG(subsystem, format, ...)`: Log a debug message

### Subsystem-Specific Convenience Macros
- `PICO_RTOS_LOG_CORE_ERROR/WARN/INFO/DEBUG(format, ...)`: Core subsystem logging
- `PICO_RTOS_LOG_TASK_ERROR/WARN/INFO/DEBUG(format, ...)`: Task subsystem logging
- `PICO_RTOS_LOG_MUTEX_ERROR/WARN/INFO/DEBUG(format, ...)`: Mutex subsystem logging
- `PICO_RTOS_LOG_QUEUE_ERROR/WARN/INFO/DEBUG(format, ...)`: Queue subsystem logging
- `PICO_RTOS_LOG_TIMER_ERROR/WARN/INFO/DEBUG(format, ...)`: Timer subsystem logging
- `PICO_RTOS_LOG_MEMORY_ERROR/WARN/INFO/DEBUG(format, ...)`: Memory subsystem logging
- `PICO_RTOS_LOG_SEM_ERROR/WARN/INFO/DEBUG(format, ...)`: Semaphore subsystem logging

---