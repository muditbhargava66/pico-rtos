#ifndef PICO_RTOS_MUTEX_H
#define PICO_RTOS_MUTEX_H

#include <stdbool.h>
#include "pico_rtos/task.h"
#include "pico/critical_section.h"

// Forward declare the blocking object structure
struct pico_rtos_block_object;

typedef struct {
    pico_rtos_task_t *owner;
    uint32_t lock_count;
    critical_section_t cs;
    struct pico_rtos_block_object *block_obj;
} pico_rtos_mutex_t;

/**
 * @brief Initialize a mutex
 * 
 * @param mutex Pointer to mutex structure
 * @return true if initialization was successful, false otherwise
 */
bool pico_rtos_mutex_init(pico_rtos_mutex_t *mutex);

/**
 * @brief Acquire a mutex lock
 * 
 * @param mutex Pointer to mutex structure
 * @param timeout Timeout in milliseconds, PICO_RTOS_WAIT_FOREVER to wait
 *                forever, or PICO_RTOS_NO_WAIT for immediate return
 * @return true if mutex was acquired successfully, false if timeout occurred
 */
bool pico_rtos_mutex_lock(pico_rtos_mutex_t *mutex, uint32_t timeout);

/**
 * @brief Try to acquire a mutex without blocking
 * 
 * @param mutex Pointer to mutex structure
 * @return true if mutex was acquired successfully, false otherwise
 */
bool pico_rtos_mutex_try_lock(pico_rtos_mutex_t *mutex);

/**
 * @brief Release a mutex lock
 * 
 * @param mutex Pointer to mutex structure
 * @return true if mutex was released successfully, false if the calling
 *         task is not the owner of the mutex
 */
bool pico_rtos_mutex_unlock(pico_rtos_mutex_t *mutex);

/**
 * @brief Get the current owner of the mutex
 * 
 * @param mutex Pointer to mutex structure
 * @return Pointer to the task that owns the mutex, or NULL if not owned
 */
pico_rtos_task_t *pico_rtos_mutex_get_owner(pico_rtos_mutex_t *mutex);

/**
 * @brief Delete a mutex and free associated resources
 * 
 * @param mutex Pointer to mutex structure
 */
void pico_rtos_mutex_delete(pico_rtos_mutex_t *mutex);

#endif // PICO_RTOS_MUTEX_H