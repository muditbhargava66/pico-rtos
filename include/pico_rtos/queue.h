#ifndef PICO_RTOS_QUEUE_H
#define PICO_RTOS_QUEUE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "pico/critical_section.h"

typedef struct {
    void *buffer;
    size_t item_size;
    size_t max_items;
    size_t head;
    size_t tail;
    size_t count;
    critical_section_t cs;
} pico_rtos_queue_t;

void pico_rtos_queue_init(pico_rtos_queue_t *queue, void *buffer, size_t item_size, size_t max_items);
bool pico_rtos_queue_send(pico_rtos_queue_t *queue, const void *item, uint32_t timeout);
bool pico_rtos_queue_receive(pico_rtos_queue_t *queue, void *item, uint32_t timeout);

#endif // PICO_RTOS_QUEUE_H