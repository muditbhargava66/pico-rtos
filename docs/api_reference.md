# Pico-RTOS API Reference

This document provides a comprehensive reference for the Pico-RTOS API.

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

## Constants

- `PICO_RTOS_WAIT_FOREVER`: Wait indefinitely for a resource.
- `PICO_RTOS_NO_WAIT`: Do not wait for a resource.

## Types

- `pico_rtos_task_function_t`: Type for task functions.
- `pico_rtos_timer_callback_t`: Type for timer callback functions.

---