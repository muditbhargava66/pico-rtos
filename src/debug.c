#include "pico_rtos/debug.h"
#include "pico_rtos/task.h"
#include "pico_rtos/config.h"
#include "pico_rtos.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#if PICO_RTOS_ENABLE_DEBUG

// =============================================================================
// INTERNAL CONSTANTS AND MACROS
// =============================================================================

#define STACK_PATTERN_BYTE 0xA5        ///< Pattern byte for stack initialization
#define STACK_OVERFLOW_GUARD 0xDEADBEEF ///< Guard pattern for overflow detection
#define MIN_STACK_FREE_BYTES 64         ///< Minimum free stack space before warning

// =============================================================================
// INTERNAL VARIABLES
// =============================================================================

static pico_rtos_stack_usage_method_t stack_usage_method = PICO_RTOS_STACK_USAGE_WATERMARK;
static uint64_t last_stats_reset_time = 0;

// Use accessor functions to get internal state from core.c

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    return time_us_64();
}

/**
 * @brief Calculate stack usage using pattern method
 */
static uint32_t calculate_stack_usage_pattern(pico_rtos_task_t *task) {
    if (!task || !task->stack_base) {
        return 0;
    }
    
    uint8_t *stack_start = (uint8_t *)task->stack_base;
    uint8_t *stack_end = stack_start + task->stack_size;
    uint8_t *current_ptr = stack_start;
    
    // Find the first non-pattern byte from the bottom of the stack
    while (current_ptr < stack_end && *current_ptr == STACK_PATTERN_BYTE) {
        current_ptr++;
    }
    
    // Calculate used bytes
    uint32_t used_bytes = stack_end - current_ptr;
    return used_bytes > task->stack_size ? task->stack_size : used_bytes;
}

/**
 * @brief Calculate stack usage using watermark method
 */
static uint32_t calculate_stack_usage_watermark(pico_rtos_task_t *task) {
    if (!task || !task->stack_base || !task->stack_ptr) {
        return 0;
    }
    
    uint8_t *stack_start = (uint8_t *)task->stack_base;
    uint8_t *current_sp = (uint8_t *)task->stack_ptr;
    
    // Calculate current usage based on stack pointer position
    if (current_sp < stack_start) {
        return task->stack_size; // Stack overflow
    }
    
    uint32_t used_bytes = (stack_start + task->stack_size) - current_sp;
    return used_bytes > task->stack_size ? task->stack_size : used_bytes;
}

/**
 * @brief Get task from task list by pointer
 */
static pico_rtos_task_t *find_task_in_list(pico_rtos_task_t *target_task) {
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    
    while (task) {
        if (task == target_task) {
            return task;
        }
        task = task->next;
    }
    
    return NULL;
}

/**
 * @brief Count total tasks in the system
 */
static uint32_t count_total_tasks(void) {
    uint32_t count = 0;
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    
    while (task) {
        count++;
        task = task->next;
    }
    
    return count;
}

/**
 * @brief Count tasks by state
 */
static void count_tasks_by_state(uint32_t *ready, uint32_t *running, uint32_t *blocked, 
                                uint32_t *suspended, uint32_t *terminated) {
    *ready = *running = *blocked = *suspended = *terminated = 0;
    
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    while (task) {
        switch (task->state) {
            case PICO_RTOS_TASK_STATE_READY:
                (*ready)++;
                break;
            case PICO_RTOS_TASK_STATE_RUNNING:
                (*running)++;
                break;
            case PICO_RTOS_TASK_STATE_BLOCKED:
                (*blocked)++;
                break;
            case PICO_RTOS_TASK_STATE_SUSPENDED:
                (*suspended)++;
                break;
            case PICO_RTOS_TASK_STATE_TERMINATED:
                (*terminated)++;
                break;
        }
        task = task->next;
    }
}

// =============================================================================
// TASK INSPECTION IMPLEMENTATION
// =============================================================================

bool pico_rtos_debug_get_task_info(pico_rtos_task_t *task, pico_rtos_task_info_t *info) {
    if (!info) {
        return false;
    }
    
    // Use current task if none specified
    if (!task) {
        task = pico_rtos_debug_get_current_task();
    }
    
    if (!task) {
        return false;
    }
    
    // Verify task is in the task list
    if (!find_task_in_list(task)) {
        return false;
    }
    
    // Enter critical section to ensure atomic access
    pico_rtos_enter_critical();
    
    // Basic task information
    info->name = task->name ? task->name : "Unknown";
    info->priority = task->priority;
    info->original_priority = task->original_priority;
    info->state = task->state;
    info->block_reason = task->block_reason;
    
    // Stack information
    info->stack_size = task->stack_size;
    info->stack_base = task->stack_base;
    info->stack_ptr = task->stack_ptr;
    info->stack_top = (uint8_t *)task->stack_base + task->stack_size;
    
    // Calculate stack usage based on configured method
    uint32_t current_usage, peak_usage, free_space;
    if (pico_rtos_debug_calculate_stack_usage(task, &current_usage, &peak_usage, &free_space)) {
        info->stack_usage = current_usage;
        info->stack_high_water = peak_usage;
        info->stack_free = free_space;
        info->stack_usage_percent = (float)current_usage / task->stack_size * 100.0f;
    } else {
        info->stack_usage = 0;
        info->stack_high_water = 0;
        info->stack_free = task->stack_size;
        info->stack_usage_percent = 0.0f;
    }
    
    // Runtime statistics (these would need to be tracked in the task structure)
    // For now, we'll use placeholder values since the task structure doesn't have these fields yet
    info->cpu_time_us = 0; // Would need to be tracked during context switches
    info->context_switches = 0; // Would need to be tracked during context switches
    info->last_run_time = pico_rtos_debug_get_system_tick_count();
    info->creation_time = 0; // Would need to be set during task creation
    
    // Task function information
    info->function_ptr = (void *)task->function;
    info->param = task->param;
    
    // Blocking information
    info->blocking_object = task->blocking_object;
    info->block_start_time = 0; // Would need to be tracked when blocking starts
    info->block_timeout = 0; // Would need to be tracked when blocking starts
    
    pico_rtos_exit_critical();
    
    return true;
}

uint32_t pico_rtos_debug_get_all_task_info(pico_rtos_task_info_t *info_array, uint32_t max_tasks) {
    if (!info_array || max_tasks == 0) {
        return 0;
    }
    
    uint32_t task_count = 0;
    
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    while (task && task_count < max_tasks) {
        if (pico_rtos_debug_get_task_info(task, &info_array[task_count])) {
            task_count++;
        }
        task = task->next;
    }
    
    pico_rtos_exit_critical();
    
    return task_count;
}

bool pico_rtos_debug_get_system_inspection(pico_rtos_system_inspection_t *summary) {
    if (!summary) {
        return false;
    }
    
    uint64_t start_time = get_time_us();
    
    pico_rtos_enter_critical();
    
    // Count tasks by state
    count_tasks_by_state(&summary->ready_tasks, &summary->running_tasks,
                        &summary->blocked_tasks, &summary->suspended_tasks,
                        &summary->terminated_tasks);
    
    summary->total_tasks = summary->ready_tasks + summary->running_tasks +
                          summary->blocked_tasks + summary->suspended_tasks +
                          summary->terminated_tasks;
    
    // Calculate stack statistics
    summary->total_stack_allocated = 0;
    summary->total_stack_used = 0;
    summary->highest_stack_usage_percent = 0.0f;
    
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    while (task) {
        summary->total_stack_allocated += task->stack_size;
        
        uint32_t current_usage, peak_usage, free_space;
        if (pico_rtos_debug_calculate_stack_usage(task, &current_usage, &peak_usage, &free_space)) {
            summary->total_stack_used += current_usage;
            float usage_percent = (float)current_usage / task->stack_size * 100.0f;
            if (usage_percent > summary->highest_stack_usage_percent) {
                summary->highest_stack_usage_percent = usage_percent;
            }
        }
        
        task = task->next;
    }
    
    // Runtime statistics
    summary->total_context_switches = 0; // Would need to be tracked globally
    summary->total_cpu_time_us = 0; // Would need to be tracked globally
    
    summary->inspection_timestamp = pico_rtos_debug_get_system_tick_count();
    
    pico_rtos_exit_critical();
    
    uint64_t end_time = get_time_us();
    summary->inspection_time_us = (uint32_t)(end_time - start_time);
    
    return true;
}

bool pico_rtos_debug_find_task_by_name(const char *name, pico_rtos_task_info_t *info) {
    if (!name || !info) {
        return false;
    }
    
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    while (task) {
        if (task->name && strcmp(task->name, name) == 0) {
            pico_rtos_exit_critical();
            return pico_rtos_debug_get_task_info(task, info);
        }
        task = task->next;
    }
    
    pico_rtos_exit_critical();
    
    return false;
}

// =============================================================================
// STACK MONITORING IMPLEMENTATION
// =============================================================================

void pico_rtos_debug_set_stack_usage_method(pico_rtos_stack_usage_method_t method) {
    stack_usage_method = method;
}

pico_rtos_stack_usage_method_t pico_rtos_debug_get_stack_usage_method(void) {
    return stack_usage_method;
}

bool pico_rtos_debug_calculate_stack_usage(pico_rtos_task_t *task, 
                                          uint32_t *current_usage,
                                          uint32_t *peak_usage, 
                                          uint32_t *free_space) {
    if (!current_usage || !peak_usage || !free_space) {
        return false;
    }
    
    // Use current task if none specified
    if (!task) {
        task = pico_rtos_debug_get_current_task();
    }
    
    if (!task) {
        return false;
    }
    
    uint32_t usage;
    if (stack_usage_method == PICO_RTOS_STACK_USAGE_PATTERN) {
        usage = calculate_stack_usage_pattern(task);
    } else {
        usage = calculate_stack_usage_watermark(task);
    }
    
    *current_usage = usage;
    *peak_usage = usage; // For now, assume current is peak (would need tracking)
    *free_space = task->stack_size > usage ? task->stack_size - usage : 0;
    
    return true;
}

bool pico_rtos_debug_check_stack_overflow(pico_rtos_task_t *task, 
                                         pico_rtos_stack_overflow_info_t *overflow_info) {
    // Use current task if none specified
    if (!task) {
        task = pico_rtos_debug_get_current_task();
    }
    
    if (!task) {
        return false;
    }
    
    uint32_t current_usage, peak_usage, free_space;
    if (!pico_rtos_debug_calculate_stack_usage(task, &current_usage, &peak_usage, &free_space)) {
        return false;
    }
    
    bool overflow_detected = (free_space < MIN_STACK_FREE_BYTES) || (current_usage >= task->stack_size);
    
    if (overflow_detected && overflow_info) {
        overflow_info->task = task;
        overflow_info->task_name = task->name ? task->name : "Unknown";
        overflow_info->stack_size = task->stack_size;
        overflow_info->overflow_bytes = current_usage > task->stack_size ? 
                                       current_usage - task->stack_size : 0;
        overflow_info->stack_base = task->stack_base;
        overflow_info->current_sp = task->stack_ptr;
        overflow_info->detection_time = pico_rtos_debug_get_system_tick_count();
    }
    
    return overflow_detected;
}

uint32_t pico_rtos_debug_check_all_stack_overflows(pico_rtos_stack_overflow_info_t *overflow_info,
                                                   uint32_t max_overflows) {
    if (!overflow_info || max_overflows == 0) {
        return 0;
    }
    
    uint32_t overflow_count = 0;
    
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    while (task && overflow_count < max_overflows) {
        if (pico_rtos_debug_check_stack_overflow(task, &overflow_info[overflow_count])) {
            overflow_count++;
        }
        task = task->next;
    }
    
    pico_rtos_exit_critical();
    
    return overflow_count;
}

bool pico_rtos_debug_init_stack_pattern(pico_rtos_task_t *task) {
    if (!task || !task->stack_base) {
        return false;
    }
    
    // Fill stack with pattern bytes
    memset(task->stack_base, STACK_PATTERN_BYTE, task->stack_size);
    
    return true;
}

// =============================================================================
// RUNTIME STATISTICS IMPLEMENTATION
// =============================================================================

bool pico_rtos_debug_reset_task_stats(pico_rtos_task_t *task) {
    // Use current task if none specified
    if (!task) {
        task = pico_rtos_debug_get_current_task();
    }
    
    if (!task) {
        return false;
    }
    
    // Reset statistics (would need to be implemented in task structure)
    // For now, this is a placeholder
    
    return true;
}

void pico_rtos_debug_reset_all_task_stats(void) {
    last_stats_reset_time = get_time_us();
    
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    while (task) {
        pico_rtos_debug_reset_task_stats(task);
        task = task->next;
    }
    
    pico_rtos_exit_critical();
}

float pico_rtos_debug_get_task_cpu_usage(pico_rtos_task_t *task, uint32_t period_ms) {
    // Use current task if none specified
    if (!task) {
        task = pico_rtos_debug_get_current_task();
    }
    
    if (!task) {
        return 0.0f;
    }
    
    // This would need to be implemented with proper CPU time tracking
    // For now, return a placeholder value
    return 0.0f;
}

uint32_t pico_rtos_debug_get_system_cpu_usage(float *task_usage, uint32_t max_tasks, 
                                              float *idle_percentage) {
    if (!task_usage || !idle_percentage || max_tasks == 0) {
        return 0;
    }
    
    uint32_t task_count = 0;
    
    pico_rtos_enter_critical();
    
    pico_rtos_task_t *task = pico_rtos_debug_get_task_list();
    while (task && task_count < max_tasks) {
        task_usage[task_count] = pico_rtos_debug_get_task_cpu_usage(task, 0);
        task_count++;
        task = task->next;
    }
    
    pico_rtos_exit_critical();
    
    // Calculate idle percentage (placeholder)
    *idle_percentage = 50.0f; // Would need proper calculation
    
    return task_count;
}

// =============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// =============================================================================

const char *pico_rtos_debug_task_state_to_string(pico_rtos_task_state_t state) {
    switch (state) {
        case PICO_RTOS_TASK_STATE_READY:
            return "READY";
        case PICO_RTOS_TASK_STATE_RUNNING:
            return "RUNNING";
        case PICO_RTOS_TASK_STATE_BLOCKED:
            return "BLOCKED";
        case PICO_RTOS_TASK_STATE_SUSPENDED:
            return "SUSPENDED";
        case PICO_RTOS_TASK_STATE_TERMINATED:
            return "TERMINATED";
        default:
            return "UNKNOWN";
    }
}

const char *pico_rtos_debug_block_reason_to_string(pico_rtos_block_reason_t reason) {
    switch (reason) {
        case PICO_RTOS_BLOCK_REASON_NONE:
            return "NONE";
        case PICO_RTOS_BLOCK_REASON_DELAY:
            return "DELAY";
        case PICO_RTOS_BLOCK_REASON_QUEUE_FULL:
            return "QUEUE_FULL";
        case PICO_RTOS_BLOCK_REASON_QUEUE_EMPTY:
            return "QUEUE_EMPTY";
        case PICO_RTOS_BLOCK_REASON_SEMAPHORE:
            return "SEMAPHORE";
        case PICO_RTOS_BLOCK_REASON_MUTEX:
            return "MUTEX";
        case PICO_RTOS_BLOCK_REASON_EVENT_GROUP:
            return "EVENT_GROUP";
        default:
            return "UNKNOWN";
    }
}

uint32_t pico_rtos_debug_format_task_info(const pico_rtos_task_info_t *info, 
                                         char *buffer, uint32_t buffer_size) {
    if (!info || !buffer || buffer_size == 0) {
        return 0;
    }
    
    return snprintf(buffer, buffer_size,
        "Task: %s\n"
        "  Priority: %lu (orig: %lu)\n"
        "  State: %s\n"
        "  Block Reason: %s\n"
        "  Stack: %lu/%lu bytes (%.1f%% used, %lu free)\n"
        "  CPU Time: %llu us\n"
        "  Context Switches: %lu\n",
        info->name,
        info->priority, info->original_priority,
        pico_rtos_debug_task_state_to_string(info->state),
        pico_rtos_debug_block_reason_to_string(info->block_reason),
        info->stack_usage, info->stack_size, info->stack_usage_percent, info->stack_free,
        info->cpu_time_us,
        info->context_switches
    );
}

void pico_rtos_debug_print_task_info(const pico_rtos_task_info_t *info) {
    if (!info) {
        return;
    }
    
    char buffer[512];
    pico_rtos_debug_format_task_info(info, buffer, sizeof(buffer));
    printf("%s", buffer);
}

void pico_rtos_debug_print_system_inspection(const pico_rtos_system_inspection_t *summary) {
    if (!summary) {
        return;
    }
    
    printf("System Inspection Summary:\n");
    printf("  Total Tasks: %lu\n", summary->total_tasks);
    printf("  Task States: Ready=%lu, Running=%lu, Blocked=%lu, Suspended=%lu, Terminated=%lu\n",
           summary->ready_tasks, summary->running_tasks, summary->blocked_tasks,
           summary->suspended_tasks, summary->terminated_tasks);
    printf("  Stack Usage: %lu/%lu bytes allocated, highest usage %.1lu%%\n",
           summary->total_stack_used, summary->total_stack_allocated,
           summary->highest_stack_usage_percent);
    printf("  Runtime: %lu context switches, %llu us total CPU time\n",
           summary->total_context_switches, summary->total_cpu_time_us);
    printf("  Inspection took %lu us at tick %lu\n",
           summary->inspection_time_us, summary->inspection_timestamp);
}

#endif // PICO_RTOS_ENABLE_DEBUG