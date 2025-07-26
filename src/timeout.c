/**
 * @file timeout.c
 * @brief Universal Timeout Support Implementation
 * 
 * This module implements comprehensive timeout management for all blocking
 * operations in Pico-RTOS, ensuring consistent timeout behavior across
 * all synchronization primitives and system calls.
 */

#include "pico_rtos/timeout.h"
#include "pico_rtos/task.h"
#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include "hardware/timer.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// INTERNAL DATA STRUCTURES
// =============================================================================

/**
 * @brief Timeout system state
 */
typedef struct {
    bool initialized;
    pico_rtos_timeout_t *active_timeouts;       // Sorted list of active timeouts
    pico_rtos_timeout_t *free_timeouts;         // List of free timeout structures
    pico_rtos_timeout_t timeout_pool[PICO_RTOS_TIMEOUT_MAX_ACTIVE];
    uint32_t next_timeout_id;
    uint32_t active_timeout_count;
    pico_rtos_timeout_config_t config;
    pico_rtos_timeout_statistics_t stats;
    critical_section_t cs;
    uint32_t last_cleanup_time;
} pico_rtos_timeout_system_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

static pico_rtos_timeout_system_t g_timeout_system = {0};

// =============================================================================
// STRING CONSTANTS
// =============================================================================

static const char *timeout_result_strings[] = {
    "Success",
    "Expired",
    "Cancelled",
    "Invalid",
    "Resource Error"
};

static const char *timeout_type_strings[] = {
    "Blocking",
    "Periodic",
    "Absolute",
    "Relative",
    "Deadline"
};

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current time in microseconds
 * @return Current time in microseconds
 */
static inline uint64_t get_current_time_us(void)
{
    return time_us_64();
}

/**
 * @brief Allocate a timeout structure from the pool
 * @return Pointer to allocated timeout, or NULL if none available
 */
static pico_rtos_timeout_t *allocate_timeout(void)
{
    if (g_timeout_system.free_timeouts == NULL) {
        g_timeout_system.stats.resource_allocation_failures++;
        return NULL;
    }
    
    pico_rtos_timeout_t *timeout = g_timeout_system.free_timeouts;
    g_timeout_system.free_timeouts = timeout->next;
    
    // Clear the timeout structure
    memset(timeout, 0, sizeof(pico_rtos_timeout_t));
    
    return timeout;
}

/**
 * @brief Return timeout structure to the free pool
 * @param timeout Timeout to deallocate
 */
static void deallocate_timeout(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL) return;
    
    timeout->next = g_timeout_system.free_timeouts;
    g_timeout_system.free_timeouts = timeout;
}

/**
 * @brief Insert timeout into sorted active list
 * @param timeout Timeout to insert
 */
static void insert_timeout_sorted(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL) return;
    
    // Find insertion point (sorted by expiry_time_us)
    pico_rtos_timeout_t **current = &g_timeout_system.active_timeouts;
    
    while (*current != NULL && (*current)->expiry_time_us <= timeout->expiry_time_us) {
        current = &(*current)->next;
    }
    
    // Insert timeout
    timeout->next = *current;
    if (*current != NULL) {
        (*current)->prev = timeout;
    }
    timeout->prev = (current == &g_timeout_system.active_timeouts) ? NULL : 
                   (pico_rtos_timeout_t *)((char *)current - offsetof(pico_rtos_timeout_t, next));
    *current = timeout;
    
    g_timeout_system.active_timeout_count++;
    g_timeout_system.stats.active_timeouts = g_timeout_system.active_timeout_count;
}

/**
 * @brief Remove timeout from active list
 * @param timeout Timeout to remove
 */
static void remove_timeout_from_list(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL) return;
    
    if (timeout->prev != NULL) {
        timeout->prev->next = timeout->next;
    } else {
        g_timeout_system.active_timeouts = timeout->next;
    }
    
    if (timeout->next != NULL) {
        timeout->next->prev = timeout->prev;
    }
    
    timeout->next = NULL;
    timeout->prev = NULL;
    
    if (g_timeout_system.active_timeout_count > 0) {
        g_timeout_system.active_timeout_count--;
        g_timeout_system.stats.active_timeouts = g_timeout_system.active_timeout_count;
    }
}

/**
 * @brief Update timeout statistics
 * @param timeout Timeout to update statistics for
 * @param result Result of the timeout operation
 */
static void update_timeout_statistics(pico_rtos_timeout_t *timeout, pico_rtos_timeout_result_t result)
{
    if (timeout == NULL) return;
    
    uint64_t current_time = get_current_time_us();
    uint64_t wait_time = current_time - timeout->start_time_us;
    
    // Update timeout-specific statistics
    timeout->total_wait_time_us += wait_time;
    if (wait_time > timeout->max_wait_time_us) {
        timeout->max_wait_time_us = wait_time;
    }
    
    // Update system statistics
    g_timeout_system.stats.total_wait_time_us += wait_time;
    if (wait_time > g_timeout_system.stats.max_wait_time_us) {
        g_timeout_system.stats.max_wait_time_us = wait_time;
    }
    
    switch (result) {
        case PICO_RTOS_TIMEOUT_SUCCESS:
            g_timeout_system.stats.total_timeouts_completed++;
            break;
        case PICO_RTOS_TIMEOUT_EXPIRED:
            g_timeout_system.stats.total_timeouts_expired++;
            break;
        case PICO_RTOS_TIMEOUT_CANCELLED:
            g_timeout_system.stats.total_timeouts_cancelled++;
            break;
        default:
            break;
    }
    
    // Calculate average wait time
    uint32_t total_operations = g_timeout_system.stats.total_timeouts_completed +
                               g_timeout_system.stats.total_timeouts_expired +
                               g_timeout_system.stats.total_timeouts_cancelled;
    
    if (total_operations > 0) {
        g_timeout_system.stats.average_wait_time_us = 
            g_timeout_system.stats.total_wait_time_us / total_operations;
    }
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_timeout_init(void)
{
    if (g_timeout_system.initialized) {
        return true;
    }
    
    // Initialize critical section
    critical_section_init(&g_timeout_system.cs);
    
    // Initialize timeout pool
    for (int i = 0; i < PICO_RTOS_TIMEOUT_MAX_ACTIVE - 1; i++) {
        g_timeout_system.timeout_pool[i].next = &g_timeout_system.timeout_pool[i + 1];
    }
    g_timeout_system.timeout_pool[PICO_RTOS_TIMEOUT_MAX_ACTIVE - 1].next = NULL;
    g_timeout_system.free_timeouts = &g_timeout_system.timeout_pool[0];
    
    // Initialize system state
    g_timeout_system.active_timeouts = NULL;
    g_timeout_system.next_timeout_id = 1;
    g_timeout_system.active_timeout_count = 0;
    g_timeout_system.last_cleanup_time = pico_rtos_get_tick_count();
    
    // Initialize configuration with defaults
    g_timeout_system.config.resolution_us = PICO_RTOS_TIMEOUT_RESOLUTION_US;
    g_timeout_system.config.max_active_timeouts = PICO_RTOS_TIMEOUT_MAX_ACTIVE;
    g_timeout_system.config.high_precision_mode = true;
    g_timeout_system.config.statistics_enabled = PICO_RTOS_TIMEOUT_ENABLE_STATISTICS;
    g_timeout_system.config.cleanup_interval_ms = 1000;
    g_timeout_system.config.accuracy_threshold_us = 1000;
    
    // Initialize statistics
    memset(&g_timeout_system.stats, 0, sizeof(g_timeout_system.stats));
    
    g_timeout_system.initialized = true;
    
    PICO_RTOS_LOG_INFO("Universal timeout system initialized");
    return true;
}

bool pico_rtos_timeout_create(pico_rtos_timeout_t *timeout,
                             pico_rtos_timeout_type_t type,
                             uint32_t timeout_ms,
                             pico_rtos_timeout_callback_t callback,
                             pico_rtos_timeout_cleanup_t cleanup,
                             void *user_data)
{
    if (!g_timeout_system.initialized) {
        PICO_RTOS_LOG_ERROR("Timeout system not initialized");
        return false;
    }
    
    if (timeout == NULL) {
        PICO_RTOS_LOG_ERROR("Invalid timeout pointer");
        return false;
    }
    
    if (timeout_ms > PICO_RTOS_TIMEOUT_MAX_MS) {
        PICO_RTOS_LOG_ERROR("Timeout value too large: %u ms", timeout_ms);
        return false;
    }
    
    critical_section_enter_blocking(&g_timeout_system.cs);
    
    // Initialize timeout structure
    memset(timeout, 0, sizeof(pico_rtos_timeout_t));
    
    timeout->timeout_id = g_timeout_system.next_timeout_id++;
    timeout->type = type;
    timeout->timeout_ms = timeout_ms;
    timeout->callback = callback;
    timeout->cleanup = cleanup;
    timeout->user_data = user_data;
    timeout->result = PICO_RTOS_TIMEOUT_SUCCESS;
    timeout->active = false;
    timeout->cancelled = false;
    timeout->expired = false;
    timeout->creation_time = pico_rtos_get_tick_count();
    timeout->activation_count = 0;
    
    // Initialize critical section
    critical_section_init(&timeout->cs);
    
    g_timeout_system.stats.total_timeouts_created++;
    
    critical_section_exit(&g_timeout_system.cs);
    
    PICO_RTOS_LOG_DEBUG("Created timeout %u (type: %s, duration: %u ms)",
                        timeout->timeout_id, 
                        pico_rtos_timeout_get_type_string(type),
                        timeout_ms);
    return true;
}

bool pico_rtos_timeout_start(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL || !g_timeout_system.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_timeout_system.cs);
    critical_section_enter_blocking(&timeout->cs);
    
    if (timeout->active) {
        critical_section_exit(&timeout->cs);
        critical_section_exit(&g_timeout_system.cs);
        return true; // Already active
    }
    
    // Set timing information
    uint64_t current_time = get_current_time_us();
    timeout->start_time_us = current_time;
    
    if (timeout->type == PICO_RTOS_TIMEOUT_TYPE_ABSOLUTE) {
        timeout->expiry_time_us = timeout->deadline_us;
    } else {
        timeout->expiry_time_us = current_time + pico_rtos_timeout_ms_to_us(timeout->timeout_ms);
    }
    
    timeout->active = true;
    timeout->cancelled = false;
    timeout->expired = false;
    timeout->activation_count++;
    timeout->waiting_task = pico_rtos_get_current_task();
    
    // Insert into active list
    insert_timeout_sorted(timeout);
    
    critical_section_exit(&timeout->cs);
    critical_section_exit(&g_timeout_system.cs);
    
    PICO_RTOS_LOG_DEBUG("Started timeout %u", timeout->timeout_id);
    return true;
}

bool pico_rtos_timeout_cancel(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL || !g_timeout_system.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_timeout_system.cs);
    critical_section_enter_blocking(&timeout->cs);
    
    if (!timeout->active || timeout->cancelled || timeout->expired) {
        critical_section_exit(&timeout->cs);
        critical_section_exit(&g_timeout_system.cs);
        return true; // Already inactive
    }
    
    // Remove from active list
    remove_timeout_from_list(timeout);
    
    // Mark as cancelled
    timeout->cancelled = true;
    timeout->active = false;
    timeout->result = PICO_RTOS_TIMEOUT_CANCELLED;
    
    // Call cleanup function if provided
    if (timeout->cleanup != NULL) {
        timeout->cleanup(timeout, timeout->user_data);
    }
    
    // Update statistics
    update_timeout_statistics(timeout, PICO_RTOS_TIMEOUT_CANCELLED);
    
    critical_section_exit(&timeout->cs);
    critical_section_exit(&g_timeout_system.cs);
    
    PICO_RTOS_LOG_DEBUG("Cancelled timeout %u", timeout->timeout_id);
    return true;
}

bool pico_rtos_timeout_reset(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL || !g_timeout_system.initialized) {
        return false;
    }
    
    bool was_active = timeout->active;
    
    if (was_active) {
        pico_rtos_timeout_cancel(timeout);
    }
    
    critical_section_enter_blocking(&timeout->cs);
    
    // Reset state
    timeout->result = PICO_RTOS_TIMEOUT_SUCCESS;
    timeout->cancelled = false;
    timeout->expired = false;
    timeout->start_time_us = 0;
    timeout->expiry_time_us = 0;
    
    critical_section_exit(&timeout->cs);
    
    if (was_active) {
        return pico_rtos_timeout_start(timeout);
    }
    
    return true;
}

bool pico_rtos_timeout_is_expired(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&timeout->cs);
    bool expired = timeout->expired;
    critical_section_exit(&timeout->cs);
    
    return expired;
}

bool pico_rtos_timeout_is_active(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&timeout->cs);
    bool active = timeout->active;
    critical_section_exit(&timeout->cs);
    
    return active;
}

uint32_t pico_rtos_timeout_get_remaining_ms(pico_rtos_timeout_t *timeout)
{
    return pico_rtos_timeout_us_to_ms(pico_rtos_timeout_get_remaining_us(timeout));
}

uint64_t pico_rtos_timeout_get_remaining_us(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL || !timeout->active) {
        return 0;
    }
    
    uint64_t current_time = get_current_time_us();
    
    critical_section_enter_blocking(&timeout->cs);
    
    if (timeout->expired || timeout->cancelled) {
        critical_section_exit(&timeout->cs);
        return 0;
    }
    
    if (current_time >= timeout->expiry_time_us) {
        critical_section_exit(&timeout->cs);
        return 0;
    }
    
    uint64_t remaining = timeout->expiry_time_us - current_time;
    critical_section_exit(&timeout->cs);
    
    return remaining;
}

uint32_t pico_rtos_timeout_get_elapsed_ms(pico_rtos_timeout_t *timeout)
{
    return pico_rtos_timeout_us_to_ms(pico_rtos_timeout_get_elapsed_us(timeout));
}

uint64_t pico_rtos_timeout_get_elapsed_us(pico_rtos_timeout_t *timeout)
{
    if (timeout == NULL || timeout->start_time_us == 0) {
        return 0;
    }
    
    uint64_t current_time = get_current_time_us();
    
    critical_section_enter_blocking(&timeout->cs);
    uint64_t elapsed = current_time - timeout->start_time_us;
    critical_section_exit(&timeout->cs);
    
    return elapsed;
}

// Blocking operation helpers
pico_rtos_timeout_result_t pico_rtos_timeout_wait_for_condition(
    bool (*condition_func)(void *data),
    void *condition_data,
    uint32_t timeout_ms)
{
    if (condition_func == NULL) {
        return PICO_RTOS_TIMEOUT_INVALID;
    }
    
    // Check condition immediately
    if (condition_func(condition_data)) {
        return PICO_RTOS_TIMEOUT_SUCCESS;
    }
    
    // Handle immediate timeout
    if (timeout_ms == PICO_RTOS_TIMEOUT_IMMEDIATE) {
        return PICO_RTOS_TIMEOUT_EXPIRED;
    }
    
    // Handle infinite timeout
    if (timeout_ms == PICO_RTOS_TIMEOUT_INFINITE) {
        while (!condition_func(condition_data)) {
            // Yield to other tasks
            pico_rtos_task_yield();
        }
        return PICO_RTOS_TIMEOUT_SUCCESS;
    }
    
    // Create and start timeout
    pico_rtos_timeout_t timeout;
    if (!pico_rtos_timeout_create(&timeout, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                 timeout_ms, NULL, NULL, NULL)) {
        return PICO_RTOS_TIMEOUT_RESOURCE_ERROR;
    }
    
    if (!pico_rtos_timeout_start(&timeout)) {
        return PICO_RTOS_TIMEOUT_RESOURCE_ERROR;
    }
    
    // Wait for condition or timeout
    while (!condition_func(condition_data) && !pico_rtos_timeout_is_expired(&timeout)) {
        pico_rtos_task_yield();
    }
    
    pico_rtos_timeout_result_t result = condition_func(condition_data) ? 
                                       PICO_RTOS_TIMEOUT_SUCCESS : 
                                       PICO_RTOS_TIMEOUT_EXPIRED;
    
    pico_rtos_timeout_cancel(&timeout);
    return result;
}

pico_rtos_timeout_result_t pico_rtos_timeout_wait_until(uint64_t target_time_us)
{
    uint64_t current_time = get_current_time_us();
    
    if (target_time_us <= current_time) {
        return PICO_RTOS_TIMEOUT_EXPIRED;
    }
    
    uint64_t wait_time_us = target_time_us - current_time;
    uint32_t wait_time_ms = pico_rtos_timeout_us_to_ms(wait_time_us);
    
    return pico_rtos_timeout_sleep(wait_time_ms);
}

pico_rtos_timeout_result_t pico_rtos_timeout_sleep(uint32_t duration_ms)
{
    return pico_rtos_timeout_sleep_us(pico_rtos_timeout_ms_to_us(duration_ms));
}

pico_rtos_timeout_result_t pico_rtos_timeout_sleep_us(uint64_t duration_us)
{
    if (duration_us == 0) {
        return PICO_RTOS_TIMEOUT_SUCCESS;
    }
    
    uint64_t start_time = get_current_time_us();
    uint64_t end_time = start_time + duration_us;
    
    while (get_current_time_us() < end_time) {
        pico_rtos_task_yield();
    }
    
    return PICO_RTOS_TIMEOUT_SUCCESS;
}

// Configuration and monitoring
void pico_rtos_timeout_get_config(pico_rtos_timeout_config_t *config)
{
    if (!g_timeout_system.initialized || config == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&g_timeout_system.cs);
    *config = g_timeout_system.config;
    critical_section_exit(&g_timeout_system.cs);
}

bool pico_rtos_timeout_set_config(const pico_rtos_timeout_config_t *config)
{
    if (!g_timeout_system.initialized || config == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&g_timeout_system.cs);
    g_timeout_system.config = *config;
    critical_section_exit(&g_timeout_system.cs);
    
    return true;
}

void pico_rtos_timeout_get_statistics(pico_rtos_timeout_statistics_t *stats)
{
    if (!g_timeout_system.initialized || stats == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&g_timeout_system.cs);
    *stats = g_timeout_system.stats;
    critical_section_exit(&g_timeout_system.cs);
}

void pico_rtos_timeout_reset_statistics(void)
{
    if (!g_timeout_system.initialized) {
        return;
    }
    
    critical_section_enter_blocking(&g_timeout_system.cs);
    memset(&g_timeout_system.stats, 0, sizeof(g_timeout_system.stats));
    g_timeout_system.stats.active_timeouts = g_timeout_system.active_timeout_count;
    critical_section_exit(&g_timeout_system.cs);
}

// Utility functions
uint64_t pico_rtos_timeout_get_time_us(void)
{
    return get_current_time_us();
}

uint32_t pico_rtos_timeout_get_time_ms(void)
{
    return pico_rtos_timeout_us_to_ms(get_current_time_us());
}

const char *pico_rtos_timeout_get_result_string(pico_rtos_timeout_result_t result)
{
    if (result < sizeof(timeout_result_strings) / sizeof(timeout_result_strings[0])) {
        return timeout_result_strings[result];
    }
    return "Unknown";
}

const char *pico_rtos_timeout_get_type_string(pico_rtos_timeout_type_t type)
{
    if (type < sizeof(timeout_type_strings) / sizeof(timeout_type_strings[0])) {
        return timeout_type_strings[type];
    }
    return "Unknown";
}

// Internal system functions
void pico_rtos_timeout_process_expirations(void)
{
    if (!g_timeout_system.initialized) {
        return;
    }
    
    uint64_t current_time = get_current_time_us();
    
    critical_section_enter_blocking(&g_timeout_system.cs);
    
    while (g_timeout_system.active_timeouts != NULL &&
           g_timeout_system.active_timeouts->expiry_time_us <= current_time) {
        
        pico_rtos_timeout_t *timeout = g_timeout_system.active_timeouts;
        
        // Remove from active list
        remove_timeout_from_list(timeout);
        
        critical_section_enter_blocking(&timeout->cs);
        
        // Mark as expired
        timeout->expired = true;
        timeout->active = false;
        timeout->result = PICO_RTOS_TIMEOUT_EXPIRED;
        
        // Call callback if provided
        if (timeout->callback != NULL) {
            timeout->callback(timeout, timeout->user_data);
        }
        
        // Update statistics
        update_timeout_statistics(timeout, PICO_RTOS_TIMEOUT_EXPIRED);
        
        critical_section_exit(&timeout->cs);
    }
    
    critical_section_exit(&g_timeout_system.cs);
}

void pico_rtos_timeout_cleanup_expired(void)
{
    if (!g_timeout_system.initialized) {
        return;
    }
    
    uint32_t current_time = pico_rtos_get_tick_count();
    
    // Only cleanup periodically
    if (current_time - g_timeout_system.last_cleanup_time < 
        g_timeout_system.config.cleanup_interval_ms) {
        return;
    }
    
    g_timeout_system.last_cleanup_time = current_time;
    
    // Process any pending expirations first
    pico_rtos_timeout_process_expirations();
    
    PICO_RTOS_LOG_DEBUG("Timeout cleanup completed");
}