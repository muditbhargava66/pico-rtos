# Pico-RTOS API Reference

## Mutex

- `void pico_rtos_mutex_init(pico_rtos_mutex_t *mutex)`
  - Initialize a mutex.

- `void pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex)`
  - Acquire a mutex lock.

- `void pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex)`
  - Release a mutex lock.

## Queue

- `void pico_rtos_queue_init(pico_rtos_queue_t *queue, void *buffer, size_t item_size, size_t max_items)`
  - Initialize a queue.

- `bool pico_rtos_queue_send(pico_rtos_queue_t *queue, const void *item, uint32_t timeout)`
  - Send an item to the queue.

- `bool pico_rtos_queue_receive(pico_rtos_queue_t *queue, void *item, uint32_t timeout)`
  - Receive an item from the queue.

## Semaphore

- `void pico_rtos_semaphore_init(pico_rtos_semaphore_t *semaphore, uint32_t initial_count, uint32_t max_count)`
  - Initialize a semaphore.

- `bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *semaphore)`
  - Give (increment) a semaphore.

- `bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *semaphore, uint32_t timeout)`
  - Take (decrement) a semaphore.

## Task

- `void pico_rtos_task_create(pico_rtos_task_t *task, const char *name, pico_rtos_task_function_t function, void *param, uint32_t stack_size, uint32_t priority)`
  - Create a new task.

- `void pico_rtos_task_suspend(pico_rtos_task_t *task)`
  - Suspend a task.

- `void pico_rtos_task_resume(pico_rtos_task_t *task)`
  - Resume a suspended task.

- `pico_rtos_task_t *pico_rtos_get_current_task(void)`
  - Get the currently running task.

## Timer

- `void pico_rtos_timer_init(pico_rtos_timer_t *timer, const char *name, pico_rtos_timer_callback_t callback, void *param, uint32_t period, bool auto_reload)`
  - Initialize a timer.

- `void pico_rtos_timer_start(pico_rtos_timer_t *timer)`
  - Start a timer.

- `void pico_rtos_timer_stop(pico_rtos_timer_t *timer)`
  - Stop a timer.

- `void pico_rtos_timer_reset(pico_rtos_timer_t *timer)`
  - Reset a timer.

- `bool pico_rtos_timer_is_expired(pico_rtos_timer_t *timer)`
  - Check if a timer has expired.

---