# Pico-RTOS v0.3.1 API Reference

This document provides a comprehensive reference for the Pico-RTOS v0.3.1 API.

## Table of Contents

1. [Core System](#core-system)
2. [Task Management](#task-management)
3. [Synchronization Primitives](#synchronization-primitives)
    - [Mutexes](#mutexes)
    - [Semaphores](#semaphores)
    - [Queues](#queues)
    - [Event Groups](#event-groups)
    - [Stream Buffers](#stream-buffers)
4. [Memory Management](#memory-management)
    - [Memory Pools](#memory-pools)
    - [MPU Support](#mpu-support)
5. [Diagnostics & Safety](#diagnostics--safety)
    - [Deadlock Detection](#deadlock-detection)
    - [Health Monitoring](#health-monitoring)
    - [Profiler](#execution-profiler)

---

## Core System

Basic system control functions.

### `void pico_rtos_start(void)`
Starts the RTOS scheduler. This function does not return.
- **Note**: Ensure all initial tasks and hardware initialization are complete before calling this.

---

## Task Management

Manage threads of execution.

### Data Structures
- **`pico_rtos_task_t`**: Opaque handle to a task.

### Functions

#### `bool pico_rtos_task_create(pico_rtos_task_function_t task_func, void *param, uint32_t *stack, size_t stack_size, uint32_t priority, pico_rtos_task_t *task)`
Creates a new task.
- **Parameters**:
  - `task_func`: Function pointer for the task entry point.
  - `param`: Pointer to parameter passed to the task.
  - `stack`: Pointer to the stack memory array.
  - `stack_size`: Size of the stack in words (uint32_t).
  - `priority`: Task priority (0 is lowest).
  - `task`: Pointer to task structure to initialize.
- **Return**: `true` on success, `false` otherwise.

#### `void pico_rtos_task_sleep(uint32_t delay_ms)`
Suspends the calling task for a specified duration.
- **Parameters**:
  - `delay_ms`: Delay in milliseconds.

#### `void pico_rtos_task_yield(void)`
Voluntarily yields the processor to the next task of the same priority.

---

## Synchronization Primitives

### Mutexes
Mutual exclusion for shared resources. defined in `include/pico_rtos/mutex.h`.

#### `bool pico_rtos_mutex_init(pico_rtos_mutex_t *mutex)`
Initializes a mutex.
- **Return**: `true` on success.

#### `bool pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex, uint32_t timeout)`
Acquires a lock on the mutex.
- **Parameters**:
  - `timeout`: Max wait time in ms (`PICO_RTOS_WAIT_FOREVER` for infinite).
- **Return**: `true` if acquired, `false` on timeout.

#### `bool pico_rtos_mutex_try_lock(pico_rtos_mutex_t *mutex)`
Attempts to lock without blocking.

#### `bool pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex)`
Releases the mutex. Must be called by the owner.

#### `void pico_rtos_mutex_delete(pico_rtos_mutex_t *mutex)`
Deletes the mutex.

### Semaphores
Counting or binary semaphores. defined in `include/pico_rtos/semaphore.h`.

#### `bool pico_rtos_semaphore_init(pico_rtos_semaphore_t *sem, uint32_t initial, uint32_t max)`
Initializes a semaphore.
- **Parameters**:
  - `initial`: Initial count.
  - `max`: Maximum count.

#### `bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *sem)`
Increments the semaphore count.

#### `bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *sem, uint32_t timeout)`
Decrements the semaphore count, blocking if count is 0.

#### `void pico_rtos_semaphore_delete(pico_rtos_semaphore_t *sem)`
Deletes the semaphore.

### Queues
Thread-safe message passing. defined in `include/pico_rtos/queue.h`.

#### `bool pico_rtos_queue_init(pico_rtos_queue_t *queue, void *buffer, size_t item_size, size_t max_items)`
Initializes a queue.
- **Parameters**:
  - `buffer`: Storage array for items. (size must be >= `item_size` * `max_items`).
  - `item_size`: Size of each item in bytes.
  - `max_items`: Capacity.

#### `bool pico_rtos_queue_send(pico_rtos_queue_t *queue, const void *item, uint32_t timeout)`
Sends an item to the back of the queue.

#### `bool pico_rtos_queue_receive(pico_rtos_queue_t *queue, void *item, uint32_t timeout)`
Receives an item from the front of the queue.

### Event Groups
Manage multiple events (bits) with "wait for any/all" semantics. defined in `include/pico_rtos/event_group.h`.

#### Constants
- `PICO_RTOS_EVENT_GROUP_MAX_EVENTS`: 32
- `PICO_RTOS_EVENT_BIT_0`...`PICO_RTOS_EVENT_BIT_31`: Bit masks.

#### `bool pico_rtos_event_group_init(pico_rtos_event_group_t *eg)`
Initializes an event group.

#### `bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *eg, uint32_t bits)`
Sets specific bits. Unblocks waiting tasks if conditions are met. (ISR safe).

#### `bool pico_rtos_event_group_clear_bits(pico_rtos_event_group_t *eg, uint32_t bits)`
Clears specific bits.

#### `uint32_t pico_rtos_event_group_wait_bits(pico_rtos_event_group_t *eg, uint32_t bits_to_wait, bool wait_all, bool clear_exit, uint32_t timeout)`
Waits for event bits.
- **Parameters**:
  - `wait_all`: if true, waits for ALL bits. If false, waits for ANY bit.
  - `clear_exit`: if true, clears the bits upon successful return.
- **Return**: The value of the event bits when the condition was met.

#### `uint32_t pico_rtos_event_group_get_bits(pico_rtos_event_group_t *eg)`
Returns current bits instantly.

### Stream Buffers
Variable-length byte streams, optimized for single-reader/single-writer. defined in `include/pico_rtos/stream_buffer.h`.

#### `bool pico_rtos_stream_buffer_init(pico_rtos_stream_buffer_t *sb, uint8_t *buffer, uint32_t size)`
Initializes a stream buffer using the provided storage.

#### `uint32_t pico_rtos_stream_buffer_send(pico_rtos_stream_buffer_t *sb, const void *data, uint32_t len, uint32_t timeout)`
Writes bytes to the stream. Thread-safe.

#### `uint32_t pico_rtos_stream_buffer_receive(pico_rtos_stream_buffer_t *sb, void *data, uint32_t max_len, uint32_t timeout)`
Reads bytes from the stream.

#### `bool pico_rtos_stream_buffer_set_zero_copy(pico_rtos_stream_buffer_t *sb, bool enable)`
Enables zero-copy mode for large transfers.

#### `bool pico_rtos_stream_buffer_zero_copy_send_start(pico_rtos_stream_buffer_t *sb, uint32_t len, pico_rtos_stream_buffer_zero_copy_t *zc, uint32_t timeout)`
Begins a zero-copy write. Fills `zc` with direct buffer pointer. Must be followed by `complete`.

#### `bool pico_rtos_stream_buffer_zero_copy_receive_start(...)`
Begins a zero-copy read.

---

## Memory Management

### Memory Pools
O(1) fixed-size block allocator. defined in `include/pico_rtos/memory_pool.h`.

#### `bool pico_rtos_memory_pool_init(pico_rtos_memory_pool_t *pool, void *mem, uint32_t block_size, uint32_t block_count)`
Initializes a pool.

#### `void *pico_rtos_memory_pool_alloc(pico_rtos_memory_pool_t *pool, uint32_t timeout)`
Allocates a block. Returns NULL on timeout.

#### `bool pico_rtos_memory_pool_free(pico_rtos_memory_pool_t *pool, void *block)`
Frees a block. Returns false if block is invalid (not from this pool) or corruption detected.

#### `bool pico_rtos_memory_pool_validate(pico_rtos_memory_pool_t *pool)`
Debug: checks integrity of the entire pool.

### MPU Support
Hardware memory protection (RP2040 limitations apply). defined in `include/pico_rtos/mpu.h`.

#### `bool pico_rtos_mpu_init(void)`
Initializes MPU hardware.

#### `bool pico_rtos_mpu_configure_region(uint8_t region_num, const pico_rtos_mpu_region_t *config)`
Detailed configuration of an MPU region.

#### `bool pico_rtos_mpu_configure_simple_region(...)`
Helper for common R/W/X scenarios.

#### `bool pico_rtos_mpu_configure_stack_protection(uint8_t region_num, uint32_t base, uint32_t size, uint32_t guard_size)`
Protects the bottom of a stack to detect overflows.

---

## Diagnostics & Safety

### Deadlock Detection
Runtime analysis of resource dependencies. defined in `include/pico_rtos/deadlock.h`.

#### `bool pico_rtos_deadlock_init(void)`
Starts the deadlock detector.

#### `uint32_t pico_rtos_deadlock_register_resource(void *ptr, pico_rtos_resource_type_t type, const char *name)`
Registers a resource (mutex, etc.) for tracking.

#### `bool pico_rtos_deadlock_detect(pico_rtos_deadlock_result_t *result)`
Manually triggers a detection cycle. (Usually runs automatically periodically).

#### `bool pico_rtos_deadlock_resolve(const pico_rtos_deadlock_result_t *res, pico_rtos_deadlock_action_t action)`
Attempts recovery (e.g., abort task).

### Health Monitoring
System-wide metrics telemetry. defined in `include/pico_rtos/health.h`.

#### `bool pico_rtos_health_init(void)`
Starts health monitor service.

#### `bool pico_rtos_health_get_system_health(pico_rtos_system_health_t *health)`
Gets global stats (uptime, task counts, error rates).

#### `bool pico_rtos_health_get_cpu_stats(uint32_t core_id, pico_rtos_cpu_stats_t *stats)`
Per-core usage stats (idle time, active time).

#### `bool pico_rtos_health_get_memory_stats(pico_rtos_memory_stats_t *stats)`
Heap usage statistics.

#### `uint32_t pico_rtos_health_register_metric(...)`
Adds a custom metric to the health system.

### Execution Profiler
High-resolution function timing. defined in `include/pico_rtos/profiler.h`.

#### Macros
- `PICO_RTOS_PROFILE_FUNCTION()`: Add this at the top of a function to profile it automatically.
- `PICO_RTOS_PROFILE_BLOCK_START(name)` / `_END()`: Profile a section of code.

#### Functions
- **`pico_rtos_profiler_init(max_entries)`**: Setup.
- **`pico_rtos_profiler_get_stats(...)`**: Get global overhead/stats.
- **`pico_rtos_profiler_print_all_entries()`**: Dumps timing table to stdout.

---

## Multi-Core (SMP)
Symmetric multi-processing support. Defined in `include/pico_rtos/smp.h`.

### Initialization

#### `bool pico_rtos_smp_init(void)`
Initializes the SMP scheduler for dual-core operation.
- **Return**: `true` on success.
- **Note**: Call after `pico_rtos_init()` and before `pico_rtos_start()`.

#### `bool pico_rtos_smp_set_load_balancing(bool enable)`
Enables or disables automatic load balancing between cores.

#### `bool pico_rtos_smp_set_load_balance_threshold(uint32_t percent)`
Sets the CPU usage difference threshold that triggers task migration.

### Core Affinity

#### `bool pico_rtos_task_set_core_affinity(pico_rtos_task_t *task, uint8_t affinity_mask)`
Sets which core(s) a task can run on.
- **Parameters**:
  - `affinity_mask`: `0x01` = Core 0, `0x02` = Core 1, `0x03` = Any.

#### `uint8_t pico_rtos_task_get_core_affinity(pico_rtos_task_t *task)`
Returns the current affinity mask for a task.

#### `uint32_t pico_rtos_get_current_core(void)`
Returns the ID of the currently executing core (0 or 1).

### Inter-Core Communication

#### `bool pico_rtos_ipc_send_message(uint32_t target_core, uint32_t type, const void *data, uint32_t size, uint32_t timeout)`
Sends a message to a task on another core.

#### `bool pico_rtos_ipc_receive_message(pico_rtos_ipc_message_t *msg, uint32_t timeout)`
Receives a pending IPC message.

#### `uint32_t pico_rtos_ipc_get_pending_count(void)`
Returns the number of pending IPC messages.

### Statistics

#### `bool pico_rtos_smp_get_stats(pico_rtos_smp_stats_t *stats)`
Gets SMP statistics including per-core usage and migration counts.

---

## High-Resolution Timers
Microsecond-precision timing. Defined in `include/pico_rtos/hires_timer.h`.

#### `bool pico_rtos_hires_timer_init(pico_rtos_hires_timer_t *timer)`
Initializes a high-resolution timer.

#### `void pico_rtos_hires_timer_start(pico_rtos_hires_timer_t *timer)`
Starts the timer.

#### `uint64_t pico_rtos_hires_timer_get_us(pico_rtos_hires_timer_t *timer)`
Returns elapsed microseconds since timer start.

#### `void pico_rtos_hires_timer_reset(pico_rtos_hires_timer_t *timer)`
Resets the timer to zero.

---

## System Tracing
Event logging for debugging. Defined in `include/pico_rtos/trace.h`.

#### `bool pico_rtos_trace_init(uint32_t buffer_size)`
Initializes trace buffer with specified size.

#### `void pico_rtos_trace_start(void)`
Begins event recording.

#### `void pico_rtos_trace_stop(void)`
Stops event recording.

#### `bool pico_rtos_trace_get_events(pico_rtos_trace_entry_t *buffer, uint32_t *count)`
Retrieves recorded trace events.

#### `void pico_rtos_trace_clear(void)`
Clears the trace buffer.

---

## Watchdog Integration
Hardware watchdog support. Defined in `include/pico_rtos/watchdog.h`.

#### `bool pico_rtos_watchdog_init(uint32_t timeout_ms)`
Initializes the hardware watchdog with specified timeout.

#### `void pico_rtos_watchdog_feed(void)`
Resets the watchdog timer (call periodically to prevent reset).

#### `void pico_rtos_watchdog_enable(void)`
Enables the watchdog.

#### `void pico_rtos_watchdog_disable(void)`
Disables the watchdog.

#### `bool pico_rtos_watchdog_caused_reboot(void)`
Returns `true` if the last reset was caused by watchdog timeout.

---

## Alert System
Configurable threshold notifications. Defined in `include/pico_rtos/alerts.h`.

#### `bool pico_rtos_alert_init(void)`
Initializes the alert system.

#### `uint32_t pico_rtos_alert_register(pico_rtos_alert_type_t type, uint32_t threshold, pico_rtos_alert_callback_t callback)`
Registers an alert with threshold and callback.

#### `bool pico_rtos_alert_unregister(uint32_t alert_id)`
Removes a registered alert.

#### `bool pico_rtos_alert_set_threshold(uint32_t alert_id, uint32_t threshold)`
Updates an alert's threshold value.

---

## I/O Abstraction
Unified peripheral interface. Defined in `include/pico_rtos/io.h`.

#### `bool pico_rtos_io_init(pico_rtos_io_t *io, const pico_rtos_io_config_t *config)`
Initializes an I/O channel with the specified configuration.

#### `int32_t pico_rtos_io_read(pico_rtos_io_t *io, void *buffer, uint32_t size, uint32_t timeout)`
Reads data from I/O channel. Returns bytes read or negative error code.

#### `int32_t pico_rtos_io_write(pico_rtos_io_t *io, const void *data, uint32_t size, uint32_t timeout)`
Writes data to I/O channel. Returns bytes written or negative error code.

#### `bool pico_rtos_io_close(pico_rtos_io_t *io)`
Closes an I/O channel and releases resources.

---

## Universal Timeouts
Consistent timeout handling. Defined in `include/pico_rtos/timeout.h`.

### Constants
| Constant | Value | Description |
|----------|-------|-------------|
| `PICO_RTOS_NO_WAIT` | `0` | Return immediately |
| `PICO_RTOS_WAIT_FOREVER` | `0xFFFFFFFF` | Block indefinitely |

#### `bool pico_rtos_timeout_init(pico_rtos_timeout_t *timeout, uint32_t ms)`
Initializes a timeout tracker.

#### `bool pico_rtos_timeout_expired(pico_rtos_timeout_t *timeout)`
Checks if timeout has elapsed.

#### `uint32_t pico_rtos_timeout_remaining(pico_rtos_timeout_t *timeout)`
Returns remaining milliseconds.

---

## See Also

- [User Guide](user_guide.md) — Conceptual overview and patterns
- [Multi-Core Guide](multicore.md) — SMP programming details
- [Error Codes](error_codes.md) — Complete error reference

---

**API Reference Version**: v0.3.1  
**Last Updated**: December 2025