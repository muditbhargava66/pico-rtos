#ifndef PICO_RTOS_QUEUE_H
#define PICO_RTOS_QUEUE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "pico/critical_section.h"
#include "pico_rtos/task.h"

// Forward declare the blocking object structure
struct pico_rtos_block_object;

typedef struct {
    void *buffer;
    size_t item_size;
    size_t max_items;
    size_t head;
    size_t tail;
    size_t count;
    critical_section_t cs;
    struct pico_rtos_block_object *send_block_obj;    // For tasks blocked trying to send
    struct pico_rtos_block_object *receive_block_obj; // For tasks blocked trying to receive
} pico_rtos_queue_t;

/**
 * @brief Initialize a queue
 * 
 * @param queue Pointer to queue structure
 * @param buffer Buffer to store queue items
 * @param item_size Size of each item in bytes
 * @param max_items Maximum number of items in the queue
 * @return true if initialized successfully, false otherwise
 */
bool pico_rtos_queue_init(pico_rtos_queue_t *queue, void *buffer, size_t item_size, size_t max_items);

/**
 * @brief Send an item to the queue
 * 
 * @param queue Pointer to queue structure
 * @param item Pointer to the item to send
 * @param timeout Timeout in milliseconds, PICO_RTOS_WAIT_FOREVER to wait
 *                forever, or PICO_RTOS_NO_WAIT for immediate return
 * @return true if item was sent successfully, false if timeout occurred
 */
bool pico_rtos_queue_send(pico_rtos_queue_t *queue, const void *item, uint32_t timeout);

/**
 * @brief Receive an item from the queue
 * 
 * @param queue Pointer to queue structure
 * @param item Pointer to store the received item
 * @param timeout Timeout in milliseconds, PICO_RTOS_WAIT_FOREVER to wait
 *                forever, or PICO_RTOS_NO_WAIT for immediate return
 * @return true if item was received successfully, false if timeout occurred
 */
bool pico_rtos_queue_receive(pico_rtos_queue_t *queue, void *item, uint32_t timeout);

/**
 * @brief Check if a queue is empty
 * 
 * @param queue Pointer to queue structure
 * @return true if queue is empty, false otherwise
 */
bool pico_rtos_queue_is_empty(pico_rtos_queue_t *queue);

/**
 * @brief Check if a queue is full
 * 
 * @param queue Pointer to queue structure
 * @return true if queue is full, false otherwise
 */
bool pico_rtos_queue_is_full(pico_rtos_queue_t *queue);

/**
 * @brief Delete a queue and free associated resources
 * 
 * @param queue Pointer to queue structure
 */
void pico_rtos_queue_delete(pico_rtos_queue_t *queue);

#endif // PICO_RTOS_QUEUE_H