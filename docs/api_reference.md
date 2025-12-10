# Pico-RTOS API Reference

This document provides a comprehensive reference for all Pico-RTOS APIs across all versions.

**Current Version**: v0.3.1 "Advanced Synchronization & Multi-Core"  
**Backward Compatibility**: 100% compatible with v0.2.1 and earlier

## 📚 API Overview

Pico-RTOS provides a rich set of APIs organized into the following categories:

### Core RTOS APIs
- System initialization and control
- Task management and scheduling
- Time and tick management
- Critical sections and interrupt control

### Synchronization Primitives
- **Mutexes**: Mutual exclusion with priority inheritance
- **Semaphores**: Counting and binary semaphores
- **Queues**: Message passing with blocking operations
- **Event Groups** *(v0.3.1)*: Multi-event synchronization
- **Stream Buffers** *(v0.3.1)*: Variable-length message passing

### Memory Management
- Dynamic memory allocation with tracking
- **Memory Pools** *(v0.3.1)*: O(1) deterministic allocation
- **MPU Support** *(v0.3.1)*: Hardware memory protection
- Stack overflow detection and monitoring

### Multi-Core Support *(v0.3.1)*
- **SMP Scheduler**: Symmetric multiprocessing
- **Load Balancing**: Automatic workload distribution
- **Core Affinity**: Task binding to specific cores
- **Inter-Core Communication**: High-performance IPC

### Debugging & Profiling *(v0.3.1)*
- **Runtime Inspection**: Task state monitoring
- **Execution Profiling**: High-resolution timing analysis
- **System Tracing**: Event logging and analysis
- **Enhanced Assertions**: Advanced debugging support

### System Extensions *(v0.3.1)*
- **I/O Abstraction**: Thread-safe peripheral access
- **High-Resolution Timers**: Microsecond precision
- **Universal Timeouts**: Consistent timeout handling
- **Multi-Level Logging**: Enhanced debug output

### Quality Assurance *(v0.3.1)*
- **Deadlock Detection**: Runtime prevention
- **Health Monitoring**: System metrics collection
- **Watchdog Integration**: Hardware reliability
- **Alert System**: Proactive monitoring

---

## 🔧 Core RTOS Functions

### System Control

#### `bool pico_rtos_init(void)`
Initialize the RTOS system.

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

**Example:**
```c
if (!pico_rtos_init()) {
    // Handle initialization failure
    return -1;
}
```

#### `void pico_rtos_start(void)`
Start the RTOS scheduler. This function does not return unless the scheduler is stopped.

**Note:** Call this after creating all initial tasks.

#### `const char *pico_rtos_get_version_string(void)`
Get the RTOS version string.

**Returns:** Version string in format "X.Y.Z"

### Time Management

#### `uint32_t pico_rtos_get_tick_count(void)`
Get the current system tick count.

**Returns:** Current system time in milliseconds since startup

#### `uint32_t pico_rtos_get_uptime_ms(void)`
Get the system uptime in milliseconds.

**Returns:** System uptime in milliseconds (thread-safe)

#### `uint32_t pico_rtos_get_tick_rate_hz(void)`
Get the current system tick frequency.

**Returns:** System tick rate in Hz (configured at build time)

### Critical Sections

#### `void pico_rtos_enter_critical(void)`
Enter a critical section (disable interrupts).

**Warning:** Keep critical sections as short as possible.

#### `void pico_rtos_exit_critical(void)`
Exit a critical section (restore interrupts).

---

## 👥 Task Management

### Task Creation and Control

#### `bool pico_rtos_task_create(pico_rtos_task_t *task, const char *name, pico_rtos_task_function_t function, void *param, uint32_t stack_size, uint32_t priority)`
Create a new task.

**Parameters:**
- `task`: Pointer to task structure
- `name`: Task name for debugging (max 15 characters)
- `function`: Task function to execute
- `param`: Parameter passed to task function
- `stack_size`: Stack size in bytes (minimum 256)
- `priority`: Task priority (0-31, higher = more priority)

**Returns:**
- `true` if task created successfully
- `false` if creation failed

**Example:**
```c
pico_rtos_task_t led_task;
bool result = pico_rtos_task_create(
    &led_task,
    "LED_Blink",
    led_task_function,
    NULL,
    1024,  // 1KB stack
    10     // Medium priority
);
```

#### `bool pico_rtos_task_delete(pico_rtos_task_t *task)`
Delete a task.

**Parameters:**
- `task`: Task to delete (NULL for current task)

**Returns:**
- `true` if deletion successful
- `false` if deletion failed

#### `void pico_rtos_task_delay(uint32_t delay_ms)`
Delay the current task for specified milliseconds.

**Parameters:**
- `delay_ms`: Delay time in milliseconds

#### `void pico_rtos_task_yield(void)`
Yield CPU to other tasks of same priority.

#### `bool pico_rtos_task_set_priority(pico_rtos_task_t *task, uint32_t priority)`
Change task priority.

**Parameters:**
- `task`: Task to modify (NULL for current task)
- `priority`: New priority (0-31)

**Returns:**
- `true` if priority changed successfully
- `false` if change failed

---

## 🔒 Synchronization Primitives

### Mutexes

#### `bool pico_rtos_mutex_init(pico_rtos_mutex_t *mutex)`
Initialize a mutex.

**Parameters:**
- `mutex`: Pointer to mutex structure

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

#### `bool pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex, uint32_t timeout_ms)`
Lock a mutex with timeout.

**Parameters:**
- `mutex`: Mutex to lock
- `timeout_ms`: Timeout in milliseconds (PICO_RTOS_WAIT_FOREVER for no timeout)

**Returns:**
- `true` if mutex locked successfully
- `false` if timeout or error occurred

#### `bool pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex)`
Unlock a mutex.

**Parameters:**
- `mutex`: Mutex to unlock

**Returns:**
- `true` if mutex unlocked successfully
- `false` if unlock failed (not owner)

### Semaphores

#### `bool pico_rtos_semaphore_init(pico_rtos_semaphore_t *semaphore, uint32_t initial_count, uint32_t max_count)`
Initialize a semaphore.

**Parameters:**
- `semaphore`: Pointer to semaphore structure
- `initial_count`: Initial count value
- `max_count`: Maximum count value

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

#### `bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *semaphore, uint32_t timeout_ms)`
Take (decrement) a semaphore.

**Parameters:**
- `semaphore`: Semaphore to take
- `timeout_ms`: Timeout in milliseconds

**Returns:**
- `true` if semaphore taken successfully
- `false` if timeout or error occurred

#### `bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *semaphore)`
Give (increment) a semaphore.

**Parameters:**
- `semaphore`: Semaphore to give

**Returns:**
- `true` if semaphore given successfully
- `false` if give failed (max count reached)

### Queues

#### `bool pico_rtos_queue_init(pico_rtos_queue_t *queue, void *buffer, uint32_t item_size, uint32_t item_count)`
Initialize a queue.

**Parameters:**
- `queue`: Pointer to queue structure
- `buffer`: Buffer for queue storage
- `item_size`: Size of each item in bytes
- `item_count`: Maximum number of items

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

#### `bool pico_rtos_queue_send(pico_rtos_queue_t *queue, const void *item, uint32_t timeout_ms)`
Send an item to the queue.

**Parameters:**
- `queue`: Queue to send to
- `item`: Pointer to item data
- `timeout_ms`: Timeout in milliseconds

**Returns:**
- `true` if item sent successfully
- `false` if timeout or error occurred

#### `bool pico_rtos_queue_receive(pico_rtos_queue_t *queue, void *item, uint32_t timeout_ms)`
Receive an item from the queue.

**Parameters:**
- `queue`: Queue to receive from
- `item`: Buffer to store received item
- `timeout_ms`: Timeout in milliseconds

**Returns:**
- `true` if item received successfully
- `false` if timeout or error occurred

---

## 🎯 Event Groups *(v0.3.1)*

Event groups provide advanced synchronization for multiple events.

### Data Structures

```c
typedef struct {
    uint32_t event_bits;                    // Current event state
    pico_rtos_block_object_t *block_obj;    // Blocking object
    critical_section_t cs;                  // Thread safety
} pico_rtos_event_group_t;
```

### Functions

#### `bool pico_rtos_event_group_init(pico_rtos_event_group_t *event_group)`
Initialize an event group.

**Parameters:**
- `event_group`: Pointer to event group structure

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

#### `bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *event_group, uint32_t bits)`
Set (activate) event bits.

**Parameters:**
- `event_group`: Event group to modify
- `bits`: Bit mask of events to set

**Returns:**
- `true` if bits set successfully
- `false` if operation failed

#### `bool pico_rtos_event_group_clear_bits(pico_rtos_event_group_t *event_group, uint32_t bits)`
Clear (deactivate) event bits.

**Parameters:**
- `event_group`: Event group to modify
- `bits`: Bit mask of events to clear

**Returns:**
- `true` if bits cleared successfully
- `false` if operation failed

#### `uint32_t pico_rtos_event_group_wait_bits(pico_rtos_event_group_t *event_group, uint32_t bits, bool wait_all, bool clear, uint32_t timeout_ms)`
Wait for event bits with configurable semantics.

**Parameters:**
- `event_group`: Event group to wait on
- `bits`: Bit mask of events to wait for
- `wait_all`: `true` for "wait all", `false` for "wait any"
- `clear`: `true` to clear bits after successful wait
- `timeout_ms`: Timeout in milliseconds

**Returns:**
- Event bits that satisfied the wait condition
- 0 if timeout or error occurred

**Example:**
```c
pico_rtos_event_group_t events;
pico_rtos_event_group_init(&events);

// Task 1: Set events
pico_rtos_event_group_set_bits(&events, 0x01 | 0x04);

// Task 2: Wait for any event
uint32_t result = pico_rtos_event_group_wait_bits(
    &events, 
    0x01 | 0x02 | 0x04,  // Wait for any of these
    false,               // Wait for ANY (not all)
    true,                // Clear bits after wait
    1000                 // 1 second timeout
);
```

---

## 📦 Stream Buffers *(v0.3.1)*

Stream buffers provide efficient variable-length message passing.

### Functions

#### `bool pico_rtos_stream_buffer_init(pico_rtos_stream_buffer_t *stream_buffer, uint8_t *buffer, uint32_t buffer_size)`
Initialize a stream buffer.

**Parameters:**
- `stream_buffer`: Pointer to stream buffer structure
- `buffer`: Buffer for data storage
- `buffer_size`: Size of buffer in bytes

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

#### `uint32_t pico_rtos_stream_buffer_send(pico_rtos_stream_buffer_t *stream_buffer, const void *data, uint32_t data_size, uint32_t timeout_ms)`
Send data to stream buffer.

**Parameters:**
- `stream_buffer`: Stream buffer to send to
- `data`: Pointer to data to send
- `data_size`: Size of data in bytes
- `timeout_ms`: Timeout in milliseconds

**Returns:**
- Number of bytes sent
- 0 if timeout or error occurred

#### `uint32_t pico_rtos_stream_buffer_receive(pico_rtos_stream_buffer_t *stream_buffer, void *data, uint32_t buffer_size, uint32_t timeout_ms)`
Receive data from stream buffer.

**Parameters:**
- `stream_buffer`: Stream buffer to receive from
- `data`: Buffer to store received data
- `buffer_size`: Size of receive buffer
- `timeout_ms`: Timeout in milliseconds

**Returns:**
- Number of bytes received
- 0 if timeout or error occurred

---

## 🧠 Memory Management

### Dynamic Memory

#### `void *pico_rtos_malloc(size_t size)`
Allocate memory with tracking.

**Parameters:**
- `size`: Size in bytes to allocate

**Returns:**
- Pointer to allocated memory
- `NULL` if allocation failed

#### `void pico_rtos_free(void *ptr)`
Free previously allocated memory.

**Parameters:**
- `ptr`: Pointer to memory to free

### Memory Pools *(v0.3.1)*

#### `bool pico_rtos_memory_pool_init(pico_rtos_memory_pool_t *pool, void *buffer, uint32_t block_size, uint32_t block_count)`
Initialize a memory pool.

**Parameters:**
- `pool`: Pointer to memory pool structure
- `buffer`: Buffer for pool storage
- `block_size`: Size of each block
- `block_count`: Number of blocks

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

#### `void *pico_rtos_memory_pool_alloc(pico_rtos_memory_pool_t *pool, uint32_t timeout_ms)`
Allocate a block from memory pool.

**Parameters:**
- `pool`: Memory pool to allocate from
- `timeout_ms`: Timeout in milliseconds

**Returns:**
- Pointer to allocated block
- `NULL` if timeout or error occurred

#### `bool pico_rtos_memory_pool_free(pico_rtos_memory_pool_t *pool, void *block)`
Free a block back to memory pool.

**Parameters:**
- `pool`: Memory pool to return block to
- `block`: Block to free

**Returns:**
- `true` if block freed successfully
- `false` if free failed

---

## 🔄 Multi-Core Support *(v0.3.1)*

### SMP Scheduler

#### `bool pico_rtos_smp_init(void)`
Initialize SMP scheduler for dual-core operation.

**Returns:**
- `true` if SMP initialization successful
- `false` if initialization failed

#### `bool pico_rtos_task_set_core_affinity(pico_rtos_task_t *task, uint32_t core_mask)`
Set task core affinity.

**Parameters:**
- `task`: Task to modify (NULL for current task)
- `core_mask`: Bitmask of allowed cores (bit 0 = core 0, bit 1 = core 1)

**Returns:**
- `true` if affinity set successfully
- `false` if operation failed

### Inter-Core Communication

#### `bool pico_rtos_ipc_channel_init(pico_rtos_ipc_channel_t *channel, void *buffer, uint32_t buffer_size)`
Initialize inter-core communication channel.

**Parameters:**
- `channel`: Pointer to IPC channel structure
- `buffer`: Buffer for message storage
- `buffer_size`: Size of buffer in bytes

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

#### `bool pico_rtos_ipc_send(pico_rtos_ipc_channel_t *channel, const void *message, uint32_t message_size, uint32_t timeout_ms)`
Send message to another core.

**Parameters:**
- `channel`: IPC channel to send on
- `message`: Message data
- `message_size`: Size of message
- `timeout_ms`: Timeout in milliseconds

**Returns:**
- `true` if message sent successfully
- `false` if timeout or error occurred

---

## 🔍 Debugging & Profiling *(v0.3.1)*

### Task Inspection

#### `bool pico_rtos_debug_get_task_info(pico_rtos_task_t *task, pico_rtos_task_info_t *info)`
Get detailed task information.

**Parameters:**
- `task`: Task to inspect (NULL for current task)
- `info`: Structure to fill with task information

**Returns:**
- `true` if information retrieved successfully
- `false` if operation failed

### Execution Profiling

#### `bool pico_rtos_profiler_start(void)`
Start execution profiling.

**Returns:**
- `true` if profiling started successfully
- `false` if start failed

#### `bool pico_rtos_profiler_stop(void)`
Stop execution profiling.

**Returns:**
- `true` if profiling stopped successfully
- `false` if stop failed

#### `bool pico_rtos_profiler_get_stats(pico_rtos_profiler_stats_t *stats)`
Get profiling statistics.

**Parameters:**
- `stats`: Structure to fill with profiling data

**Returns:**
- `true` if statistics retrieved successfully
- `false` if operation failed

---

## 📊 System Monitoring

### System Statistics

#### `bool pico_rtos_get_system_stats(pico_rtos_system_stats_t *stats)`
Get comprehensive system statistics.

**Parameters:**
- `stats`: Structure to fill with system statistics

**Returns:**
- `true` if statistics retrieved successfully
- `false` if operation failed

### Health Monitoring *(v0.3.1)*

#### `bool pico_rtos_health_monitor_init(void)`
Initialize system health monitoring.

**Returns:**
- `true` if initialization successful
- `false` if initialization failed

#### `bool pico_rtos_health_get_metrics(pico_rtos_health_metrics_t *metrics)`
Get current health metrics.

**Parameters:**
- `metrics`: Structure to fill with health data

**Returns:**
- `true` if metrics retrieved successfully
- `false` if operation failed

---

## 🚨 Error Handling

### Error Codes

All functions return appropriate error codes. Common error codes include:

- `PICO_RTOS_ERROR_NONE` (0): Success
- `PICO_RTOS_ERROR_INVALID_PARAM`: Invalid parameter
- `PICO_RTOS_ERROR_TIMEOUT`: Operation timed out
- `PICO_RTOS_ERROR_NO_MEMORY`: Insufficient memory
- `PICO_RTOS_ERROR_RESOURCE_BUSY`: Resource is busy

### Error History

#### `bool pico_rtos_get_error_history(pico_rtos_error_entry_t *entries, uint32_t *count)`
Get recent error history.

**Parameters:**
- `entries`: Array to fill with error entries
- `count`: Pointer to count (input: array size, output: entries filled)

**Returns:**
- `true` if history retrieved successfully
- `false` if operation failed

---

## 🔧 Configuration Constants

### Timeouts
- `PICO_RTOS_WAIT_FOREVER`: Wait indefinitely
- `PICO_RTOS_NO_WAIT`: Don't wait (immediate return)

### Priorities
- `PICO_RTOS_PRIORITY_IDLE`: Idle task priority (0)
- `PICO_RTOS_PRIORITY_LOW`: Low priority (8)
- `PICO_RTOS_PRIORITY_NORMAL`: Normal priority (16)
- `PICO_RTOS_PRIORITY_HIGH`: High priority (24)
- `PICO_RTOS_PRIORITY_CRITICAL`: Critical priority (31)

### Event Group Constants
- `PICO_RTOS_EVENT_BIT_0` to `PICO_RTOS_EVENT_BIT_31`: Individual event bits
- `PICO_RTOS_EVENT_ALL_BITS`: All event bits (0xFFFFFFFF)

---

## 📝 Usage Examples

### Basic Task Creation
```c
#include "pico_rtos.h"

void led_task(void *param) {
    while (1) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        pico_rtos_task_delay(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        pico_rtos_task_delay(500);
    }
}

int main() {
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    if (!pico_rtos_init()) {
        return -1;
    }
    
    pico_rtos_task_t led_task_handle;
    pico_rtos_task_create(
        &led_task_handle,
        "LED",
        led_task,
        NULL,
        1024,
        PICO_RTOS_PRIORITY_NORMAL
    );
    
    pico_rtos_start();
    return 0;
}
```

### Multi-Core Example *(v0.3.1)*
```c
void core0_task(void *param) {
    // Task runs on core 0
    while (1) {
        printf("Core 0 task running\n");
        pico_rtos_task_delay(1000);
    }
}

void core1_task(void *param) {
    // Task runs on core 1
    while (1) {
        printf("Core 1 task running\n");
        pico_rtos_task_delay(1000);
    }
}

int main() {
    pico_rtos_init();
    pico_rtos_smp_init();  // Enable multi-core
    
    pico_rtos_task_t task0, task1;
    
    // Create task for core 0
    pico_rtos_task_create(&task0, "Core0", core0_task, NULL, 1024, 10);
    pico_rtos_task_set_core_affinity(&task0, 0x01);  // Core 0 only
    
    // Create task for core 1
    pico_rtos_task_create(&task1, "Core1", core1_task, NULL, 1024, 10);
    pico_rtos_task_set_core_affinity(&task1, 0x02);  // Core 1 only
    
    pico_rtos_start();
    return 0;
}
```

---

## 🔄 Version Compatibility

### v0.3.1 Compatibility
- **100% backward compatible** with v0.2.1 and earlier
- All existing APIs continue to work unchanged
- New features are additive and optional
- Configuration migration tools available

### Migration Notes
- No code changes required for existing applications
- New features can be adopted incrementally
- See [Migration Guide](migration_guide.md) for details

---

**API Reference Version**: v0.3.1  
**Last Updated**: July 25, 2025  
**Compatibility**: v0.1.0, v0.2.0, v0.2.1, v0.3.1