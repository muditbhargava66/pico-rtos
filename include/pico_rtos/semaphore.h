#ifndef PICO_RTOS_SEMAPHORE_H
#define PICO_RTOS_SEMAPHORE_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/critical_section.h"
#include "pico_rtos/task.h"

// Forward declare the blocking object structure
struct pico_rtos_block_object;

typedef struct {
    uint32_t count;
    uint32_t max_count;
    critical_section_t cs;
    struct pico_rtos_block_object *block_obj;
} pico_rtos_semaphore_t;

/**
 * @brief Initialize a semaphore
 * 
 * @param semaphore Pointer to semaphore structure
 * @param initial_count Initial count value (0 for binary semaphore)
 * @param max_count Maximum count value (1 for binary semaphore)
 * @return true if initialized successfully, false otherwise
 */
bool pico_rtos_semaphore_init(pico_rtos_semaphore_t *semaphore, uint32_t initial_count, uint32_t max_count);

/**
 * @brief Give (increment) a semaphore
 * 
 * @param semaphore Pointer to semaphore structure
 * @return true if semaphore was given successfully, false if at max_count
 */
bool pico_rtos_semaphore_give(pico_rtos_semaphore_t *semaphore);

/**
 * @brief Take (decrement) a semaphore
 * 
 * @param semaphore Pointer to semaphore structure
 * @param timeout Timeout in milliseconds, PICO_RTOS_WAIT_FOREVER to wait 
 *                forever, or PICO_RTOS_NO_WAIT for immediate return
 * @return true if semaphore was taken successfully, false if timeout occurred
 */
bool pico_rtos_semaphore_take(pico_rtos_semaphore_t *semaphore, uint32_t timeout);

/**
 * @brief Check if a semaphore can be taken without blocking
 * 
 * @param semaphore Pointer to semaphore structure
 * @return true if semaphore can be taken without blocking, false otherwise
 */
bool pico_rtos_semaphore_is_available(pico_rtos_semaphore_t *semaphore);

/**
 * @brief Delete a semaphore and free associated resources
 * 
 * @param semaphore Pointer to semaphore structure
 */
void pico_rtos_semaphore_delete(pico_rtos_semaphore_t *semaphore);

#endif // PICO_RTOS_SEMAPHORE_H