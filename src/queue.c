#include "pico_rtos/queue.h"
#include "pico/critical_section.h"

void pico_rtos_queue_init(pico_rtos_queue_t *queue, void *buffer, size_t item_size, size_t max_items) {
    queue->buffer = buffer;
    queue->item_size = item_size;
    queue->max_items = max_items;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

bool pico_rtos_queue_send(pico_rtos_queue_t *queue, const void *item, uint32_t timeout) {
    critical_section_enter_blocking(&queue->cs);

    if (queue->count == queue->max_items) {
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        pico_rtos_task_suspend(current_task);
        critical_section_exit(&queue->cs);
        pico_rtos_scheduler();
        return false;
    }

    memcpy((uint8_t *)queue->buffer + queue->tail * queue->item_size, item, queue->item_size);
    queue->tail = (queue->tail + 1) % queue->max_items;
    queue->count++;

    pico_rtos_task_resume_highest_priority();

    critical_section_exit(&queue->cs);
    return true;
}

bool pico_rtos_queue_receive(pico_rtos_queue_t *queue, void *item, uint32_t timeout) {
    critical_section_enter_blocking(&queue->cs);

    if (queue->count == 0) {
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        pico_rtos_task_suspend(current_task);
        critical_section_exit(&queue->cs);
        pico_rtos_scheduler();
        return false;
    }

    memcpy(item, (uint8_t *)queue->buffer + queue->head * queue->item_size, queue->item_size);
    queue->head = (queue->head + 1) % queue->max_items;
    queue->count--;

    pico_rtos_task_resume_highest_priority();

    critical_section_exit(&queue->cs);
    return true;
}