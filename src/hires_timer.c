/**
 * @file hires_timer.c
 * @brief High-Resolution Software Timers Implementation
 * 
 * This module implements microsecond-resolution software timers using the RP2040's
 * hardware timer capabilities with drift compensation and precise timing.
 */

#include "pico_rtos/hires_timer.h"
#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// INTERNAL DATA STRUCTURES
// =============================================================================

/**
 * @brief High-resolution timer subsystem state
 */
typedef struct {
    bool initialized;
    pico_rtos_hires_timer_t *active_timers;     // Sorted list of active timers
    pico_rtos_hires_timer_t *free_timers;       // List of free timer structures
    pico_rtos_hires_timer_t timer_pool[PICO_RTOS_HIRES_TIMER_MAX_TIMERS];
    uint32_t next_timer_id;
    uint32_t active_timer_count;
    uint32_t total_timers_created;
    uint32_t total_expirations;
    uint32_t total_missed_deadlines;
    uint64_t system_start_time_us;
    uint64_t timer_overhead_us;
    uint32_t max_timers_active;
    critical_section_t cs;
    
    // Hardware timer management
    uint32_t hw_timer_num;                      // Hardware timer number (0-3)
    bool hw_timer_active;
    uint64_t next_hw_expiry_us;
    
    // Calibration data
    bool calibrated;
    int64_t calibration_offset_us;
    double frequency_correction;
} pico_rtos_hires_timer_subsystem_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

static pico_rtos_hires_timer_subsystem_t g_hires_timer_subsystem = {0};

// =============================================================================
// ERROR DESCRIPTION STRINGS
// =============================================================================

static const char *hires_timer_error_strings[] = {
    "No error",
    "Invalid period",
    "Timer not found",
    "Timer running",
    "Timer not running",
    "Maximum timers exceeded",
    "Invalid callback",
    "Hardware fault",
    "Drift too large",
    "Invalid parameter"
};

static const char *timer_state_strings[] = {
    "Stopped",
    "Running",
    "Expired",
    "Error"
};

static const char *timer_mode_strings[] = {
    "One-shot",
    "Periodic"
};

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current time in microseconds using hardware timer
 * @return Current time in microseconds
 */
static inline uint64_t get_current_time_us(void)
{
    return time_us_64();
}

/**
 * @brief Apply calibration correction to time value
 * @param time_us Raw time value
 * @return Corrected time value
 */
static uint64_t apply_calibration(uint64_t time_us)
{
    if (!g_hires_timer_subsystem.calibrated) {
        return time_us;
    }
    
    // Apply frequency correction and offset
    double corrected = (double)time_us * g_hires_timer_subsystem.frequency_correction;
    return (uint64_t)corrected + g_hires_timer_subsystem.calibration_offset_us;
}

/**
 * @brief Find free timer structure from pool
 * @return Pointer to free timer, or NULL if none available
 */
static pico_rtos_hires_timer_t *allocate_timer(void)
{
    if (g_hires_timer_subsystem.free_timers == NULL) {
        return NULL;
    }
    
    pico_rtos_hires_timer_t *timer = g_hires_timer_subsystem.free_timers;
    g_hires_timer_subsystem.free_timers = timer->next;
    
    // Clear the timer structure
    memset(timer, 0, sizeof(pico_rtos_hires_timer_t));
    
    return timer;
}

/**
 * @brief Return timer structure to free pool
 * @param timer Timer to deallocate
 */
static void deallocate_timer(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL) return;
    
    timer->next = g_hires_timer_subsystem.free_timers;
    g_hires_timer_subsystem.free_timers = timer;
}

/**
 * @brief Insert timer into sorted active list
 * @param timer Timer to insert
 */
static void insert_timer_sorted(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL) return;
    
    // Find insertion point (sorted by next_expiry_us)
    pico_rtos_hires_timer_t **current = &g_hires_timer_subsystem.active_timers;
    
    while (*current != NULL && (*current)->next_expiry_us <= timer->next_expiry_us) {
        current = &(*current)->next;
    }
    
    // Insert timer
    timer->next = *current;
    if (*current != NULL) {
        (*current)->prev = timer;
    }
    timer->prev = (current == &g_hires_timer_subsystem.active_timers) ? NULL : 
                  (pico_rtos_hires_timer_t *)((char *)current - offsetof(pico_rtos_hires_timer_t, next));
    *current = timer;
    
    g_hires_timer_subsystem.active_timer_count++;
    
    if (g_hires_timer_subsystem.active_timer_count > g_hires_timer_subsystem.max_timers_active) {
        g_hires_timer_subsystem.max_timers_active = g_hires_timer_subsystem.active_timer_count;
    }
}

/**
 * @brief Remove timer from active list
 * @param timer Timer to remove
 */
static void remove_timer_from_list(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL) return;
    
    if (timer->prev != NULL) {
        timer->prev->next = timer->next;
    } else {
        g_hires_timer_subsystem.active_timers = timer->next;
    }
    
    if (timer->next != NULL) {
        timer->next->prev = timer->prev;
    }
    
    timer->next = NULL;
    timer->prev = NULL;
    
    if (g_hires_timer_subsystem.active_timer_count > 0) {
        g_hires_timer_subsystem.active_timer_count--;
    }
}

/**
 * @brief Update hardware timer for next expiration
 */
static void update_hardware_timer(void)
{
    if (g_hires_timer_subsystem.active_timers == NULL) {
        // No active timers, disable hardware timer
        if (g_hires_timer_subsystem.hw_timer_active) {
            hw_clear_bits(&timer_hw->inte, 1u << g_hires_timer_subsystem.hw_timer_num);
            g_hires_timer_subsystem.hw_timer_active = false;
        }
        return;
    }
    
    uint64_t next_expiry = g_hires_timer_subsystem.active_timers->next_expiry_us;
    uint64_t current_time = get_current_time_us();
    
    // Ensure we don't set timer in the past
    if (next_expiry <= current_time) {
        next_expiry = current_time + 1;
    }
    
    // Set hardware timer
    timer_hw->alarm[g_hires_timer_subsystem.hw_timer_num] = (uint32_t)next_expiry;
    hw_set_bits(&timer_hw->inte, 1u << g_hires_timer_subsystem.hw_timer_num);
    
    g_hires_timer_subsystem.hw_timer_active = true;
    g_hires_timer_subsystem.next_hw_expiry_us = next_expiry;
}

/**
 * @brief Calculate drift compensation for a timer
 * @param timer Timer to calculate drift for
 * @param actual_expiry_time Actual expiry time
 */
static void calculate_drift_compensation(pico_rtos_hires_timer_t *timer, uint64_t actual_expiry_time)
{
    if (timer->total_expirations == 0) {
        timer->last_actual_expiry_us = actual_expiry_time;
        return;
    }
    
    // Calculate drift (difference between expected and actual)
    int64_t expected_interval = timer->period_us;
    int64_t actual_interval = actual_expiry_time - timer->last_actual_expiry_us;
    int64_t drift = actual_interval - expected_interval;
    
    // Update drift statistics
    if (drift > timer->max_drift_us) {
        timer->max_drift_us = drift;
    }
    if (drift < timer->min_drift_us) {
        timer->min_drift_us = drift;
    }
    
    // Accumulate drift compensation
    if (PICO_RTOS_HIRES_TIMER_DRIFT_COMPENSATION) {
        timer->drift_compensation_us += drift / 4; // Gradual correction
        
        // Limit drift compensation to prevent instability
        if (timer->drift_compensation_us > (int64_t)timer->period_us / 4) {
            timer->drift_compensation_us = (int64_t)timer->period_us / 4;
        } else if (timer->drift_compensation_us < -(int64_t)timer->period_us / 4) {
            timer->drift_compensation_us = -(int64_t)timer->period_us / 4;
        }
    }
    
    timer->last_actual_expiry_us = actual_expiry_time;
}

/**
 * @brief Hardware timer interrupt handler
 */
static void hires_timer_irq_handler(void)
{
    uint32_t timer_num = g_hires_timer_subsystem.hw_timer_num;
    
    // Clear interrupt
    hw_clear_bits(&timer_hw->intr, 1u << timer_num);
    
    // Process timer expirations
    pico_rtos_hires_timer_process_expirations();
    
    // Update hardware timer for next expiration
    update_hardware_timer();
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_hires_timer_init(void)
{
    if (g_hires_timer_subsystem.initialized) {
        return true;
    }
    
    // Initialize critical section
    critical_section_init(&g_hires_timer_subsystem.cs);
    
    // Initialize timer pool
    for (int i = 0; i < PICO_RTOS_HIRES_TIMER_MAX_TIMERS - 1; i++) {
        g_hires_timer_subsystem.timer_pool[i].next = &g_hires_timer_subsystem.timer_pool[i + 1];
    }
    g_hires_timer_subsystem.timer_pool[PICO_RTOS_HIRES_TIMER_MAX_TIMERS - 1].next = NULL;
    g_hires_timer_subsystem.free_timers = &g_hires_timer_subsystem.timer_pool[0];
    
    // Initialize system state
    g_hires_timer_subsystem.active_timers = NULL;
    g_hires_timer_subsystem.next_timer_id = 1;
    g_hires_timer_subsystem.active_timer_count = 0;
    g_hires_timer_subsystem.total_timers_created = 0;
    g_hires_timer_subsystem.total_expirations = 0;
    g_hires_timer_subsystem.total_missed_deadlines = 0;
    g_hires_timer_subsystem.system_start_time_us = get_current_time_us();
    g_hires_timer_subsystem.max_timers_active = 0;
    
    // Initialize hardware timer (use timer 2 for high-res timers)
    g_hires_timer_subsystem.hw_timer_num = 2;
    g_hires_timer_subsystem.hw_timer_active = false;
    
    // Set up interrupt handler
    irq_set_exclusive_handler(TIMER_IRQ_2, hires_timer_irq_handler);
    irq_set_enabled(TIMER_IRQ_2, true);
    
    // Initialize calibration
    g_hires_timer_subsystem.calibrated = false;
    g_hires_timer_subsystem.calibration_offset_us = 0;
    g_hires_timer_subsystem.frequency_correction = 1.0;
    
    g_hires_timer_subsystem.initialized = true;
    
    PICO_RTOS_LOG_INFO("High-resolution timer subsystem initialized");
    return true;
}

bool pico_rtos_hires_timer_create(pico_rtos_hires_timer_t *timer,
                                 const char *name,
                                 pico_rtos_hires_timer_callback_t callback,
                                 void *param,
                                 uint64_t period_us,
                                 pico_rtos_hires_timer_mode_t mode)
{
    if (!g_hires_timer_subsystem.initialized) {
        PICO_RTOS_LOG_ERROR("High-resolution timer subsystem not initialized");
        return false;
    }
    
    if (timer == NULL || callback == NULL) {
        PICO_RTOS_LOG_ERROR("Invalid parameters for timer creation");
        return false;
    }
    
    if (period_us < PICO_RTOS_HIRES_TIMER_MIN_PERIOD_US || 
        period_us > PICO_RTOS_HIRES_TIMER_MAX_PERIOD_US) {
        PICO_RTOS_LOG_ERROR("Invalid timer period: %llu us", period_us);
        return false;
    }
    
    critical_section_enter_blocking(&g_hires_timer_subsystem.cs);
    
    // Initialize timer structure
    memset(timer, 0, sizeof(pico_rtos_hires_timer_t));
    
    timer->name = name;
    timer->timer_id = g_hires_timer_subsystem.next_timer_id++;
    timer->callback = callback;
    timer->param = param;
    timer->period_us = period_us;
    timer->mode = mode;
    timer->state = PICO_RTOS_HIRES_TIMER_STATE_STOPPED;
    timer->active = true;
    
    // Initialize drift compensation
    timer->drift_compensation_us = 0;
    timer->last_actual_expiry_us = 0;
    timer->total_expirations = 0;
    
    // Initialize statistics
    timer->created_time_us = get_current_time_us();
    timer->started_time_us = 0;
    timer->total_runtime_us = 0;
    timer->expiration_count = 0;
    timer->missed_deadlines = 0;
    timer->max_drift_us = INT64_MIN;
    timer->min_drift_us = INT64_MAX;
    timer->max_callback_duration_us = 0;
    
    // Initialize critical section
    critical_section_init(&timer->cs);
    
    g_hires_timer_subsystem.total_timers_created++;
    
    critical_section_exit(&g_hires_timer_subsystem.cs);
    
    PICO_RTOS_LOG_DEBUG("Created high-resolution timer %u ('%s') with period %llu us",
                        timer->timer_id, name ? name : "unnamed", period_us);
    return true;
}

bool pico_rtos_hires_timer_start(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return false;
    }
    
    critical_section_enter_blocking(&g_hires_timer_subsystem.cs);
    critical_section_enter_blocking(&timer->cs);
    
    if (timer->state == PICO_RTOS_HIRES_TIMER_STATE_RUNNING) {
        critical_section_exit(&timer->cs);
        critical_section_exit(&g_hires_timer_subsystem.cs);
        return true; // Already running
    }
    
    // Calculate next expiry time
    uint64_t current_time = apply_calibration(get_current_time_us());
    timer->next_expiry_us = current_time + timer->period_us - timer->drift_compensation_us;
    timer->started_time_us = current_time;
    timer->state = PICO_RTOS_HIRES_TIMER_STATE_RUNNING;
    
    // Insert into active timer list
    insert_timer_sorted(timer);
    
    // Update hardware timer
    update_hardware_timer();
    
    critical_section_exit(&timer->cs);
    critical_section_exit(&g_hires_timer_subsystem.cs);
    
    PICO_RTOS_LOG_DEBUG("Started high-resolution timer %u", timer->timer_id);
    return true;
}

bool pico_rtos_hires_timer_stop(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return false;
    }
    
    critical_section_enter_blocking(&g_hires_timer_subsystem.cs);
    critical_section_enter_blocking(&timer->cs);
    
    if (timer->state != PICO_RTOS_HIRES_TIMER_STATE_RUNNING) {
        critical_section_exit(&timer->cs);
        critical_section_exit(&g_hires_timer_subsystem.cs);
        return true; // Already stopped
    }
    
    // Remove from active list
    remove_timer_from_list(timer);
    
    // Update runtime statistics
    uint64_t current_time = get_current_time_us();
    timer->total_runtime_us += current_time - timer->started_time_us;
    
    timer->state = PICO_RTOS_HIRES_TIMER_STATE_STOPPED;
    
    // Update hardware timer
    update_hardware_timer();
    
    critical_section_exit(&timer->cs);
    critical_section_exit(&g_hires_timer_subsystem.cs);
    
    PICO_RTOS_LOG_DEBUG("Stopped high-resolution timer %u", timer->timer_id);
    return true;
}

bool pico_rtos_hires_timer_reset(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return false;
    }
    
    bool was_running = (timer->state == PICO_RTOS_HIRES_TIMER_STATE_RUNNING);
    
    if (was_running) {
        pico_rtos_hires_timer_stop(timer);
    }
    
    // Reset drift compensation
    timer->drift_compensation_us = 0;
    timer->last_actual_expiry_us = 0;
    
    if (was_running) {
        return pico_rtos_hires_timer_start(timer);
    }
    
    return true;
}

bool pico_rtos_hires_timer_change_period(pico_rtos_hires_timer_t *timer, uint64_t period_us)
{
    if (timer == NULL || !timer->active) {
        return false;
    }
    
    if (period_us < PICO_RTOS_HIRES_TIMER_MIN_PERIOD_US || 
        period_us > PICO_RTOS_HIRES_TIMER_MAX_PERIOD_US) {
        return false;
    }
    
    critical_section_enter_blocking(&timer->cs);
    
    bool was_running = (timer->state == PICO_RTOS_HIRES_TIMER_STATE_RUNNING);
    
    if (was_running) {
        pico_rtos_hires_timer_stop(timer);
    }
    
    timer->period_us = period_us;
    
    if (was_running) {
        pico_rtos_hires_timer_start(timer);
    }
    
    critical_section_exit(&timer->cs);
    return true;
}

bool pico_rtos_hires_timer_change_mode(pico_rtos_hires_timer_t *timer,
                                      pico_rtos_hires_timer_mode_t mode)
{
    if (timer == NULL || !timer->active) {
        return false;
    }
    
    critical_section_enter_blocking(&timer->cs);
    timer->mode = mode;
    critical_section_exit(&timer->cs);
    
    return true;
}

bool pico_rtos_hires_timer_delete(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return false;
    }
    
    // Stop timer if running
    pico_rtos_hires_timer_stop(timer);
    
    critical_section_enter_blocking(&timer->cs);
    timer->active = false;
    critical_section_exit(&timer->cs);
    
    PICO_RTOS_LOG_DEBUG("Deleted high-resolution timer %u", timer->timer_id);
    return true;
}

// Query functions
bool pico_rtos_hires_timer_is_running(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return false;
    }
    
    return (timer->state == PICO_RTOS_HIRES_TIMER_STATE_RUNNING);
}

bool pico_rtos_hires_timer_is_active(pico_rtos_hires_timer_t *timer)
{
    return (timer != NULL && timer->active);
}

pico_rtos_hires_timer_state_t pico_rtos_hires_timer_get_state(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return PICO_RTOS_HIRES_TIMER_STATE_ERROR;
    }
    
    return timer->state;
}

uint64_t pico_rtos_hires_timer_get_remaining_time_us(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active || timer->state != PICO_RTOS_HIRES_TIMER_STATE_RUNNING) {
        return 0;
    }
    
    uint64_t current_time = apply_calibration(get_current_time_us());
    
    if (timer->next_expiry_us <= current_time) {
        return 0;
    }
    
    return timer->next_expiry_us - current_time;
}

uint64_t pico_rtos_hires_timer_get_period_us(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return 0;
    }
    
    return timer->period_us;
}

pico_rtos_hires_timer_mode_t pico_rtos_hires_timer_get_mode(pico_rtos_hires_timer_t *timer)
{
    if (timer == NULL || !timer->active) {
        return PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT;
    }
    
    return timer->mode;
}

// Time functions
uint64_t pico_rtos_hires_timer_get_time_us(void)
{
    return apply_calibration(get_current_time_us());
}

uint64_t pico_rtos_hires_timer_get_time_ns(void)
{
    // RP2040 timer resolution is 1 microsecond, so multiply by 1000
    return pico_rtos_hires_timer_get_time_us() * 1000;
}

void pico_rtos_hires_timer_delay_us(uint64_t delay_us)
{
    uint64_t start_time = get_current_time_us();
    uint64_t end_time = start_time + delay_us;
    
    while (get_current_time_us() < end_time) {
        // Busy wait
        __asm volatile ("nop");
    }
}

bool pico_rtos_hires_timer_delay_until_us(uint64_t target_time_us)
{
    uint64_t current_time = get_current_time_us();
    
    if (target_time_us <= current_time) {
        return false; // Target time already passed
    }
    
    pico_rtos_hires_timer_delay_us(target_time_us - current_time);
    return true;
}

// Process timer expirations (called by interrupt handler)
void pico_rtos_hires_timer_process_expirations(void)
{
    uint64_t current_time = apply_calibration(get_current_time_us());
    
    critical_section_enter_blocking(&g_hires_timer_subsystem.cs);
    
    while (g_hires_timer_subsystem.active_timers != NULL &&
           g_hires_timer_subsystem.active_timers->next_expiry_us <= current_time) {
        
        pico_rtos_hires_timer_t *timer = g_hires_timer_subsystem.active_timers;
        
        // Remove from active list
        remove_timer_from_list(timer);
        
        // Update statistics
        timer->expiration_count++;
        g_hires_timer_subsystem.total_expirations++;
        
        // Calculate drift compensation
        calculate_drift_compensation(timer, current_time);
        
        // Execute callback
        uint64_t callback_start = get_current_time_us();
        timer->callback(timer->param);
        uint64_t callback_duration = get_current_time_us() - callback_start;
        
        if (callback_duration > timer->max_callback_duration_us) {
            timer->max_callback_duration_us = callback_duration;
        }
        
        // Handle timer mode
        if (timer->mode == PICO_RTOS_HIRES_TIMER_MODE_PERIODIC) {
            // Reschedule periodic timer
            timer->next_expiry_us = current_time + timer->period_us - timer->drift_compensation_us;
            insert_timer_sorted(timer);
        } else {
            // One-shot timer - mark as expired
            timer->state = PICO_RTOS_HIRES_TIMER_STATE_EXPIRED;
        }
        
        timer->total_expirations++;
    }
    
    critical_section_exit(&g_hires_timer_subsystem.cs);
}

uint64_t pico_rtos_hires_timer_get_next_expiration(void)
{
    if (g_hires_timer_subsystem.active_timers == NULL) {
        return 0;
    }
    
    return g_hires_timer_subsystem.active_timers->next_expiry_us;
}

// Utility functions
const char *pico_rtos_hires_timer_get_error_string(pico_rtos_hires_timer_error_t error)
{
    if (error >= PICO_RTOS_HIRES_TIMER_ERROR_INVALID_PERIOD && 
        error <= PICO_RTOS_HIRES_TIMER_ERROR_INVALID_PARAMETER) {
        int index = error - PICO_RTOS_HIRES_TIMER_ERROR_INVALID_PERIOD + 1;
        if (index < sizeof(hires_timer_error_strings) / sizeof(hires_timer_error_strings[0])) {
            return hires_timer_error_strings[index];
        }
    }
    
    return hires_timer_error_strings[0]; // "No error"
}

const char *pico_rtos_hires_timer_get_state_string(pico_rtos_hires_timer_state_t state)
{
    if (state < sizeof(timer_state_strings) / sizeof(timer_state_strings[0])) {
        return timer_state_strings[state];
    }
    return "Unknown";
}

const char *pico_rtos_hires_timer_get_mode_string(pico_rtos_hires_timer_mode_t mode)
{
    if (mode < sizeof(timer_mode_strings) / sizeof(timer_mode_strings[0])) {
        return timer_mode_strings[mode];
    }
    return "Unknown";
}