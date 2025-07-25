/**
 * @file deadlock.c
 * @brief Deadlock Detection System Implementation
 * 
 * This module implements comprehensive deadlock detection capabilities for
 * Pico-RTOS using resource dependency tracking and cycle detection algorithms.
 */

#include "pico_rtos/deadlock.h"
#include "pico_rtos/task.h"
#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include "hardware/timer.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

static pico_rtos_deadlock_detector_t g_deadlock_detector = {0};

// =============================================================================
// STRING CONSTANTS
// =============================================================================

static const char *resource_type_strings[] = {
    "Mutex",
    "Semaphore",
    "Queue",
    "Event Group",
    "Stream Buffer",
    "Memory Pool",
    "Custom"
};

static const char *deadlock_state_strings[] = {
    "None",
    "Potential",
    "Detected",
    "Resolved"
};

static const char *deadlock_action_strings[] = {
    "None",
    "Abort Task",
    "Release Resource",
    "Timeout Operation",
    "Callback"
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
 * @brief Find resource by ID
 * @param resource_id Resource ID to find
 * @return Pointer to resource, or NULL if not found
 */
static pico_rtos_deadlock_resource_t *find_resource_by_id(uint32_t resource_id)
{
    for (uint32_t i = 0; i < g_deadlock_detector.resource_count; i++) {
        if (g_deadlock_detector.resources[i].resource_id == resource_id &&
            g_deadlock_detector.resources[i].active) {
            return &g_deadlock_detector.resources[i];
        }
    }
    return NULL;
}

/**
 * @brief Find resource by pointer
 * @param resource_ptr Pointer to resource
 * @return Pointer to resource structure, or NULL if not found
 */
static pico_rtos_deadlock_resource_t *find_resource_by_ptr(void *resource_ptr)
{
    for (uint32_t i = 0; i < g_deadlock_detector.resource_count; i++) {
        if (g_deadlock_detector.resources[i].resource_ptr == resource_ptr &&
            g_deadlock_detector.resources[i].active) {
            return &g_deadlock_detector.resources[i];
        }
    }
    return NULL;
}

/**
 * @brief Find task dependency structure
 * @param task Task to find
 * @return Pointer to task dependency, or NULL if not found
 */
static pico_rtos_task_dependency_t *find_task_dependency(pico_rtos_task_t *task)
{
    for (uint32_t i = 0; i < g_deadlock_detector.task_count; i++) {
        if (g_deadlock_detector.task_deps[i].task == task) {
            return &g_deadlock_detector.task_deps[i];
        }
    }
    return NULL;
}

/**
 * @brief Create or get task dependency structure
 * @param task Task to create dependency for
 * @return Pointer to task dependency, or NULL if no space
 */
static pico_rtos_task_dependency_t *get_or_create_task_dependency(pico_rtos_task_t *task)
{
    pico_rtos_task_dependency_t *dep = find_task_dependency(task);
    if (dep != NULL) {
        return dep;
    }
    
    if (g_deadlock_detector.task_count >= PICO_RTOS_DEADLOCK_MAX_TASKS) {
        return NULL;
    }
    
    dep = &g_deadlock_detector.task_deps[g_deadlock_detector.task_count++];
    memset(dep, 0, sizeof(pico_rtos_task_dependency_t));
    dep->task = task;
    
    return dep;
}

/**
 * @brief Perform depth-first search for cycle detection
 * @param task Starting task
 * @param visited Array to track visited tasks
 * @param recursion_stack Array to track recursion stack
 * @param path Array to store path
 * @param path_length Current path length
 * @param result Result structure to fill if cycle found
 * @return true if cycle detected, false otherwise
 */
static bool dfs_cycle_detection(pico_rtos_task_t *task,
                               bool *visited,
                               bool *recursion_stack,
                               pico_rtos_task_t **path,
                               uint32_t *path_length,
                               pico_rtos_deadlock_result_t *result)
{
    if (g_deadlock_detector.detection_depth > g_deadlock_detector.max_detection_depth) {
        g_deadlock_detector.max_detection_depth = g_deadlock_detector.detection_depth;
    }
    
    pico_rtos_task_dependency_t *dep = find_task_dependency(task);
    if (dep == NULL) {
        return false;
    }
    
    // Find task index
    uint32_t task_index = dep - g_deadlock_detector.task_deps;
    
    visited[task_index] = true;
    recursion_stack[task_index] = true;
    path[*path_length] = task;
    (*path_length)++;
    
    g_deadlock_detector.detection_depth++;
    
    // Check if task is waiting for a resource
    if (dep->waiting_for != NULL) {
        pico_rtos_deadlock_resource_t *resource = dep->waiting_for;
        
        // Check if resource has an owner
        if (resource->owner != NULL) {
            pico_rtos_task_dependency_t *owner_dep = find_task_dependency(resource->owner);
            if (owner_dep != NULL) {
                uint32_t owner_index = owner_dep - g_deadlock_detector.task_deps;
                
                if (recursion_stack[owner_index]) {
                    // Cycle detected!
                    result->state = PICO_RTOS_DEADLOCK_STATE_DETECTED;
                    result->cycle_length = *path_length;
                    
                    // Copy cycle information
                    for (uint32_t i = 0; i < *path_length && i < PICO_RTOS_DEADLOCK_MAX_TASKS; i++) {
                        result->cycle_tasks[i] = path[i];
                    }
                    
                    // Find cycle start and copy resources
                    uint32_t cycle_start = 0;
                    for (uint32_t i = 0; i < *path_length; i++) {
                        if (path[i] == resource->owner) {
                            cycle_start = i;
                            break;
                        }
                    }
                    
                    uint32_t resource_count = 0;
                    for (uint32_t i = cycle_start; i < *path_length && resource_count < PICO_RTOS_DEADLOCK_MAX_RESOURCES; i++) {
                        pico_rtos_task_dependency_t *cycle_dep = find_task_dependency(path[i]);
                        if (cycle_dep && cycle_dep->waiting_for) {
                            result->cycle_resources[resource_count++] = cycle_dep->waiting_for;
                        }
                    }
                    
                    result->description = "Circular wait detected in resource dependency graph";
                    
                    g_deadlock_detector.detection_depth--;
                    return true;
                }
                
                if (!visited[owner_index]) {
                    if (dfs_cycle_detection(resource->owner, visited, recursion_stack, 
                                          path, path_length, result)) {
                        g_deadlock_detector.detection_depth--;
                        return true;
                    }
                }
            }
        }
    }
    
    recursion_stack[task_index] = false;
    (*path_length)--;
    g_deadlock_detector.detection_depth--;
    
    return false;
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_deadlock_init(void)
{
    if (g_deadlock_detector.initialized) {
        return true;
    }
    
    // Initialize critical section
    critical_section_init(&g_deadlock_detector.cs);
    
    // Initialize detector state
    memset(&g_deadlock_detector, 0, sizeof(g_deadlock_detector));
    g_deadlock_detector.enabled = true;
    g_deadlock_detector.next_resource_id = 1;
    g_deadlock_detector.max_detection_depth = 0;
    
    // Initialize resource critical sections
    for (uint32_t i = 0; i < PICO_RTOS_DEADLOCK_MAX_RESOURCES; i++) {
        critical_section_init(&g_deadlock_detector.resources[i].cs);
    }
    
    g_deadlock_detector.initialized = true;
    
    PICO_RTOS_LOG_INFO("Deadlock detection system initialized");
    return true;
}

void pico_rtos_deadlock_set_enabled(bool enabled)
{
    if (!g_deadlock_detector.initialized) {
        return;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    g_deadlock_detector.enabled = enabled;
    critical_section_exit(&g_deadlock_detector.cs);
    
    PICO_RTOS_LOG_INFO("Deadlock detection %s", enabled ? "enabled" : "disabled");
}

bool pico_rtos_deadlock_is_enabled(void)
{
    if (!g_deadlock_detector.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    bool enabled = g_deadlock_detector.enabled;
    critical_section_exit(&g_deadlock_detector.cs);
    
    return enabled;
}

void pico_rtos_deadlock_set_callback(pico_rtos_deadlock_callback_t callback, void *user_data)
{
    if (!g_deadlock_detector.initialized) {
        return;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    g_deadlock_detector.callback = callback;
    g_deadlock_detector.callback_data = user_data;
    critical_section_exit(&g_deadlock_detector.cs);
}

uint32_t pico_rtos_deadlock_register_resource(void *resource_ptr,
                                             pico_rtos_resource_type_t type,
                                             const char *name)
{
    if (!g_deadlock_detector.initialized || resource_ptr == NULL) {
        return 0;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    
    // Check if resource already registered
    if (find_resource_by_ptr(resource_ptr) != NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return 0;
    }
    
    // Find free resource slot
    if (g_deadlock_detector.resource_count >= PICO_RTOS_DEADLOCK_MAX_RESOURCES) {
        critical_section_exit(&g_deadlock_detector.cs);
        PICO_RTOS_LOG_ERROR("Maximum number of resources exceeded");
        return 0;
    }
    
    pico_rtos_deadlock_resource_t *resource = &g_deadlock_detector.resources[g_deadlock_detector.resource_count++];
    
    // Initialize resource
    memset(resource, 0, sizeof(pico_rtos_deadlock_resource_t));
    resource->resource_id = g_deadlock_detector.next_resource_id++;
    resource->type = type;
    resource->resource_ptr = resource_ptr;
    resource->name = name;
    resource->active = true;
    
    uint32_t resource_id = resource->resource_id;
    
    critical_section_exit(&g_deadlock_detector.cs);
    
    PICO_RTOS_LOG_DEBUG("Registered resource %u (%s): %s", 
                        resource_id, 
                        pico_rtos_deadlock_get_resource_type_string(type),
                        name ? name : "unnamed");
    
    return resource_id;
}

bool pico_rtos_deadlock_unregister_resource(uint32_t resource_id)
{
    if (!g_deadlock_detector.initialized || resource_id == 0) {
        return false;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    
    pico_rtos_deadlock_resource_t *resource = find_resource_by_id(resource_id);
    if (resource == NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return false;
    }
    
    // Mark resource as inactive
    resource->active = false;
    
    // Clean up any task dependencies
    for (uint32_t i = 0; i < g_deadlock_detector.task_count; i++) {
        pico_rtos_task_dependency_t *dep = &g_deadlock_detector.task_deps[i];
        
        // Remove from owned resources
        for (uint32_t j = 0; j < dep->owned_count; j++) {
            if (dep->owned_resources[j] == resource) {
                // Shift remaining resources
                for (uint32_t k = j; k < dep->owned_count - 1; k++) {
                    dep->owned_resources[k] = dep->owned_resources[k + 1];
                }
                dep->owned_count--;
                break;
            }
        }
        
        // Clear waiting reference
        if (dep->waiting_for == resource) {
            dep->waiting_for = NULL;
        }
    }
    
    critical_section_exit(&g_deadlock_detector.cs);
    
    PICO_RTOS_LOG_DEBUG("Unregistered resource %u", resource_id);
    return true;
}

bool pico_rtos_deadlock_request_resource(uint32_t resource_id, pico_rtos_task_t *task)
{
    if (!g_deadlock_detector.initialized || !g_deadlock_detector.enabled || 
        resource_id == 0 || task == NULL) {
        return true; // Allow operation if detection disabled
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    
    pico_rtos_deadlock_resource_t *resource = find_resource_by_id(resource_id);
    if (resource == NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return true;
    }
    
    pico_rtos_task_dependency_t *dep = get_or_create_task_dependency(task);
    if (dep == NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return true; // Allow operation if can't track
    }
    
    // Set waiting relationship
    dep->waiting_for = resource;
    
    // Add to resource's waiting list
    if (resource->waiting_count < PICO_RTOS_DEADLOCK_MAX_TASKS) {
        resource->waiting_tasks[resource->waiting_count++] = task;
    }
    
    // Perform deadlock detection
    pico_rtos_deadlock_result_t result;
    bool safe = true;
    
    if (pico_rtos_deadlock_detect(&result)) {
        if (result.state == PICO_RTOS_DEADLOCK_STATE_DETECTED) {
            safe = false;
            g_deadlock_detector.total_detections++;
            
            PICO_RTOS_LOG_WARN("Potential deadlock detected for resource %u", resource_id);
            
            // Call callback if registered
            if (g_deadlock_detector.callback != NULL) {
                pico_rtos_deadlock_action_t action = g_deadlock_detector.callback(
                    &g_deadlock_detector,
                    result.cycle_resources,
                    result.cycle_length,
                    result.cycle_tasks,
                    result.cycle_length);
                
                if (action != PICO_RTOS_DEADLOCK_ACTION_NONE) {
                    pico_rtos_deadlock_resolve(&result, action);
                }
            }
        }
    }
    
    critical_section_exit(&g_deadlock_detector.cs);
    
    return safe;
}

bool pico_rtos_deadlock_acquire_resource(uint32_t resource_id, pico_rtos_task_t *task)
{
    if (!g_deadlock_detector.initialized || resource_id == 0 || task == NULL) {
        return true;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    
    pico_rtos_deadlock_resource_t *resource = find_resource_by_id(resource_id);
    if (resource == NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return true;
    }
    
    pico_rtos_task_dependency_t *dep = get_or_create_task_dependency(task);
    if (dep == NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return true;
    }
    
    // Set ownership
    resource->owner = task;
    resource->acquisition_count++;
    
    // Add to task's owned resources
    if (dep->owned_count < PICO_RTOS_DEADLOCK_MAX_RESOURCES) {
        dep->owned_resources[dep->owned_count++] = resource;
    }
    
    // Clear waiting relationship
    dep->waiting_for = NULL;
    
    // Remove from resource's waiting list
    for (uint32_t i = 0; i < resource->waiting_count; i++) {
        if (resource->waiting_tasks[i] == task) {
            // Shift remaining tasks
            for (uint32_t j = i; j < resource->waiting_count - 1; j++) {
                resource->waiting_tasks[j] = resource->waiting_tasks[j + 1];
            }
            resource->waiting_count--;
            break;
        }
    }
    
    critical_section_exit(&g_deadlock_detector.cs);
    
    PICO_RTOS_LOG_DEBUG("Task acquired resource %u", resource_id);
    return true;
}

bool pico_rtos_deadlock_release_resource(uint32_t resource_id, pico_rtos_task_t *task)
{
    if (!g_deadlock_detector.initialized || resource_id == 0 || task == NULL) {
        return true;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    
    pico_rtos_deadlock_resource_t *resource = find_resource_by_id(resource_id);
    if (resource == NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return true;
    }
    
    pico_rtos_task_dependency_t *dep = find_task_dependency(task);
    if (dep == NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return true;
    }
    
    // Clear ownership
    resource->owner = NULL;
    
    // Remove from task's owned resources
    for (uint32_t i = 0; i < dep->owned_count; i++) {
        if (dep->owned_resources[i] == resource) {
            // Shift remaining resources
            for (uint32_t j = i; j < dep->owned_count - 1; j++) {
                dep->owned_resources[j] = dep->owned_resources[j + 1];
            }
            dep->owned_count--;
            break;
        }
    }
    
    critical_section_exit(&g_deadlock_detector.cs);
    
    PICO_RTOS_LOG_DEBUG("Task released resource %u", resource_id);
    return true;
}

bool pico_rtos_deadlock_cancel_wait(uint32_t resource_id, pico_rtos_task_t *task)
{
    if (!g_deadlock_detector.initialized || resource_id == 0 || task == NULL) {
        return true;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    
    pico_rtos_deadlock_resource_t *resource = find_resource_by_id(resource_id);
    if (resource == NULL) {
        critical_section_exit(&g_deadlock_detector.cs);
        return true;
    }
    
    pico_rtos_task_dependency_t *dep = find_task_dependency(task);
    if (dep != NULL) {
        dep->waiting_for = NULL;
    }
    
    // Remove from resource's waiting list
    for (uint32_t i = 0; i < resource->waiting_count; i++) {
        if (resource->waiting_tasks[i] == task) {
            // Shift remaining tasks
            for (uint32_t j = i; j < resource->waiting_count - 1; j++) {
                resource->waiting_tasks[j] = resource->waiting_tasks[j + 1];
            }
            resource->waiting_count--;
            break;
        }
    }
    
    critical_section_exit(&g_deadlock_detector.cs);
    
    PICO_RTOS_LOG_DEBUG("Task cancelled wait for resource %u", resource_id);
    return true;
}

bool pico_rtos_deadlock_detect(pico_rtos_deadlock_result_t *result)
{
    if (!g_deadlock_detector.initialized || !g_deadlock_detector.enabled || result == NULL) {
        return false;
    }
    
    if (g_deadlock_detector.detection_in_progress) {
        return false; // Avoid recursive detection
    }
    
    uint64_t start_time = get_current_time_us();
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    
    g_deadlock_detector.detection_in_progress = true;
    g_deadlock_detector.detection_depth = 0;
    
    // Initialize result
    memset(result, 0, sizeof(pico_rtos_deadlock_result_t));
    result->state = PICO_RTOS_DEADLOCK_STATE_NONE;
    
    // Prepare for DFS
    bool visited[PICO_RTOS_DEADLOCK_MAX_TASKS] = {false};
    bool recursion_stack[PICO_RTOS_DEADLOCK_MAX_TASKS] = {false};
    pico_rtos_task_t *path[PICO_RTOS_DEADLOCK_MAX_TASKS];
    uint32_t path_length = 0;
    
    // Perform DFS from each unvisited task
    for (uint32_t i = 0; i < g_deadlock_detector.task_count; i++) {
        if (!visited[i]) {
            if (dfs_cycle_detection(g_deadlock_detector.task_deps[i].task,
                                  visited, recursion_stack, path, &path_length, result)) {
                break; // Cycle found
            }
        }
    }
    
    g_deadlock_detector.detection_in_progress = false;
    
    uint64_t end_time = get_current_time_us();
    uint32_t detection_time = (uint32_t)(end_time - start_time);
    
    result->detection_time_us = detection_time;
    g_deadlock_detector.total_detection_time_us += detection_time;
    
    if (detection_time > g_deadlock_detector.max_detection_time_us) {
        g_deadlock_detector.max_detection_time_us = detection_time;
    }
    
    critical_section_exit(&g_deadlock_detector.cs);
    
    return true;
}

// Utility functions
const char *pico_rtos_deadlock_get_resource_type_string(pico_rtos_resource_type_t type)
{
    if (type < sizeof(resource_type_strings) / sizeof(resource_type_strings[0])) {
        return resource_type_strings[type];
    }
    return "Unknown";
}

const char *pico_rtos_deadlock_get_state_string(pico_rtos_deadlock_state_t state)
{
    if (state < sizeof(deadlock_state_strings) / sizeof(deadlock_state_strings[0])) {
        return deadlock_state_strings[state];
    }
    return "Unknown";
}

const char *pico_rtos_deadlock_get_action_string(pico_rtos_deadlock_action_t action)
{
    if (action < sizeof(deadlock_action_strings) / sizeof(deadlock_action_strings[0])) {
        return deadlock_action_strings[action];
    }
    return "Unknown";
}

void pico_rtos_deadlock_get_statistics(pico_rtos_deadlock_statistics_t *stats)
{
    if (!g_deadlock_detector.initialized || stats == NULL) {
        return;
    }
    
    critical_section_enter_blocking(&g_deadlock_detector.cs);
    
    stats->active_resources = g_deadlock_detector.resource_count;
    stats->tracked_tasks = g_deadlock_detector.task_count;
    stats->total_detections = g_deadlock_detector.total_detections;
    stats->false_positives = g_deadlock_detector.false_positives;
    stats->successful_recoveries = g_deadlock_detector.successful_recoveries;
    stats->failed_recoveries = g_deadlock_detector.failed_recoveries;
    stats->total_detection_time_us = g_deadlock_detector.total_detection_time_us;
    stats->max_detection_time_us = g_deadlock_detector.max_detection_time_us;
    
    if (g_deadlock_detector.total_detections > 0) {
        stats->average_detection_time_us = 
            g_deadlock_detector.total_detection_time_us / g_deadlock_detector.total_detections;
        
        uint32_t total_attempts = g_deadlock_detector.total_detections + g_deadlock_detector.false_positives;
        if (total_attempts > 0) {
            stats->detection_accuracy = 
                ((double)g_deadlock_detector.total_detections / total_attempts) * 100.0;
        }
    }
    
    critical_section_exit(&g_deadlock_detector.cs);
}

void pico_rtos_deadlock_print_result(const pico_rtos_deadlock_result_t *result)
{
    if (result == NULL) {
        return;
    }
    
    printf("Deadlock Detection Result:\n");
    printf("  State: %s\n", pico_rtos_deadlock_get_state_string(result->state));
    printf("  Detection Time: %u us\n", result->detection_time_us);
    
    if (result->state == PICO_RTOS_DEADLOCK_STATE_DETECTED) {
        printf("  Cycle Length: %u\n", result->cycle_length);
        printf("  Description: %s\n", result->description ? result->description : "N/A");
        
        printf("  Tasks in Cycle:\n");
        for (uint32_t i = 0; i < result->cycle_length && i < PICO_RTOS_DEADLOCK_MAX_TASKS; i++) {
            if (result->cycle_tasks[i] != NULL) {
                printf("    Task %p\n", (void *)result->cycle_tasks[i]);
            }
        }
        
        printf("  Resources in Cycle:\n");
        for (uint32_t i = 0; i < result->cycle_length && i < PICO_RTOS_DEADLOCK_MAX_RESOURCES; i++) {
            if (result->cycle_resources[i] != NULL) {
                printf("    Resource %u (%s): %s\n",
                       result->cycle_resources[i]->resource_id,
                       pico_rtos_deadlock_get_resource_type_string(result->cycle_resources[i]->type),
                       result->cycle_resources[i]->name ? result->cycle_resources[i]->name : "unnamed");
            }
        }
    }
}