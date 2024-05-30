# Pico-RTOS User Guide

This user guide provides an overview of the Pico-RTOS features and how to use them in your projects.

## Introduction

Pico-RTOS is a lightweight real-time operating system specifically designed for the Raspberry Pi Pico board. It provides a set of APIs and primitives for multitasking, inter-task communication, synchronization, and timing.

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