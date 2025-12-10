#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pico_rtos/smp.h"
#include "pico_rtos/config.h"
#include "pico_rtos/error.h"
#include "pico_rtos/logging.h"
#include "pico_rtos/mutex.h"
#include "pico_rtos/semaphore.h"
#include "pico_rtos/queue.h"
#include "pico_rtos/event_group.h"

#if PICO_ON_DEVICE
#include "pico/multicore.h"
#endif

// Forward declarations for functions that may not be available during testing
#if !PICO_ON_DEVICE
extern uint32_t pico_rtos_get_tick_count(void);
extern pico_rtos_task_t *pico_rtos_get_current_task(void);
extern uint32_t *pico_rtos_init_task_stack(uint32_t *stack_base, uint32_t stack_size,
                                          pico_rtos_task_function_t function, void *param);
#else
#include "pico_rtos/context_switch.h"
#include "pico_rtos.h"
#endif

// =============================================================================
// GLOBAL SMP STATE
// =============================================================================

static pico_rtos_smp_scheduler_t smp_scheduler;
static bool smp_system_initialized = false;

// Per-core idle tasks
static pico_rtos_task_t idle_tasks[2];
static uint32_t idle_task_stacks[2][PICO_RTOS_IDLE_TASK_STACK_SIZE];

// Migration request queue (simple circular buffer)
#define PICO_RTOS_MAX_MIGRATION_REQUESTS 8
static pico_rtos_migration_request_t migration_requests[PICO_RTOS_MAX_MIGRATION_REQUESTS];
static uint32_t migration_queue_head = 0;
static uint32_t migration_queue_tail = 0;
static pico_rtos_critical_section_t migration_queue_cs;

// Load balancing state
static uint32_t last_load_balance_time = 0;
static const uint32_t LOAD_BALANCE_INTERVAL_MS = 100; // Balance every 100ms

// Core health monitoring state
static bool health_monitoring_initialized = false;
static uint32_t last_health_check_time = 0;
static uint32_t health_check_counter = 0;

// Default health monitoring configuration
static const pico_rtos_core_health_config_t DEFAULT_HEALTH_CONFIG = {
    .enabled = true,
    .watchdog_timeout_ms = 5000,           // 5 second watchdog timeout
    .health_check_interval_ms = 1000,      // Check health every 1 second
    .max_missed_heartbeats = 3,            // Allow 3 missed heartbeats
    .recovery_timeout_ms = 10000,          // 10 second recovery timeout
    .auto_recovery_enabled = true,         // Enable automatic recovery
    .graceful_degradation_enabled = true   // Enable graceful degradation
};

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static void pico_rtos_smp_idle_task_function(void *param);
static bool pico_rtos_smp_init_idle_task(uint32_t core_id);
static void pico_rtos_smp_core1_entry(void);
static uint32_t pico_rtos_smp_calculate_core_load(uint32_t core_id);
static bool pico_rtos_smp_should_balance_load(void);
static uint32_t pico_rtos_smp_find_best_core_for_task(pico_rtos_task_t *task);
static bool pico_rtos_smp_enqueue_migration_request(pico_rtos_migration_request_t *request);
static bool pico_rtos_smp_dequeue_migration_request(pico_rtos_migration_request_t *request);
static void pico_rtos_smp_process_migration_requests(void);

// Core health monitoring forward declarations
static void pico_rtos_core_health_init_state(uint32_t core_id);
static bool pico_rtos_core_health_check_watchdog(uint32_t core_id);
static bool pico_rtos_core_health_check_heartbeat(uint32_t core_id);
static void pico_rtos_core_health_handle_failure(uint32_t core_id, pico_rtos_core_failure_type_t failure_type);
static bool pico_rtos_core_health_attempt_recovery(uint32_t core_id);
static uint32_t pico_rtos_core_health_migrate_all_tasks(uint32_t failed_core_id, uint32_t target_core_id);

// =============================================================================
// CORE IDENTIFICATION
// =============================================================================

uint32_t pico_rtos_smp_get_core_id(void) {
    return get_core_num();
}

// =============================================================================
// SMP INITIALIZATION
// =============================================================================

bool pico_rtos_smp_init(void) {
    if (smp_system_initialized) {
        PICO_RTOS_LOG_SMP_WARN("SMP system already initialized");
        return true;
    }

    PICO_RTOS_LOG_SMP_INFO("Initializing SMP scheduler system");

    // Initialize SMP scheduler structure
    memset(&smp_scheduler, 0, sizeof(pico_rtos_smp_scheduler_t));

    // Initialize SMP critical section
    pico_rtos_critical_section_init(&smp_scheduler.smp_cs);
    pico_rtos_critical_section_init(&migration_queue_cs);

    // Initialize per-core state
    for (int i = 0; i < 2; i++) {
        smp_scheduler.cores[i].core_id = i;
        smp_scheduler.cores[i].running_task = NULL;
        smp_scheduler.cores[i].ready_list = NULL;
        smp_scheduler.cores[i].scheduler_active = false;
        smp_scheduler.cores[i].context_switches = 0;
        smp_scheduler.cores[i].idle_time_us = 0;
        smp_scheduler.cores[i].total_runtime_us = 0;
        smp_scheduler.cores[i].task_count = 0;
        smp_scheduler.cores[i].load_percentage = 0;
        smp_scheduler.cores[i].migration_in_progress = false;
    }

    // Set default configuration
    smp_scheduler.load_balance_threshold = PICO_RTOS_SMP_LOAD_BALANCE_THRESHOLD;
    smp_scheduler.load_balancing_enabled = PICO_RTOS_ENABLE_SMP_LOAD_BALANCING;
    smp_scheduler.migration_count = 0;
    smp_scheduler.last_balance_time = 0;

    // Initialize idle tasks for both cores
    if (!pico_rtos_smp_init_idle_task(0)) {
        PICO_RTOS_LOG_SMP_ERROR("Failed to initialize idle task for core 0");
        return false;
    }

    if (!pico_rtos_smp_init_idle_task(1)) {
        PICO_RTOS_LOG_SMP_ERROR("Failed to initialize idle task for core 1");
        return false;
    }

    // Initialize migration queue
    migration_queue_head = 0;
    migration_queue_tail = 0;

    smp_scheduler.smp_initialized = true;
    smp_system_initialized = true;

    // Initialize health monitoring with default configuration
    if (!pico_rtos_core_health_init(NULL)) {
        PICO_RTOS_LOG_SMP_WARN("Failed to initialize health monitoring, continuing without it");
    }

    PICO_RTOS_LOG_SMP_INFO("SMP scheduler system initialized successfully");
    return true;
}

void pico_rtos_smp_start(void) {
    if (!smp_system_initialized) {
        PICO_RTOS_LOG_SMP_ERROR("SMP system not initialized");
        return;
    }

    PICO_RTOS_LOG_SMP_INFO("Starting SMP scheduler on both cores");

    // Start core 0 scheduler (current core)
    smp_scheduler.cores[0].scheduler_active = true;
    smp_scheduler.cores[0].running_task = &idle_tasks[0];

#if PICO_ON_DEVICE
    // Launch core 1
    multicore_launch_core1(pico_rtos_smp_core1_entry);

    // Wait for core 1 to be ready
    uint32_t timeout = 1000; // 1 second timeout
    while (!smp_scheduler.cores[1].scheduler_active && timeout > 0) {
        sleep_ms(1);
        timeout--;
    }

    if (!smp_scheduler.cores[1].scheduler_active) {
        PICO_RTOS_LOG_SMP_ERROR("Core 1 failed to start within timeout");
        return;
    }
#else
    // In non-device builds (testing), simulate core 1 being ready
    smp_scheduler.cores[1].scheduler_active = true;
    smp_scheduler.cores[1].running_task = &idle_tasks[1];
#endif

    // Start health monitoring
    pico_rtos_core_health_start();

    PICO_RTOS_LOG_SMP_INFO("SMP scheduler started on both cores");
}

// =============================================================================
// CORE 1 ENTRY POINT
// =============================================================================

static void pico_rtos_smp_core1_entry(void) {
    PICO_RTOS_LOG_SMP_INFO("Core 1 scheduler starting");

    // Initialize core 1 scheduler state
    smp_scheduler.cores[1].scheduler_active = true;
    smp_scheduler.cores[1].running_task = &idle_tasks[1];

    // Start scheduler loop on core 1
    // This would integrate with the existing scheduler system
    // For now, just run the idle task
    pico_rtos_smp_idle_task_function((void*)1);
}

// =============================================================================
// IDLE TASK MANAGEMENT
// =============================================================================

static bool pico_rtos_smp_init_idle_task(uint32_t core_id) {
    if (core_id >= 2) {
        return false;
    }

    pico_rtos_task_t *idle_task = &idle_tasks[core_id];
    uint32_t *idle_stack = idle_task_stacks[core_id];

    // Initialize idle task structure
    memset(idle_task, 0, sizeof(pico_rtos_task_t));

    char name_buffer[16];
    snprintf(name_buffer, sizeof(name_buffer), "IDLE_CORE%lu", core_id);
    idle_task->name = (core_id == 0) ? "IDLE_CORE0" : "IDLE_CORE1";
    idle_task->function = pico_rtos_smp_idle_task_function;
    idle_task->param = (void*)(uintptr_t)core_id;
    idle_task->stack_size = sizeof(idle_task_stacks[core_id]);
    idle_task->priority = 0; // Lowest priority
    idle_task->original_priority = 0;
    idle_task->state = PICO_RTOS_TASK_STATE_READY;
    idle_task->block_reason = PICO_RTOS_BLOCK_REASON_NONE;
    idle_task->auto_delete = false; // Static task

    // Set up stack
    idle_task->stack_base = idle_stack;

    // Set stack canary
    idle_stack[0] = 0xDEADBEEF;
    idle_stack[1] = 0xDEADBEEF;

    // Initialize stack for context switching
    idle_task->stack_ptr = pico_rtos_init_task_stack(idle_stack,
                                                    sizeof(idle_task_stacks[core_id]),
                                                    pico_rtos_smp_idle_task_function,
                                                    (void*)(uintptr_t)core_id);

    if (idle_task->stack_ptr == NULL) {
        PICO_RTOS_LOG_SMP_ERROR("Failed to initialize stack for idle task core %lu", core_id);
        return false;
    }

    // Initialize critical section
    pico_rtos_critical_section_init(&idle_task->cs);

    // Set SMP-specific fields
#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    idle_task->core_affinity = (core_id == 0) ? PICO_RTOS_CORE_AFFINITY_CORE0 : PICO_RTOS_CORE_AFFINITY_CORE1;
    idle_task->assigned_core = core_id;
    idle_task->cpu_time_us = 0;
    idle_task->context_switch_count = 0;
    idle_task->stack_high_water_mark = 0;
    idle_task->migration_count = 0;
    idle_task->migration_pending = false;
    idle_task->last_run_core = core_id;
    memset(idle_task->task_local_storage, 0, sizeof(idle_task->task_local_storage));
#endif

    PICO_RTOS_LOG_SMP_DEBUG("Idle task initialized for core %lu", core_id);
    return true;
}

static void pico_rtos_smp_idle_task_function(void *param) {
    uint32_t core_id = (uint32_t)(uintptr_t)param;
    uint32_t idle_counter = 0;

    PICO_RTOS_LOG_SMP_DEBUG("Idle task started on core %lu", core_id);

    while (1) {
        idle_counter++;

        // Update idle time statistics
        pico_rtos_smp_enter_critical();
        smp_scheduler.cores[core_id].idle_time_us += 1000; // Approximate 1ms per loop
        pico_rtos_smp_exit_critical();

        // Send heartbeat to indicate core is alive
        if (idle_counter % 10 == 0) {
            pico_rtos_core_health_send_heartbeat();
        }

        // Feed watchdog periodically
        if (idle_counter % 50 == 0) {
            pico_rtos_core_health_feed_watchdog();
        }

        // Process health monitoring (only on core 0 to avoid conflicts)
        if (core_id == 0 && idle_counter % 100 == 0) {
            pico_rtos_core_health_process();
        }

        // Process migration requests periodically
        if (idle_counter % 100 == 0) {
            pico_rtos_smp_process_migration_requests();
        }

        // Check for load balancing periodically
        if (smp_scheduler.load_balancing_enabled && idle_counter % 1000 == 0) {
            if (pico_rtos_smp_should_balance_load()) {
                pico_rtos_smp_balance_load();
            }
        }

        // Default idle behavior - wait for interrupt
        __wfi();
    }
}

// =============================================================================
// STATE ACCESS FUNCTIONS
// =============================================================================

pico_rtos_core_state_t *pico_rtos_smp_get_core_state(uint32_t core_id) {
    if (core_id >= 2 || !smp_system_initialized) {
        return NULL;
    }

    return &smp_scheduler.cores[core_id];
}

pico_rtos_smp_scheduler_t *pico_rtos_smp_get_scheduler_state(void) {
    if (!smp_system_initialized) {
        return NULL;
    }

    return &smp_scheduler;
}

// =============================================================================
// TASK AFFINITY MANAGEMENT
// =============================================================================

bool pico_rtos_smp_set_task_affinity(pico_rtos_task_t *task, pico_rtos_core_affinity_t affinity) {
    if (!smp_system_initialized) {
        PICO_RTOS_LOG_SMP_ERROR("SMP system not initialized");
        return false;
    }

    if (task == NULL) {
        task = pico_rtos_get_current_task();
    }

    if (task == NULL) {
        PICO_RTOS_LOG_SMP_ERROR("No task specified for affinity setting");
        return false;
    }

    pico_rtos_smp_enter_critical();

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    pico_rtos_core_affinity_t old_affinity = (pico_rtos_core_affinity_t)task->core_affinity;
    task->core_affinity = affinity;

    PICO_RTOS_LOG_SMP_DEBUG("Task %s affinity changed from %d to %d",
                           task->name ? task->name : "unnamed", old_affinity, affinity);

    // If affinity changed and task can't run on current core, migrate it
    if (old_affinity != affinity && !pico_rtos_smp_task_can_run_on_core(task, task->assigned_core)) {
        uint32_t target_core = pico_rtos_smp_find_best_core_for_task(task);
        if (target_core != task->assigned_core) {
            pico_rtos_migration_request_t request = {
                .task = task,
                .source_core = task->assigned_core,
                .target_core = target_core,
                .migration_reason = PICO_RTOS_MIGRATION_AFFINITY_CHANGE,
                .urgent = true
            };
            pico_rtos_smp_enqueue_migration_request(&request);
        }
    }
#endif

    pico_rtos_smp_exit_critical();
    return true;
}

pico_rtos_core_affinity_t pico_rtos_smp_get_task_affinity(pico_rtos_task_t *task) {
    if (!smp_system_initialized) {
        return PICO_RTOS_CORE_AFFINITY_NONE;
    }

    if (task == NULL) {
        task = pico_rtos_get_current_task();
    }

    if (task == NULL) {
        return PICO_RTOS_CORE_AFFINITY_NONE;
    }

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    return (pico_rtos_core_affinity_t)task->core_affinity;
#else
    return PICO_RTOS_CORE_AFFINITY_NONE;
#endif
}

bool pico_rtos_smp_task_can_run_on_core(pico_rtos_task_t *task, uint32_t core_id) {
    if (!smp_system_initialized || task == NULL || core_id >= 2) {
        return false;
    }

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    pico_rtos_core_affinity_t affinity = (pico_rtos_core_affinity_t)task->core_affinity;

    switch (affinity) {
        case PICO_RTOS_CORE_AFFINITY_NONE:
        case PICO_RTOS_CORE_AFFINITY_ANY:
            return true;
        case PICO_RTOS_CORE_AFFINITY_CORE0:
            return (core_id == 0);
        case PICO_RTOS_CORE_AFFINITY_CORE1:
            return (core_id == 1);
        default:
            return false;
    }
#else
    return (core_id == 0); // Single core mode, only core 0 exists
#endif
}

// =============================================================================
// TASK DISTRIBUTION ALGORITHMS
// =============================================================================

/**
 * @brief Task assignment strategies
 */
// Task assignment strategy enum is defined in smp.h

static pico_rtos_task_assignment_strategy_t assignment_strategy = PICO_RTOS_ASSIGN_LEAST_LOADED;
static uint32_t round_robin_next_core = 0;

/**
 * @brief Set task assignment strategy
 */
void pico_rtos_smp_set_assignment_strategy(pico_rtos_task_assignment_strategy_t strategy) {
    if (!smp_system_initialized) {
        return;
    }

    pico_rtos_smp_enter_critical();
    assignment_strategy = strategy;
    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_INFO("Task assignment strategy set to %d", strategy);
}

/**
 * @brief Get current task assignment strategy
 */
pico_rtos_task_assignment_strategy_t pico_rtos_smp_get_assignment_strategy(void) {
    if (!smp_system_initialized) {
        return PICO_RTOS_ASSIGN_LEAST_LOADED;
    }

    pico_rtos_task_assignment_strategy_t strategy;
    pico_rtos_smp_enter_critical();
    strategy = assignment_strategy;
    pico_rtos_smp_exit_critical();

    return strategy;
}

/**
 * @brief Find optimal core for new task based on current strategy
 */
static uint32_t pico_rtos_smp_assign_task_to_core(pico_rtos_task_t *task) {
    if (!task) {
        return 0;
    }

    // First check affinity constraints
    bool can_run_core0 = pico_rtos_smp_task_can_run_on_core(task, 0);
    bool can_run_core1 = pico_rtos_smp_task_can_run_on_core(task, 1);

    // If task can only run on one core, assign it there
    if (can_run_core0 && !can_run_core1) {
        return 0;
    }
    if (!can_run_core0 && can_run_core1) {
        return 1;
    }
    if (!can_run_core0 && !can_run_core1) {
        // This shouldn't happen, but default to core 0
        PICO_RTOS_LOG_SMP_WARN("Task %s cannot run on any core, defaulting to core 0",
                              task->name ? task->name : "unnamed");
        return 0;
    }

    // Task can run on both cores, use assignment strategy
    switch (assignment_strategy) {
        case PICO_RTOS_ASSIGN_ROUND_ROBIN:
            {
                uint32_t core = round_robin_next_core;
                round_robin_next_core = (round_robin_next_core + 1) % 2;
                return core;
            }

        case PICO_RTOS_ASSIGN_LEAST_LOADED:
            {
                uint32_t core0_load = pico_rtos_smp_calculate_core_load(0);
                uint32_t core1_load = pico_rtos_smp_calculate_core_load(1);
                return (core0_load <= core1_load) ? 0 : 1;
            }

        case PICO_RTOS_ASSIGN_PRIORITY_BASED:
            {
                // High priority tasks go to core 0, low priority to core 1
                return (task->priority >= 5) ? 0 : 1;
            }

        case PICO_RTOS_ASSIGN_AFFINITY_FIRST:
        default:
            // Default to least loaded
            {
                uint32_t core0_load = pico_rtos_smp_calculate_core_load(0);
                uint32_t core1_load = pico_rtos_smp_calculate_core_load(1);
                return (core0_load <= core1_load) ? 0 : 1;
            }
    }
}

// =============================================================================
// ADVANCED LOAD BALANCING
// =============================================================================

/**
 * @brief Load balancing metrics
 */
typedef struct {
    uint32_t task_count;           ///< Number of tasks
    uint32_t total_priority;       ///< Sum of all task priorities
    uint32_t cpu_utilization;      ///< CPU utilization percentage
    uint32_t ready_tasks;          ///< Number of ready tasks
    uint32_t high_priority_tasks;  ///< Number of high priority tasks (>= 5)
} pico_rtos_core_metrics_t;

/**
 * @brief Calculate comprehensive core metrics
 */
static void pico_rtos_smp_calculate_core_metrics(uint32_t core_id, pico_rtos_core_metrics_t *metrics) {
    if (core_id >= 2 || !metrics) {
        return;
    }

    memset(metrics, 0, sizeof(pico_rtos_core_metrics_t));

    pico_rtos_core_state_t *core = &smp_scheduler.cores[core_id];
    pico_rtos_task_t *task = core->ready_list;

    while (task) {
        metrics->task_count++;
        metrics->total_priority += task->priority;

        if (task->state == PICO_RTOS_TASK_STATE_READY) {
            metrics->ready_tasks++;
        }

        if (task->priority >= 5) {
            metrics->high_priority_tasks++;
        }

        task = task->next;
    }

    // Calculate CPU utilization based on idle time
    uint64_t total_time = core->total_runtime_us + core->idle_time_us;
    if (total_time > 0) {
        metrics->cpu_utilization = (uint32_t)((core->total_runtime_us * 100) / total_time);
    }
}

void pico_rtos_smp_set_load_balancing(bool enabled) {
    if (!smp_system_initialized) {
        return;
    }

    pico_rtos_smp_enter_critical();
    smp_scheduler.load_balancing_enabled = enabled;
    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_INFO("Load balancing %s", enabled ? "enabled" : "disabled");
}

void pico_rtos_smp_set_load_balance_threshold(uint32_t threshold_percent) {
    if (!smp_system_initialized || threshold_percent > 100) {
        return;
    }

    pico_rtos_smp_enter_critical();
    smp_scheduler.load_balance_threshold = threshold_percent;
    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_INFO("Load balance threshold set to %lu%%", threshold_percent);
}

uint32_t pico_rtos_smp_get_core_load(uint32_t core_id) {
    if (!smp_system_initialized || core_id >= 2) {
        return 0;
    }

    return pico_rtos_smp_calculate_core_load(core_id);
}

static uint32_t pico_rtos_smp_calculate_core_load(uint32_t core_id) {
    if (core_id >= 2) {
        return 0;
    }

    pico_rtos_core_metrics_t metrics;
    pico_rtos_smp_calculate_core_metrics(core_id, &metrics);

    // Weighted load calculation considering multiple factors
    uint32_t task_load = (metrics.task_count * 100) / PICO_RTOS_MAX_TASKS_PER_CORE;
    uint32_t cpu_load = metrics.cpu_utilization;
    uint32_t priority_load = 0;

    if (metrics.task_count > 0) {
        uint32_t avg_priority = metrics.total_priority / metrics.task_count;
        priority_load = (avg_priority * 10); // Scale priority impact
    }

    // Combine factors with weights
    uint32_t combined_load = (task_load * 40 + cpu_load * 40 + priority_load * 20) / 100;

    // Clamp to 100%
    if (combined_load > 100) {
        combined_load = 100;
    }

    smp_scheduler.cores[core_id].load_percentage = combined_load;
    return combined_load;
}

static bool pico_rtos_smp_should_balance_load(void) {
    uint32_t current_time = pico_rtos_get_tick_count();

    // Check if enough time has passed since last balance
    if ((current_time - last_load_balance_time) < LOAD_BALANCE_INTERVAL_MS) {
        return false;
    }

    // Calculate load difference between cores
    uint32_t core0_load = pico_rtos_smp_calculate_core_load(0);
    uint32_t core1_load = pico_rtos_smp_calculate_core_load(1);

    uint32_t load_diff = (core0_load > core1_load) ?
                        (core0_load - core1_load) : (core1_load - core0_load);

    return (load_diff > smp_scheduler.load_balance_threshold);
}

/**
 * @brief Find best task to migrate for load balancing
 */
static pico_rtos_task_t *pico_rtos_smp_find_migration_candidate(uint32_t source_core, uint32_t target_core) {
    if (source_core >= 2 || target_core >= 2) {
        return NULL;
    }

    pico_rtos_core_state_t *core = &smp_scheduler.cores[source_core];
    pico_rtos_task_t *best_candidate = NULL;
    uint32_t best_score = 0;

    pico_rtos_task_t *task = core->ready_list;
    while (task) {
        // Check if task can run on target core
        if (pico_rtos_smp_task_can_run_on_core(task, target_core)) {
            // Score based on priority (lower priority tasks are better candidates)
            // and migration history (prefer tasks that haven't been migrated recently)
            uint32_t score = (10 - task->priority) * 10; // Lower priority = higher score

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
            if (task->migration_count == 0) {
                score += 20; // Bonus for never-migrated tasks
            } else if (task->migration_count < 3) {
                score += 10; // Smaller bonus for lightly migrated tasks
            }
#endif

            if (score > best_score) {
                best_score = score;
                best_candidate = task;
            }
        }
        task = task->next;
    }

    return best_candidate;
}

uint32_t pico_rtos_smp_balance_load(void) {
    if (!smp_system_initialized || !smp_scheduler.load_balancing_enabled) {
        return 0;
    }

    PICO_RTOS_LOG_SMP_DEBUG("Starting load balancing");

    uint32_t migrations = 0;
    last_load_balance_time = pico_rtos_get_tick_count();

    // Calculate current loads
    uint32_t core0_load = pico_rtos_smp_calculate_core_load(0);
    uint32_t core1_load = pico_rtos_smp_calculate_core_load(1);

    PICO_RTOS_LOG_SMP_DEBUG("Core loads: Core0=%lu%%, Core1=%lu%%", core0_load, core1_load);

    // Determine which core is overloaded
    uint32_t overloaded_core, underloaded_core;
    if (core0_load > core1_load) {
        overloaded_core = 0;
        underloaded_core = 1;
    } else {
        overloaded_core = 1;
        underloaded_core = 0;
    }

    uint32_t load_diff = (core0_load > core1_load) ?
                        (core0_load - core1_load) : (core1_load - core0_load);

    // Only balance if difference exceeds threshold
    if (load_diff > smp_scheduler.load_balance_threshold) {
        // Find tasks to migrate
        uint32_t max_migrations = 3; // Limit migrations per balance cycle

        for (uint32_t i = 0; i < max_migrations && load_diff > smp_scheduler.load_balance_threshold; i++) {
            pico_rtos_task_t *candidate = pico_rtos_smp_find_migration_candidate(overloaded_core, underloaded_core);

            if (candidate) {
                // Create migration request
                pico_rtos_migration_request_t request = {
                    .task = candidate,
                    .source_core = overloaded_core,
                    .target_core = underloaded_core,
                    .migration_reason = PICO_RTOS_MIGRATION_LOAD_BALANCE,
                    .urgent = false
                };

                if (pico_rtos_smp_enqueue_migration_request(&request)) {
                    migrations++;
                    PICO_RTOS_LOG_SMP_DEBUG("Queued migration: task %s from core %lu to core %lu",
                                           candidate->name ? candidate->name : "unnamed",
                                           overloaded_core, underloaded_core);

                    // Recalculate loads after potential migration
                    core0_load = pico_rtos_smp_calculate_core_load(0);
                    core1_load = pico_rtos_smp_calculate_core_load(1);
                    load_diff = (core0_load > core1_load) ?
                               (core0_load - core1_load) : (core1_load - core0_load);
                } else {
                    PICO_RTOS_LOG_SMP_WARN("Failed to queue migration request");
                    break;
                }
            } else {
                PICO_RTOS_LOG_SMP_DEBUG("No suitable migration candidate found");
                break;
            }
        }
    }

    smp_scheduler.last_balance_time = last_load_balance_time;

    PICO_RTOS_LOG_SMP_DEBUG("Load balancing completed, %lu tasks migrated", migrations);
    return migrations;
}

/**
 * @brief Get detailed load balancing statistics
 */
bool pico_rtos_smp_get_load_balance_stats(uint32_t *total_migrations,
                                         uint32_t *last_balance_time,
                                         uint32_t *balance_cycles) {
    if (!smp_system_initialized) {
        return false;
    }

    pico_rtos_smp_enter_critical();

    if (total_migrations) {
        *total_migrations = smp_scheduler.migration_count;
    }

    if (last_balance_time) {
        *last_balance_time = smp_scheduler.last_balance_time;
    }

    if (balance_cycles) {
        // This would be tracked in a full implementation
        *balance_cycles = 0;
    }

    pico_rtos_smp_exit_critical();
    return true;
}

// =============================================================================
// TASK MIGRATION
// =============================================================================

static uint32_t pico_rtos_smp_find_best_core_for_task(pico_rtos_task_t *task) {
    if (!task) {
        return 0;
    }

    // Check affinity constraints first
    if (!pico_rtos_smp_task_can_run_on_core(task, 0)) {
        return 1;
    }
    if (!pico_rtos_smp_task_can_run_on_core(task, 1)) {
        return 0;
    }

    // Choose core with lower load
    uint32_t core0_load = pico_rtos_smp_calculate_core_load(0);
    uint32_t core1_load = pico_rtos_smp_calculate_core_load(1);

    return (core0_load <= core1_load) ? 0 : 1;
}

bool pico_rtos_smp_migrate_task(pico_rtos_task_t *task, uint32_t target_core,
                               pico_rtos_migration_reason_t reason) {
    if (!smp_system_initialized || target_core >= 2) {
        return false;
    }

    if (task == NULL) {
        task = pico_rtos_get_current_task();
    }

    if (task == NULL) {
        return false;
    }

    // Check if task can run on target core
    if (!pico_rtos_smp_task_can_run_on_core(task, target_core)) {
        PICO_RTOS_LOG_SMP_WARN("Task %s cannot run on core %lu due to affinity constraints",
                              task->name ? task->name : "unnamed", target_core);
        return false;
    }

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    uint32_t source_core = task->assigned_core;

    if (source_core == target_core) {
        PICO_RTOS_LOG_SMP_DEBUG("Task %s already on target core %lu",
                               task->name ? task->name : "unnamed", target_core);
        return true;
    }

    pico_rtos_migration_request_t request = {
        .task = task,
        .source_core = source_core,
        .target_core = target_core,
        .migration_reason = reason,
        .urgent = false
    };

    bool success = pico_rtos_smp_enqueue_migration_request(&request);
    if (success) {
        PICO_RTOS_LOG_SMP_DEBUG("Migration request queued for task %s: core %lu -> %lu",
                               task->name ? task->name : "unnamed", source_core, target_core);
    }

    return success;
#else
    return false;
#endif
}

bool pico_rtos_smp_get_migration_stats(pico_rtos_task_t *task,
                                      uint32_t *migration_count,
                                      uint32_t *last_migration_time) {
    if (!smp_system_initialized) {
        return false;
    }

    if (task == NULL) {
        task = pico_rtos_get_current_task();
    }

    if (task == NULL) {
        return false;
    }

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    if (migration_count) {
        *migration_count = task->migration_count;
    }
    if (last_migration_time) {
        *last_migration_time = 0; // Would store actual timestamp
    }
    return true;
#else
    return false;
#endif
}

// =============================================================================
// CORE HEALTH MONITORING AND FAILURE DETECTION
// =============================================================================

bool pico_rtos_core_health_init(const pico_rtos_core_health_config_t *config) {
    if (!smp_system_initialized) {
        PICO_RTOS_LOG_SMP_ERROR("SMP system not initialized");
        return false;
    }

    PICO_RTOS_LOG_SMP_INFO("Initializing core health monitoring system");

    pico_rtos_smp_enter_critical();

    // Use provided config or default
    if (config) {
        smp_scheduler.health_config = *config;
    } else {
        smp_scheduler.health_config = DEFAULT_HEALTH_CONFIG;
    }

    // Initialize health state for both cores
    pico_rtos_core_health_init_state(0);
    pico_rtos_core_health_init_state(1);

    // Reset global health monitoring state
    last_health_check_time = 0;
    health_check_counter = 0;
    smp_scheduler.single_core_mode = false;
    smp_scheduler.healthy_core_id = 0;
    smp_scheduler.failure_callback = NULL;
    smp_scheduler.failure_callback_data = NULL;

    health_monitoring_initialized = true;

    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_INFO("Core health monitoring initialized (watchdog: %lums, interval: %lums)",
                          smp_scheduler.health_config.watchdog_timeout_ms,
                          smp_scheduler.health_config.health_check_interval_ms);

    return true;
}

static void pico_rtos_core_health_init_state(uint32_t core_id) {
    if (core_id >= 2) {
        return;
    }

    pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;

    health->status = PICO_RTOS_CORE_HEALTH_HEALTHY;
    health->failure_type = PICO_RTOS_CORE_FAILURE_NONE;
    health->last_heartbeat_time = pico_rtos_get_tick_count();
    health->missed_heartbeats = 0;
    health->failure_count = 0;
    health->recovery_attempts = 0;
    health->last_failure_time = 0;
    health->total_downtime_ms = 0;
    health->watchdog_fed = false;
    health->watchdog_feed_count = 0;

    PICO_RTOS_LOG_SMP_DEBUG("Health state initialized for core %lu", core_id);
}

void pico_rtos_core_health_start(void) {
    if (!health_monitoring_initialized) {
        PICO_RTOS_LOG_SMP_ERROR("Health monitoring not initialized");
        return;
    }

    if (!smp_scheduler.health_config.enabled) {
        PICO_RTOS_LOG_SMP_INFO("Health monitoring disabled by configuration");
        return;
    }

    PICO_RTOS_LOG_SMP_INFO("Starting core health monitoring");

    pico_rtos_smp_enter_critical();

    // Reset health states and start monitoring
    for (int i = 0; i < 2; i++) {
        pico_rtos_core_health_state_t *health = &smp_scheduler.cores[i].health;
        health->status = PICO_RTOS_CORE_HEALTH_HEALTHY;
        health->last_heartbeat_time = pico_rtos_get_tick_count();
        health->watchdog_fed = true; // Start with watchdog fed
    }

    last_health_check_time = pico_rtos_get_tick_count();

    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_INFO("Core health monitoring started");
}

void pico_rtos_core_health_stop(void) {
    if (!health_monitoring_initialized) {
        return;
    }

    PICO_RTOS_LOG_SMP_INFO("Stopping core health monitoring");

    pico_rtos_smp_enter_critical();

    // Mark all cores as unknown health status
    for (int i = 0; i < 2; i++) {
        smp_scheduler.cores[i].health.status = PICO_RTOS_CORE_HEALTH_UNKNOWN;
    }

    pico_rtos_smp_exit_critical();
}

void pico_rtos_core_health_feed_watchdog(void) {
    if (!health_monitoring_initialized || !smp_scheduler.health_config.enabled) {
        return;
    }

    uint32_t core_id = pico_rtos_smp_get_core_id();
    if (core_id >= 2) {
        return;
    }

    pico_rtos_smp_enter_critical();

    pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;
    health->watchdog_fed = true;
    health->watchdog_feed_count++;

    pico_rtos_smp_exit_critical();
}

void pico_rtos_core_health_send_heartbeat(void) {
    if (!health_monitoring_initialized || !smp_scheduler.health_config.enabled) {
        return;
    }

    uint32_t core_id = pico_rtos_smp_get_core_id();
    if (core_id >= 2) {
        return;
    }

    pico_rtos_smp_enter_critical();

    pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;
    health->last_heartbeat_time = pico_rtos_get_tick_count();

    // Reset missed heartbeats if we were previously unresponsive
    if (health->status == PICO_RTOS_CORE_HEALTH_UNRESPONSIVE) {
        health->missed_heartbeats = 0;
        health->status = PICO_RTOS_CORE_HEALTH_HEALTHY;
        PICO_RTOS_LOG_SMP_INFO("Core %lu recovered from unresponsive state", core_id);
    }

    pico_rtos_smp_exit_critical();
}

pico_rtos_core_health_status_t pico_rtos_core_health_check(uint32_t core_id) {
    if (!health_monitoring_initialized || core_id >= 2) {
        return PICO_RTOS_CORE_HEALTH_UNKNOWN;
    }

    pico_rtos_core_health_status_t status;

    pico_rtos_smp_enter_critical();
    status = smp_scheduler.cores[core_id].health.status;
    pico_rtos_smp_exit_critical();

    return status;
}

bool pico_rtos_core_health_get_state(uint32_t core_id, pico_rtos_core_health_state_t *state) {
    if (!health_monitoring_initialized || core_id >= 2 || !state) {
        return false;
    }

    pico_rtos_smp_enter_critical();
    *state = smp_scheduler.cores[core_id].health;
    pico_rtos_smp_exit_critical();

    return true;
}

bool pico_rtos_core_health_register_failure_callback(pico_rtos_core_failure_callback_t callback,
                                                     void *user_data) {
    if (!health_monitoring_initialized) {
        return false;
    }

    pico_rtos_smp_enter_critical();
    smp_scheduler.failure_callback = callback;
    smp_scheduler.failure_callback_data = user_data;
    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_INFO("Core failure callback registered");
    return true;
}

static bool pico_rtos_core_health_check_watchdog(uint32_t core_id) {
    if (core_id >= 2) {
        return false;
    }

    pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;
    uint32_t current_time = pico_rtos_get_tick_count();

    // Check if watchdog was fed recently
    if (!health->watchdog_fed) {
        uint32_t time_since_last_feed = current_time - health->last_heartbeat_time;
        if (time_since_last_feed > smp_scheduler.health_config.watchdog_timeout_ms) {
            PICO_RTOS_LOG_SMP_WARN("Watchdog timeout detected for core %lu (timeout: %lums)",
                                  core_id, time_since_last_feed);
            return false;
        }
    }

    // Reset watchdog fed flag for next cycle
    health->watchdog_fed = false;

    return true;
}

static bool pico_rtos_core_health_check_heartbeat(uint32_t core_id) {
    if (core_id >= 2) {
        return false;
    }

    pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;
    uint32_t current_time = pico_rtos_get_tick_count();
    uint32_t time_since_heartbeat = current_time - health->last_heartbeat_time;

    uint32_t heartbeat_timeout = smp_scheduler.health_config.health_check_interval_ms *
                                smp_scheduler.health_config.max_missed_heartbeats;

    if (time_since_heartbeat > heartbeat_timeout) {
        health->missed_heartbeats++;
        PICO_RTOS_LOG_SMP_WARN("Missed heartbeat from core %lu (missed: %lu, timeout: %lums)",
                              core_id, health->missed_heartbeats, time_since_heartbeat);
        return false;
    }

    return true;
}

static void pico_rtos_core_health_handle_failure(uint32_t core_id, pico_rtos_core_failure_type_t failure_type) {
    if (core_id >= 2) {
        return;
    }

    pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;

    PICO_RTOS_LOG_SMP_ERROR("Core failure detected: core %lu, type %d", core_id, failure_type);

    // Update health state
    health->status = PICO_RTOS_CORE_HEALTH_FAILED;
    health->failure_type = failure_type;
    health->failure_count++;
    health->last_failure_time = pico_rtos_get_tick_count();

    // Call user failure callback if registered
    bool attempt_recovery = true;
    if (smp_scheduler.failure_callback) {
        attempt_recovery = smp_scheduler.failure_callback(core_id, failure_type,
                                                         smp_scheduler.failure_callback_data);
    }

    // Attempt automatic recovery if enabled and requested
    if (attempt_recovery && smp_scheduler.health_config.auto_recovery_enabled) {
        if (!pico_rtos_core_health_attempt_recovery(core_id)) {
            PICO_RTOS_LOG_SMP_ERROR("Core %lu recovery failed", core_id);

            // If recovery fails and graceful degradation is enabled, switch to single-core mode
            if (smp_scheduler.health_config.graceful_degradation_enabled) {
                pico_rtos_core_health_degrade_to_single_core(core_id);
            }
        }
    } else {
        PICO_RTOS_LOG_SMP_INFO("Core %lu recovery skipped (disabled or callback returned false)", core_id);

        // Immediate graceful degradation if recovery is not attempted
        if (smp_scheduler.health_config.graceful_degradation_enabled) {
            pico_rtos_core_health_degrade_to_single_core(core_id);
        }
    }
}

static bool pico_rtos_core_health_attempt_recovery(uint32_t core_id) {
    if (core_id >= 2) {
        return false;
    }

    pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;

    PICO_RTOS_LOG_SMP_INFO("Attempting recovery for core %lu (attempt %lu)",
                          core_id, health->recovery_attempts + 1);

    health->status = PICO_RTOS_CORE_HEALTH_RECOVERING;
    health->recovery_attempts++;

    uint32_t recovery_start_time = pico_rtos_get_tick_count();

    // For RP2040, we can't actually restart a core, so we simulate recovery
    // by resetting the core's health state and hoping it recovers naturally

    // Reset health indicators
    health->missed_heartbeats = 0;
    health->watchdog_fed = true;
    health->last_heartbeat_time = pico_rtos_get_tick_count();

    // Wait for recovery timeout to see if core responds
    uint32_t timeout = smp_scheduler.health_config.recovery_timeout_ms;
    uint32_t check_interval = 100; // Check every 100ms

    for (uint32_t elapsed = 0; elapsed < timeout; elapsed += check_interval) {
        sleep_ms(check_interval);

        // Check if core is responding
        if (pico_rtos_core_health_check_heartbeat(core_id) &&
            pico_rtos_core_health_check_watchdog(core_id)) {

            health->status = PICO_RTOS_CORE_HEALTH_HEALTHY;
            health->failure_type = PICO_RTOS_CORE_FAILURE_NONE;

            uint32_t recovery_time = pico_rtos_get_tick_count() - recovery_start_time;
            health->total_downtime_ms += recovery_time;

            PICO_RTOS_LOG_SMP_INFO("Core %lu recovered successfully (recovery time: %lums)",
                                  core_id, recovery_time);
            return true;
        }
    }

    // Recovery failed
    health->status = PICO_RTOS_CORE_HEALTH_FAILED;
    health->total_downtime_ms += timeout;

    PICO_RTOS_LOG_SMP_ERROR("Core %lu recovery failed after %lums", core_id, timeout);
    return false;
}

bool pico_rtos_core_health_recover_core(uint32_t core_id) {
    if (!health_monitoring_initialized || core_id >= 2) {
        return false;
    }

    pico_rtos_smp_enter_critical();
    bool result = pico_rtos_core_health_attempt_recovery(core_id);
    pico_rtos_smp_exit_critical();

    return result;
}

uint32_t pico_rtos_core_health_degrade_to_single_core(uint32_t failed_core_id) {
    if (!health_monitoring_initialized || failed_core_id >= 2) {
        return 0;
    }

    uint32_t healthy_core_id = (failed_core_id == 0) ? 1 : 0;

    PICO_RTOS_LOG_SMP_WARN("Degrading to single-core operation: failed core %lu, healthy core %lu",
                          failed_core_id, healthy_core_id);

    pico_rtos_smp_enter_critical();

    // Migrate all tasks from failed core to healthy core
    uint32_t migrated_tasks = pico_rtos_core_health_migrate_all_tasks(failed_core_id, healthy_core_id);

    // Update system state
    smp_scheduler.single_core_mode = true;
    smp_scheduler.healthy_core_id = healthy_core_id;

    // Mark failed core as inactive
    smp_scheduler.cores[failed_core_id].scheduler_active = false;

    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_WARN("Single-core degradation complete: %lu tasks migrated to core %lu",
                          migrated_tasks, healthy_core_id);

    return migrated_tasks;
}

static uint32_t pico_rtos_core_health_migrate_all_tasks(uint32_t failed_core_id, uint32_t target_core_id) {
    if (failed_core_id >= 2 || target_core_id >= 2) {
        return 0;
    }

    uint32_t migrated_count = 0;
    pico_rtos_core_state_t *failed_core = &smp_scheduler.cores[failed_core_id];
    pico_rtos_task_t *task = failed_core->ready_list;

    while (task) {
        pico_rtos_task_t *next_task = task->next;

        // Skip idle task (it's core-specific)
        if (task != &idle_tasks[failed_core_id]) {
            // Create urgent migration request
            pico_rtos_migration_request_t request = {
                .task = task,
                .source_core = failed_core_id,
                .target_core = target_core_id,
                .migration_reason = PICO_RTOS_MIGRATION_CORE_FAILURE,
                .urgent = true
            };

            if (pico_rtos_smp_enqueue_migration_request(&request)) {
                migrated_count++;
                PICO_RTOS_LOG_SMP_DEBUG("Queued emergency migration: task %s from core %lu to core %lu",
                                       task->name ? task->name : "unnamed",
                                       failed_core_id, target_core_id);
            }
        }

        task = next_task;
    }

    // Process all migration requests immediately
    pico_rtos_smp_process_migration_requests();

    return migrated_count;
}

bool pico_rtos_core_health_is_single_core_mode(void) {
    if (!health_monitoring_initialized) {
        return false;
    }

    bool single_core;
    pico_rtos_smp_enter_critical();
    single_core = smp_scheduler.single_core_mode;
    pico_rtos_smp_exit_critical();

    return single_core;
}

bool pico_rtos_core_health_get_failure_stats(uint32_t core_id,
                                             uint32_t *failure_count,
                                             uint32_t *recovery_count,
                                             uint32_t *total_downtime_ms) {
    if (!health_monitoring_initialized || core_id >= 2) {
        return false;
    }

    pico_rtos_smp_enter_critical();

    pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;

    if (failure_count) {
        *failure_count = health->failure_count;
    }

    if (recovery_count) {
        *recovery_count = health->recovery_attempts;
    }

    if (total_downtime_ms) {
        *total_downtime_ms = health->total_downtime_ms;
    }

    pico_rtos_smp_exit_critical();

    return true;
}

void pico_rtos_core_health_reset_stats(uint32_t core_id) {
    if (!health_monitoring_initialized) {
        return;
    }

    pico_rtos_smp_enter_critical();

    if (core_id == 0xFF) {
        // Reset stats for both cores
        for (int i = 0; i < 2; i++) {
            pico_rtos_core_health_state_t *health = &smp_scheduler.cores[i].health;
            health->failure_count = 0;
            health->recovery_attempts = 0;
            health->total_downtime_ms = 0;
            health->watchdog_feed_count = 0;
        }
        PICO_RTOS_LOG_SMP_INFO("Health statistics reset for both cores");
    } else if (core_id < 2) {
        // Reset stats for specific core
        pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;
        health->failure_count = 0;
        health->recovery_attempts = 0;
        health->total_downtime_ms = 0;
        health->watchdog_feed_count = 0;
        PICO_RTOS_LOG_SMP_INFO("Health statistics reset for core %lu", core_id);
    }

    pico_rtos_smp_exit_critical();
}

void pico_rtos_core_health_process(void) {
    if (!health_monitoring_initialized || !smp_scheduler.health_config.enabled) {
        return;
    }

    uint32_t current_time = pico_rtos_get_tick_count();

    // Check if it's time for health monitoring
    if ((current_time - last_health_check_time) < smp_scheduler.health_config.health_check_interval_ms) {
        return;
    }

    last_health_check_time = current_time;
    health_check_counter++;

    // Check health of both cores
    for (uint32_t core_id = 0; core_id < 2; core_id++) {
        // Skip checking failed cores that are in single-core mode
        if (smp_scheduler.single_core_mode && core_id != smp_scheduler.healthy_core_id) {
            continue;
        }

        pico_rtos_core_health_state_t *health = &smp_scheduler.cores[core_id].health;

        // Skip cores that are already failed or recovering
        if (health->status == PICO_RTOS_CORE_HEALTH_FAILED ||
            health->status == PICO_RTOS_CORE_HEALTH_RECOVERING) {
            continue;
        }

        bool watchdog_ok = pico_rtos_core_health_check_watchdog(core_id);
        bool heartbeat_ok = pico_rtos_core_health_check_heartbeat(core_id);

        if (!watchdog_ok) {
            pico_rtos_core_health_handle_failure(core_id, PICO_RTOS_CORE_FAILURE_WATCHDOG);
        } else if (!heartbeat_ok) {
            if (health->missed_heartbeats >= smp_scheduler.health_config.max_missed_heartbeats) {
                pico_rtos_core_health_handle_failure(core_id, PICO_RTOS_CORE_FAILURE_HANG);
            } else {
                // Mark as unresponsive but not failed yet
                health->status = PICO_RTOS_CORE_HEALTH_UNRESPONSIVE;
            }
        } else {
            // Core is healthy
            if (health->status != PICO_RTOS_CORE_HEALTH_HEALTHY) {
                health->status = PICO_RTOS_CORE_HEALTH_HEALTHY;
                health->missed_heartbeats = 0;
            }
        }
    }

    // Log health status periodically (every 10 checks)
    if (health_check_counter % 10 == 0) {
        PICO_RTOS_LOG_SMP_DEBUG("Health check #%lu: Core0=%d, Core1=%d",
                               health_check_counter,
                               smp_scheduler.cores[0].health.status,
                               smp_scheduler.cores[1].health.status);
    }
}

// =============================================================================
// MIGRATION QUEUE MANAGEMENT
// =============================================================================

static bool pico_rtos_smp_enqueue_migration_request(pico_rtos_migration_request_t *request) {
    if (!request) {
        return false;
    }

    pico_rtos_critical_section_enter_blocking(&migration_queue_cs);

    uint32_t next_head = (migration_queue_head + 1) % PICO_RTOS_MAX_MIGRATION_REQUESTS;

    if (next_head == migration_queue_tail) {
        // Queue is full
        pico_rtos_critical_section_exit(&migration_queue_cs);
        PICO_RTOS_LOG_SMP_WARN("Migration request queue is full");
        return false;
    }

    migration_requests[migration_queue_head] = *request;
    migration_queue_head = next_head;

    pico_rtos_critical_section_exit(&migration_queue_cs);
    return true;
}

static bool pico_rtos_smp_dequeue_migration_request(pico_rtos_migration_request_t *request) {
    if (!request) {
        return false;
    }

    pico_rtos_critical_section_enter_blocking(&migration_queue_cs);

    if (migration_queue_head == migration_queue_tail) {
        // Queue is empty
        pico_rtos_critical_section_exit(&migration_queue_cs);
        return false;
    }

    *request = migration_requests[migration_queue_tail];
    migration_queue_tail = (migration_queue_tail + 1) % PICO_RTOS_MAX_MIGRATION_REQUESTS;

    pico_rtos_critical_section_exit(&migration_queue_cs);
    return true;
}

static void pico_rtos_smp_process_migration_requests(void) {
    pico_rtos_migration_request_t request;

    while (pico_rtos_smp_dequeue_migration_request(&request)) {
        // Process the migration request
        // In a full implementation, this would actually move the task
        PICO_RTOS_LOG_SMP_DEBUG("Processing migration request for task %s: core %lu -> %lu",
                               request.task->name ? request.task->name : "unnamed",
                               request.source_core, request.target_core);

        // Update migration statistics
        smp_scheduler.migration_count++;

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
        if (request.task) {
            request.task->migration_count++;
            request.task->assigned_core = request.target_core;
        }
#endif
    }
}

// =============================================================================
// INTER-CORE SYNCHRONIZATION
// =============================================================================

void pico_rtos_smp_enter_critical(void) {
    if (smp_system_initialized) {
        pico_rtos_critical_section_enter_blocking(&smp_scheduler.smp_cs);
    }
}

void pico_rtos_smp_exit_critical(void) {
    if (smp_system_initialized) {
        pico_rtos_critical_section_exit(&smp_scheduler.smp_cs);
    }
}

void pico_rtos_smp_wake_core(uint32_t target_core) {
    if (!smp_system_initialized || target_core >= 2) {
        return;
    }

    // Use RP2040 inter-core FIFO to wake up the other core
    if (target_core != pico_rtos_smp_get_core_id()) {
#if PICO_ON_DEVICE
        multicore_fifo_push_blocking(PICO_RTOS_IPC_MSG_WAKEUP | (target_core << 16));
#endif
    }
}

// =============================================================================
// ADVANCED INTER-CORE CRITICAL SECTIONS
// =============================================================================

bool pico_rtos_inter_core_cs_init(pico_rtos_inter_core_cs_t *ics) {
    if (!ics) {
        return false;
    }

    pico_rtos_critical_section_init(&ics->hw_cs);
    ics->owner_core = 0xFFFFFFFF; // No owner
    ics->nesting_count = 0;
    ics->lock_count = 0;
    ics->contention_count = 0;

    PICO_RTOS_LOG_SMP_DEBUG("Inter-core critical section initialized");
    return true;
}

bool pico_rtos_inter_core_cs_enter(pico_rtos_inter_core_cs_t *ics, uint32_t timeout_ms) {
    if (!ics || !smp_system_initialized) {
        return false;
    }

    uint32_t current_core = pico_rtos_smp_get_core_id();
    uint32_t start_time = pico_rtos_get_tick_count();

    // Check if this core already owns the critical section (recursive entry)
    if (ics->owner_core == current_core) {
        ics->nesting_count++;
        ics->lock_count++;
        return true;
    }

    // Try to acquire the critical section
    while (true) {
        // Use hardware critical section for atomic access
        pico_rtos_critical_section_enter_blocking(&ics->hw_cs);

        if (ics->owner_core == 0xFFFFFFFF) {
            // Critical section is free, acquire it
            ics->owner_core = current_core;
            ics->nesting_count = 1;
            ics->lock_count++;
            pico_rtos_critical_section_exit(&ics->hw_cs);

            PICO_RTOS_LOG_SMP_DEBUG("Core %lu acquired inter-core critical section", current_core);
            return true;
        }

        // Critical section is owned by another core
        ics->contention_count++;
        pico_rtos_critical_section_exit(&ics->hw_cs);

        // Check timeout
        if (timeout_ms > 0) {
            uint32_t elapsed = pico_rtos_get_tick_count() - start_time;
            if (elapsed >= timeout_ms) {
                PICO_RTOS_LOG_SMP_WARN("Core %lu timed out waiting for inter-core critical section", current_core);
                return false;
            }
        }

        // Brief delay before retrying to reduce contention
#if PICO_ON_DEVICE
        sleep_us(10);
#endif
    }
}

void pico_rtos_inter_core_cs_exit(pico_rtos_inter_core_cs_t *ics) {
    if (!ics || !smp_system_initialized) {
        return;
    }

    uint32_t current_core = pico_rtos_smp_get_core_id();

    pico_rtos_critical_section_enter_blocking(&ics->hw_cs);

    if (ics->owner_core != current_core) {
        pico_rtos_critical_section_exit(&ics->hw_cs);
        PICO_RTOS_LOG_SMP_ERROR("Core %lu attempted to exit critical section owned by core %lu",
                               current_core, ics->owner_core);
        return;
    }

    ics->nesting_count--;
    if (ics->nesting_count == 0) {
        // Release the critical section
        ics->owner_core = 0xFFFFFFFF;
        PICO_RTOS_LOG_SMP_DEBUG("Core %lu released inter-core critical section", current_core);
    }

    pico_rtos_critical_section_exit(&ics->hw_cs);
}

bool pico_rtos_inter_core_cs_try_enter(pico_rtos_inter_core_cs_t *ics) {
    if (!ics || !smp_system_initialized) {
        return false;
    }

    uint32_t current_core = pico_rtos_smp_get_core_id();

    // Check if this core already owns the critical section
    if (ics->owner_core == current_core) {
        ics->nesting_count++;
        ics->lock_count++;
        return true;
    }

    pico_rtos_critical_section_enter_blocking(&ics->hw_cs);

    if (ics->owner_core == 0xFFFFFFFF) {
        // Critical section is free, acquire it
        ics->owner_core = current_core;
        ics->nesting_count = 1;
        ics->lock_count++;
        pico_rtos_critical_section_exit(&ics->hw_cs);
        return true;
    }

    // Critical section is busy
    pico_rtos_critical_section_exit(&ics->hw_cs);
    return false;
}

bool pico_rtos_inter_core_cs_get_stats(pico_rtos_inter_core_cs_t *ics,
                                       uint32_t *lock_count,
                                       uint32_t *contention_count) {
    if (!ics) {
        return false;
    }

    pico_rtos_critical_section_enter_blocking(&ics->hw_cs);

    if (lock_count) {
        *lock_count = ics->lock_count;
    }
    if (contention_count) {
        *contention_count = ics->contention_count;
    }

    pico_rtos_critical_section_exit(&ics->hw_cs);
    return true;
}

// =============================================================================
// INTER-CORE COMMUNICATION USING HARDWARE FIFOS
// =============================================================================

// Global IPC channels for each core
static pico_rtos_ipc_channel_t ipc_channels[2];
static pico_rtos_ipc_message_t ipc_buffers[2][32]; // 32 messages per core
static bool ipc_system_initialized = false;

bool pico_rtos_ipc_channel_init(pico_rtos_ipc_channel_t *channel,
                               pico_rtos_ipc_message_t *buffer,
                               uint32_t buffer_size) {
    if (!channel || !buffer || buffer_size == 0) {
        return false;
    }

    channel->read_pos = 0;
    channel->write_pos = 0;
    channel->buffer = buffer;
    channel->buffer_size = buffer_size;
    channel->dropped_messages = 0;
    channel->total_messages = 0;

    pico_rtos_critical_section_init(&channel->access_cs);

    PICO_RTOS_LOG_SMP_DEBUG("IPC channel initialized with buffer size %lu", buffer_size);
    return true;
}

static bool pico_rtos_ipc_init_system(void) {
    if (ipc_system_initialized) {
        return true;
    }

    // Initialize IPC channels for both cores
    for (int i = 0; i < 2; i++) {
        if (!pico_rtos_ipc_channel_init(&ipc_channels[i], ipc_buffers[i], 32)) {
            PICO_RTOS_LOG_SMP_ERROR("Failed to initialize IPC channel for core %d", i);
            return false;
        }
    }

    ipc_system_initialized = true;
    PICO_RTOS_LOG_SMP_INFO("IPC system initialized");
    return true;
}

bool pico_rtos_ipc_send_message(uint32_t target_core,
                               const pico_rtos_ipc_message_t *message,
                               uint32_t timeout_ms) {
    if (!smp_system_initialized || target_core >= 2 || !message) {
        return false;
    }

    if (!ipc_system_initialized && !pico_rtos_ipc_init_system()) {
        return false;
    }

    uint32_t current_core = pico_rtos_smp_get_core_id();
    if (target_core == current_core) {
        PICO_RTOS_LOG_SMP_WARN("Attempted to send IPC message to same core");
        return false;
    }

    pico_rtos_ipc_channel_t *channel = &ipc_channels[target_core];
    uint32_t start_time = pico_rtos_get_tick_count();

    while (true) {
        pico_rtos_critical_section_enter_blocking(&channel->access_cs);

        uint32_t next_write = (channel->write_pos + 1) % channel->buffer_size;

        if (next_write != channel->read_pos) {
            // Buffer has space, add message
            pico_rtos_ipc_message_t *msg = &channel->buffer[channel->write_pos];
            *msg = *message;
            msg->source_core = current_core;
            msg->timestamp = pico_rtos_get_tick_count();

            channel->write_pos = next_write;
            channel->total_messages++;

            pico_rtos_critical_section_exit(&channel->access_cs);

            // Use hardware FIFO to notify target core
#if PICO_ON_DEVICE
            multicore_fifo_push_blocking(message->type | (current_core << 16));
#endif

            PICO_RTOS_LOG_SMP_DEBUG("IPC message sent from core %lu to core %lu (type 0x%lx)",
                                   current_core, target_core, message->type);
            return true;
        }

        // Buffer is full
        channel->dropped_messages++;
        pico_rtos_critical_section_exit(&channel->access_cs);

        // Check timeout
        if (timeout_ms > 0) {
            uint32_t elapsed = pico_rtos_get_tick_count() - start_time;
            if (elapsed >= timeout_ms) {
                PICO_RTOS_LOG_SMP_WARN("IPC send timeout from core %lu to core %lu", current_core, target_core);
                return false;
            }
        }

        // Brief delay before retrying
#if PICO_ON_DEVICE
        sleep_us(100);
#endif
    }
}

bool pico_rtos_ipc_receive_message(pico_rtos_ipc_message_t *message,
                                  uint32_t timeout_ms) {
    if (!smp_system_initialized || !message) {
        return false;
    }

    if (!ipc_system_initialized && !pico_rtos_ipc_init_system()) {
        return false;
    }

    uint32_t current_core = pico_rtos_smp_get_core_id();
    pico_rtos_ipc_channel_t *channel = &ipc_channels[current_core];
    uint32_t start_time = pico_rtos_get_tick_count();

    while (true) {
        pico_rtos_critical_section_enter_blocking(&channel->access_cs);

        if (channel->read_pos != channel->write_pos) {
            // Message available
            *message = channel->buffer[channel->read_pos];
            channel->read_pos = (channel->read_pos + 1) % channel->buffer_size;

            pico_rtos_critical_section_exit(&channel->access_cs);

            PICO_RTOS_LOG_SMP_DEBUG("IPC message received on core %lu from core %lu (type 0x%lx)",
                                   current_core, message->source_core, message->type);
            return true;
        }

        pico_rtos_critical_section_exit(&channel->access_cs);

        // Check timeout
        if (timeout_ms > 0) {
            uint32_t elapsed = pico_rtos_get_tick_count() - start_time;
            if (elapsed >= timeout_ms) {
                return false;
            }
        }

        // Check hardware FIFO for notifications
#if PICO_ON_DEVICE
        if (multicore_fifo_rvalid()) {
            uint32_t fifo_data = multicore_fifo_pop_blocking();
            // FIFO data contains message type and source core
            // The actual message is in our channel buffer
            continue; // Try reading from buffer again
        }
#endif

        // Brief delay before retrying
#if PICO_ON_DEVICE
        sleep_us(100);
#endif
    }
}

uint32_t pico_rtos_ipc_pending_messages(void) {
    if (!smp_system_initialized || !ipc_system_initialized) {
        return 0;
    }

    uint32_t current_core = pico_rtos_smp_get_core_id();
    pico_rtos_ipc_channel_t *channel = &ipc_channels[current_core];

    pico_rtos_critical_section_enter_blocking(&channel->access_cs);

    uint32_t pending;
    if (channel->write_pos >= channel->read_pos) {
        pending = channel->write_pos - channel->read_pos;
    } else {
        pending = channel->buffer_size - channel->read_pos + channel->write_pos;
    }

    pico_rtos_critical_section_exit(&channel->access_cs);

    return pending;
}

bool pico_rtos_ipc_send_notification(uint32_t target_core, uint32_t notification_id) {
    pico_rtos_ipc_message_t msg = {
        .type = PICO_RTOS_IPC_MSG_USER_DEFINED,
        .source_core = pico_rtos_smp_get_core_id(),
        .data1 = notification_id,
        .data2 = 0,
        .timestamp = 0
    };

    return pico_rtos_ipc_send_message(target_core, &msg, 100);
}

void pico_rtos_ipc_process_messages(void) {
    if (!smp_system_initialized || !ipc_system_initialized) {
        return;
    }

    pico_rtos_ipc_message_t message;

    // Process up to 10 messages per call to avoid blocking too long
    for (int i = 0; i < 10; i++) {
        if (!pico_rtos_ipc_receive_message(&message, 0)) {
            break; // No more messages
        }

        // Handle system messages
        switch (message.type) {
            case PICO_RTOS_IPC_MSG_WAKEUP:
                // Scheduler wake-up - nothing special needed
                break;

            case PICO_RTOS_IPC_MSG_TASK_READY:
                // Task became ready on another core
                pico_rtos_smp_wake_core(pico_rtos_smp_get_core_id());
                break;

            case PICO_RTOS_IPC_MSG_MIGRATION_REQ:
                // Task migration request - would be handled by migration system
                break;

            case PICO_RTOS_IPC_MSG_SYNC_BARRIER:
                // Synchronization barrier notification
                break;

            default:
                // User-defined message - application should handle
                PICO_RTOS_LOG_SMP_DEBUG("User IPC message received: type 0x%lx, data1=0x%lx, data2=0x%lx",
                                       message.type, message.data1, message.data2);
                break;
        }
    }
}

bool pico_rtos_ipc_get_channel_stats(pico_rtos_ipc_channel_t *channel,
                                    uint32_t *total_messages,
                                    uint32_t *dropped_messages) {
    if (!channel) {
        return false;
    }

    pico_rtos_critical_section_enter_blocking(&channel->access_cs);

    if (total_messages) {
        *total_messages = channel->total_messages;
    }
    if (dropped_messages) {
        *dropped_messages = channel->dropped_messages;
    }

    pico_rtos_critical_section_exit(&channel->access_cs);
    return true;
}

// =============================================================================
// SYNCHRONIZATION BARRIERS
// =============================================================================

bool pico_rtos_sync_barrier_init(pico_rtos_sync_barrier_t *barrier,
                                uint32_t required_cores,
                                uint32_t barrier_id) {
    if (!barrier || required_cores == 0 || required_cores > 3) {
        return false;
    }

    barrier->core_flags = 0;
    barrier->required_cores = required_cores;
    barrier->barrier_id = barrier_id;
    barrier->wait_count = 0;

    pico_rtos_critical_section_init(&barrier->cs);

    PICO_RTOS_LOG_SMP_DEBUG("Sync barrier %lu initialized (required cores: 0x%lx)",
                           barrier_id, required_cores);
    return true;
}

bool pico_rtos_sync_barrier_wait(pico_rtos_sync_barrier_t *barrier,
                                uint32_t timeout_ms) {
    if (!barrier || !smp_system_initialized) {
        return false;
    }

    uint32_t current_core = pico_rtos_smp_get_core_id();
    uint32_t core_bit = (1 << current_core);
    uint32_t start_time = pico_rtos_get_tick_count();

    PICO_RTOS_LOG_SMP_DEBUG("Core %lu waiting at barrier %lu", current_core, barrier->barrier_id);

    pico_rtos_critical_section_enter_blocking(&barrier->cs);

    // Mark this core as having reached the barrier
    barrier->core_flags |= core_bit;
    barrier->wait_count++;

    // Check if all required cores have reached the barrier
    if ((barrier->core_flags & barrier->required_cores) == barrier->required_cores) {
        // All cores have arrived, release the barrier
        barrier->core_flags = 0; // Reset for next use
        pico_rtos_critical_section_exit(&barrier->cs);

        // Notify other cores that barrier is released
        pico_rtos_ipc_message_t msg = {
            .type = PICO_RTOS_IPC_MSG_SYNC_BARRIER,
            .data1 = barrier->barrier_id,
            .data2 = 1 // Released
        };

        // Send to all other cores
        for (uint32_t core = 0; core < 2; core++) {
            if (core != current_core && (barrier->required_cores & (1 << core))) {
                pico_rtos_ipc_send_message(core, &msg, 0);
            }
        }

        PICO_RTOS_LOG_SMP_DEBUG("Core %lu released barrier %lu", current_core, barrier->barrier_id);
        return true;
    }

    pico_rtos_critical_section_exit(&barrier->cs);

    // Wait for barrier to be released
    while (true) {
        pico_rtos_critical_section_enter_blocking(&barrier->cs);

        // Check if barrier was reset (indicating it was released)
        if ((barrier->core_flags & core_bit) == 0) {
            pico_rtos_critical_section_exit(&barrier->cs);
            PICO_RTOS_LOG_SMP_DEBUG("Core %lu passed barrier %lu", current_core, barrier->barrier_id);
            return true;
        }

        pico_rtos_critical_section_exit(&barrier->cs);

        // Check timeout
        if (timeout_ms > 0) {
            uint32_t elapsed = pico_rtos_get_tick_count() - start_time;
            if (elapsed >= timeout_ms) {
                // Remove this core from the barrier on timeout
                pico_rtos_critical_section_enter_blocking(&barrier->cs);
                barrier->core_flags &= ~core_bit;
                pico_rtos_critical_section_exit(&barrier->cs);

                PICO_RTOS_LOG_SMP_WARN("Core %lu timed out at barrier %lu", current_core, barrier->barrier_id);
                return false;
            }
        }

        // Brief delay before checking again
#if PICO_ON_DEVICE
        sleep_us(100);
#endif
    }
}

void pico_rtos_sync_barrier_reset(pico_rtos_sync_barrier_t *barrier) {
    if (!barrier) {
        return;
    }

    pico_rtos_critical_section_enter_blocking(&barrier->cs);
    barrier->core_flags = 0;
    pico_rtos_critical_section_exit(&barrier->cs);

    PICO_RTOS_LOG_SMP_DEBUG("Barrier %lu reset", barrier->barrier_id);
}

bool pico_rtos_sync_barrier_get_stats(pico_rtos_sync_barrier_t *barrier,
                                     uint32_t *wait_count,
                                     uint32_t *current_cores) {
    if (!barrier) {
        return false;
    }

    pico_rtos_critical_section_enter_blocking(&barrier->cs);

    if (wait_count) {
        *wait_count = barrier->wait_count;
    }
    if (current_cores) {
        *current_cores = barrier->core_flags;
    }

    pico_rtos_critical_section_exit(&barrier->cs);
    return true;
}

// =============================================================================
// THREAD-SAFE SYNCHRONIZATION PRIMITIVES ACROSS CORES
// =============================================================================

bool pico_rtos_mutex_enable_smp(struct pico_rtos_mutex *mutex) {
    if (!mutex || !smp_system_initialized) {
        return false;
    }

    // For now, mutexes already use critical sections which work across cores
    // In a full implementation, we might add additional inter-core coordination
    PICO_RTOS_LOG_SMP_DEBUG("Mutex SMP support enabled");
    return true;
}

bool pico_rtos_semaphore_enable_smp(struct pico_rtos_semaphore *semaphore) {
    if (!semaphore || !smp_system_initialized) {
        return false;
    }

    // Semaphores already use critical sections which work across cores
    PICO_RTOS_LOG_SMP_DEBUG("Semaphore SMP support enabled");
    return true;
}

bool pico_rtos_queue_enable_smp(struct pico_rtos_queue *queue) {
    if (!queue || !smp_system_initialized) {
        return false;
    }

    // Queues already use critical sections which work across cores
    PICO_RTOS_LOG_SMP_DEBUG("Queue SMP support enabled");
    return true;
}

bool pico_rtos_event_group_enable_smp(struct pico_rtos_event_group *event_group) {
    if (!event_group || !smp_system_initialized) {
        return false;
    }

    // Event groups already use critical sections which work across cores
    PICO_RTOS_LOG_SMP_DEBUG("Event group SMP support enabled");
    return true;
}

// =============================================================================
// DEBUGGING AND STATISTICS
// =============================================================================

bool pico_rtos_smp_get_stats(pico_rtos_smp_scheduler_t *stats) {
    if (!smp_system_initialized || !stats) {
        return false;
    }

    pico_rtos_smp_enter_critical();
    *stats = smp_scheduler;
    pico_rtos_smp_exit_critical();

    return true;
}

void pico_rtos_smp_print_state(void) {
    if (!smp_system_initialized) {
        printf("SMP system not initialized\n");
        return;
    }

    printf("=== SMP Scheduler State ===\n");
    printf("Load balancing: %s\n", smp_scheduler.load_balancing_enabled ? "enabled" : "disabled");
    printf("Load balance threshold: %lu%%\n", smp_scheduler.load_balance_threshold);
    printf("Total migrations: %lu\n", smp_scheduler.migration_count);

    for (int i = 0; i < 2; i++) {
        pico_rtos_core_state_t *core = &smp_scheduler.cores[i];
        printf("Core %d:\n", i);
        printf("  Active: %s\n", core->scheduler_active ? "yes" : "no");
        printf("  Task count: %lu\n", core->task_count);
        printf("  Load: %lu%%\n", core->load_percentage);
        printf("  Context switches: %lu\n", core->context_switches);
        printf("  Idle time: %llu us\n", core->idle_time_us);
    }
    printf("===========================\n");
}

void pico_rtos_smp_reset_stats(void) {
    if (!smp_system_initialized) {
        return;
    }

    pico_rtos_smp_enter_critical();

    smp_scheduler.migration_count = 0;
    smp_scheduler.last_balance_time = 0;

    for (int i = 0; i < 2; i++) {
        smp_scheduler.cores[i].context_switches = 0;
        smp_scheduler.cores[i].idle_time_us = 0;
        smp_scheduler.cores[i].total_runtime_us = 0;
    }

    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_INFO("SMP statistics reset");
}

// =============================================================================
// INTERNAL SCHEDULER FUNCTIONS
// =============================================================================

void pico_rtos_smp_schedule_next_task(void) {
    uint32_t core_id = pico_rtos_smp_get_core_id();

    if (!smp_system_initialized || core_id >= 2) {
        return;
    }

    pico_rtos_core_state_t *core = &smp_scheduler.cores[core_id];

    // For now, just run the idle task
    // In a full implementation, this would select the highest priority ready task
    core->running_task = &idle_tasks[core_id];
    core->context_switches++;
}

void pico_rtos_smp_add_task_to_core(pico_rtos_task_t *task, uint32_t core_id) {
    if (!smp_system_initialized || !task || core_id >= 2) {
        return;
    }

    pico_rtos_smp_enter_critical();

    pico_rtos_core_state_t *core = &smp_scheduler.cores[core_id];

    // Add task to core's ready list maintaining priority order
    if (core->ready_list == NULL || task->priority > core->ready_list->priority) {
        // Insert at head
        task->next = core->ready_list;
        core->ready_list = task;
    } else {
        // Find correct position based on priority
        pico_rtos_task_t *current = core->ready_list;
        while (current->next != NULL && current->next->priority >= task->priority) {
            current = current->next;
        }
        task->next = current->next;
        current->next = task;
    }

    core->task_count++;

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    task->assigned_core = core_id;
#endif

    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_DEBUG("Task %s added to core %lu (priority %lu)",
                           task->name ? task->name : "unnamed", core_id, task->priority);
}

/**
 * @brief Add new task to optimal core based on assignment strategy
 */
bool pico_rtos_smp_add_new_task(pico_rtos_task_t *task) {
    if (!smp_system_initialized || !task) {
        return false;
    }

    // Find optimal core for this task
    uint32_t target_core = pico_rtos_smp_assign_task_to_core(task);

    // Add task to the selected core
    pico_rtos_smp_add_task_to_core(task, target_core);

    PICO_RTOS_LOG_SMP_INFO("New task %s assigned to core %lu using strategy %d",
                          task->name ? task->name : "unnamed", target_core, assignment_strategy);

    return true;
}

void pico_rtos_smp_remove_task_from_core(pico_rtos_task_t *task, uint32_t core_id) {
    if (!smp_system_initialized || !task || core_id >= 2) {
        return;
    }

    pico_rtos_smp_enter_critical();

    pico_rtos_core_state_t *core = &smp_scheduler.cores[core_id];

    // Remove task from core's ready list (simplified implementation)
    if (core->ready_list == task) {
        core->ready_list = task->next;
        core->task_count--;
    } else {
        pico_rtos_task_t *prev = core->ready_list;
        while (prev && prev->next != task) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = task->next;
            core->task_count--;
        }
    }

    task->next = NULL;

    pico_rtos_smp_exit_critical();

    PICO_RTOS_LOG_SMP_DEBUG("Task %s removed from core %lu",
                           task->name ? task->name : "unnamed", core_id);
}