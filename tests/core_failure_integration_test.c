#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// Include the platform header for consistent definitions
#include "pico_rtos/platform.h"

// Mock RTOS functions
static uint32_t mock_tick_count = 0;
uint32_t pico_rtos_get_tick_count(void) { return mock_tick_count; }

// Mock task structure for testing
typedef struct pico_rtos_task {
    const char *name;
    void (*function)(void *);
    void *param;
    uint32_t stack_size;
    uint32_t priority;
    uint32_t original_priority;
    uint32_t state;
    uint32_t block_reason;
    bool auto_delete;
    uint32_t *stack_base;
    uint32_t *stack_ptr;
    critical_section_t cs;
    uint32_t core_affinity;
    uint32_t assigned_core;
    uint64_t cpu_time_us;
    uint32_t context_switch_count;
    uint32_t stack_high_water_mark;
    uint32_t migration_count;
    bool migration_pending;
    uint32_t last_run_core;
    void *task_local_storage[4];
    struct pico_rtos_task *next;
} pico_rtos_task_t;

static pico_rtos_task_t mock_current_task = {
    .name = "test_task",
    .core_affinity = 3, // PICO_RTOS_CORE_AFFINITY_ANY
    .assigned_core = 0
};

pico_rtos_task_t *pico_rtos_get_current_task(void) {
    return &mock_current_task;
}

uint32_t *pico_rtos_init_task_stack(uint32_t *stack_base, uint32_t stack_size, 
                                   void (*function)(void *), void *param) {
    return stack_base + (stack_size / sizeof(uint32_t)) - 16;
}

// Test framework
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return false; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return true; \
    } while(0)

// Include the actual SMP implementation (simplified for testing)
#include "../src/smp.c"

// Test helper functions
static void reset_test_environment(void) {
    mock_tick_count = 0;
    smp_system_initialized = false;
    health_monitoring_initialized = false;
    memset(&smp_scheduler, 0, sizeof(smp_scheduler));
}

static void advance_time(uint32_t ms) {
    mock_tick_count += ms;
}

// Test callback for failure scenarios
static bool test_callback_called = false;
static uint32_t test_callback_core_id = 0;
static pico_rtos_core_failure_type_t test_callback_failure_type = PICO_RTOS_CORE_FAILURE_NONE;

static bool test_failure_callback(uint32_t failed_core_id,
                                 pico_rtos_core_failure_type_t failure_type,
                                 void *user_data) {
    test_callback_called = true;
    test_callback_core_id = failed_core_id;
    test_callback_failure_type = failure_type;
    return true; // Allow recovery
}

// =============================================================================
// INTEGRATION TESTS
// =============================================================================

/**
 * @brief Test complete SMP system initialization with health monitoring
 */
static bool test_complete_smp_initialization(void) {
    reset_test_environment();
    
    // Initialize SMP system (this should also initialize health monitoring)
    TEST_ASSERT(pico_rtos_smp_init(), "SMP system initialization failed");
    
    // Verify SMP scheduler state
    pico_rtos_smp_scheduler_t *scheduler = pico_rtos_smp_get_scheduler_state();
    TEST_ASSERT(scheduler != NULL, "Failed to get scheduler state");
    TEST_ASSERT(scheduler->smp_initialized, "SMP system not marked as initialized");
    
    // Verify core states
    for (int i = 0; i < 2; i++) {
        pico_rtos_core_state_t *core = pico_rtos_smp_get_core_state(i);
        TEST_ASSERT(core != NULL, "Failed to get core state");
        TEST_ASSERT(core->core_id == i, "Core ID mismatch");
        TEST_ASSERT(core->health.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core not initialized as healthy");
    }
    
    // Verify health monitoring is initialized
    TEST_ASSERT(health_monitoring_initialized, "Health monitoring not initialized");
    
    TEST_PASS();
}

/**
 * @brief Test core health monitoring integration with SMP scheduler
 */
static bool test_health_monitoring_integration(void) {
    reset_test_environment();
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP system initialization failed");
    
    // Start health monitoring
    pico_rtos_core_health_start();
    
    // Register failure callback
    TEST_ASSERT(pico_rtos_core_health_register_failure_callback(test_failure_callback, NULL),
               "Failed to register failure callback");
    
    // Simulate normal operation
    for (int i = 0; i < 10; i++) {
        pico_rtos_core_health_feed_watchdog();
        pico_rtos_core_health_send_heartbeat();
        advance_time(100);
        pico_rtos_core_health_process();
    }
    
    // Verify cores remain healthy
    for (int i = 0; i < 2; i++) {
        pico_rtos_core_health_state_t state;
        TEST_ASSERT(pico_rtos_core_health_get_state(i, &state), "Failed to get core health state");
        TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core should remain healthy");
        TEST_ASSERT(state.watchdog_feed_count > 0, "Watchdog should have been fed");
    }
    
    TEST_PASS();
}

/**
 * @brief Test watchdog failure detection and recovery cycle
 */
static bool test_watchdog_failure_recovery_cycle(void) {
    reset_test_environment();
    test_callback_called = false;
    
    // Initialize with short timeouts for testing
    pico_rtos_core_health_config_t config = {
        .enabled = true,
        .watchdog_timeout_ms = 1000,
        .health_check_interval_ms = 100,
        .max_missed_heartbeats = 3,
        .recovery_timeout_ms = 500,
        .auto_recovery_enabled = true,
        .graceful_degradation_enabled = false
    };
    
    TEST_ASSERT(pico_rtos_smp_init(), "SMP system initialization failed");
    TEST_ASSERT(pico_rtos_core_health_init(&config), "Health monitoring init failed");
    TEST_ASSERT(pico_rtos_core_health_register_failure_callback(test_failure_callback, NULL),
               "Failed to register failure callback");
    
    pico_rtos_core_health_start();
    
    // Normal operation
    pico_rtos_core_health_feed_watchdog();
    pico_rtos_core_health_send_heartbeat();
    
    // Stop feeding watchdog to trigger failure
    advance_time(1500); // Exceed watchdog timeout
    pico_rtos_core_health_process();
    
    // Verify failure was detected
    pico_rtos_core_health_state_t state;
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core state");
    TEST_ASSERT(state.failure_count > 0, "Failure should have been detected");
    TEST_ASSERT(test_callback_called, "Failure callback should have been called");
    TEST_ASSERT(test_callback_failure_type == PICO_RTOS_CORE_FAILURE_WATCHDOG, 
               "Should detect watchdog failure");
    
    // Test manual recovery
    TEST_ASSERT(pico_rtos_core_health_recover_core(0), "Manual recovery should succeed");
    
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core state after recovery");
    TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core should be healthy after recovery");
    TEST_ASSERT(state.recovery_attempts > 0, "Recovery attempts should be recorded");
    
    TEST_PASS();
}

/**
 * @brief Test graceful degradation to single-core mode
 */
static bool test_graceful_degradation_integration(void) {
    reset_test_environment();
    
    pico_rtos_core_health_config_t config = {
        .enabled = true,
        .watchdog_timeout_ms = 1000,
        .health_check_interval_ms = 100,
        .max_missed_heartbeats = 2,
        .recovery_timeout_ms = 500,
        .auto_recovery_enabled = false, // Disable recovery to force degradation
        .graceful_degradation_enabled = true
    };
    
    TEST_ASSERT(pico_rtos_smp_init(), "SMP system initialization failed");
    TEST_ASSERT(pico_rtos_core_health_init(&config), "Health monitoring init failed");
    pico_rtos_core_health_start();
    
    // Verify initial dual-core mode
    TEST_ASSERT(!pico_rtos_core_health_is_single_core_mode(), "Should start in dual-core mode");
    
    // Trigger graceful degradation
    uint32_t migrated_tasks = pico_rtos_core_health_degrade_to_single_core(1);
    
    // Verify single-core mode
    TEST_ASSERT(pico_rtos_core_health_is_single_core_mode(), "Should be in single-core mode");
    
    // Verify scheduler state reflects single-core operation
    pico_rtos_smp_scheduler_t *scheduler = pico_rtos_smp_get_scheduler_state();
    TEST_ASSERT(scheduler->single_core_mode, "Scheduler should be in single-core mode");
    TEST_ASSERT(scheduler->healthy_core_id == 0, "Healthy core should be core 0");
    
    TEST_PASS();
}

/**
 * @brief Test load balancing with health monitoring
 */
static bool test_load_balancing_with_health_monitoring(void) {
    reset_test_environment();
    
    TEST_ASSERT(pico_rtos_smp_init(), "SMP system initialization failed");
    pico_rtos_core_health_start();
    
    // Enable load balancing
    pico_rtos_smp_set_load_balancing(true);
    pico_rtos_smp_set_load_balance_threshold(20);
    
    // Verify load balancing is enabled
    pico_rtos_smp_scheduler_t *scheduler = pico_rtos_smp_get_scheduler_state();
    TEST_ASSERT(scheduler->load_balancing_enabled, "Load balancing should be enabled");
    TEST_ASSERT(scheduler->load_balance_threshold == 20, "Load balance threshold should be set");
    
    // Test load calculation
    uint32_t core0_load = pico_rtos_smp_get_core_load(0);
    uint32_t core1_load = pico_rtos_smp_get_core_load(1);
    
    // Loads should be calculable (even if zero in test environment)
    TEST_ASSERT(core0_load <= 100, "Core 0 load should be valid percentage");
    TEST_ASSERT(core1_load <= 100, "Core 1 load should be valid percentage");
    
    // Test load balancing trigger
    uint32_t migrations = pico_rtos_smp_balance_load();
    // In test environment with no real tasks, migrations should be 0
    
    TEST_PASS();
}

/**
 * @brief Test task affinity with health monitoring
 */
static bool test_task_affinity_with_health_monitoring(void) {
    reset_test_environment();
    
    TEST_ASSERT(pico_rtos_smp_init(), "SMP system initialization failed");
    pico_rtos_core_health_start();
    
    // Test setting task affinity
    TEST_ASSERT(pico_rtos_smp_set_task_affinity(NULL, 1), "Failed to set task affinity to core 0"); // PICO_RTOS_CORE_AFFINITY_CORE0
    
    // Verify affinity was set
    uint32_t affinity = pico_rtos_smp_get_task_affinity(NULL);
    TEST_ASSERT(affinity == 1, "Task affinity should be set to core 0");
    
    // Test affinity checking
    TEST_ASSERT(pico_rtos_smp_task_can_run_on_core(&mock_current_task, 0), "Task should be able to run on core 0");
    TEST_ASSERT(!pico_rtos_smp_task_can_run_on_core(&mock_current_task, 1), "Task should not be able to run on core 1");
    
    // Test setting to any core
    TEST_ASSERT(pico_rtos_smp_set_task_affinity(NULL, 3), "Failed to set task affinity to any core"); // PICO_RTOS_CORE_AFFINITY_ANY
    
    affinity = pico_rtos_smp_get_task_affinity(NULL);
    TEST_ASSERT(affinity == 3, "Task affinity should be set to any core");
    
    TEST_ASSERT(pico_rtos_smp_task_can_run_on_core(&mock_current_task, 0), "Task should be able to run on core 0");
    TEST_ASSERT(pico_rtos_smp_task_can_run_on_core(&mock_current_task, 1), "Task should be able to run on core 1");
    
    TEST_PASS();
}

/**
 * @brief Test comprehensive failure statistics
 */
static bool test_comprehensive_failure_statistics(void) {
    reset_test_environment();
    
    TEST_ASSERT(pico_rtos_smp_init(), "SMP system initialization failed");
    pico_rtos_core_health_start();
    
    // Get initial statistics
    uint32_t initial_failures, initial_recoveries, initial_downtime;
    TEST_ASSERT(pico_rtos_core_health_get_failure_stats(0, &initial_failures, &initial_recoveries, &initial_downtime),
               "Failed to get initial statistics");
    
    TEST_ASSERT(initial_failures == 0, "Initial failure count should be zero");
    TEST_ASSERT(initial_recoveries == 0, "Initial recovery count should be zero");
    
    // Simulate some failures and recoveries
    for (int i = 0; i < 3; i++) {
        // Set core as failed
        smp_scheduler.cores[0].health.status = PICO_RTOS_CORE_HEALTH_FAILED;
        smp_scheduler.cores[0].health.failure_count++;
        
        // Recover core
        TEST_ASSERT(pico_rtos_core_health_recover_core(0), "Recovery should succeed");
    }
    
    // Check updated statistics
    uint32_t final_failures, final_recoveries, final_downtime;
    TEST_ASSERT(pico_rtos_core_health_get_failure_stats(0, &final_failures, &final_recoveries, &final_downtime),
               "Failed to get final statistics");
    
    TEST_ASSERT(final_failures == 3, "Should have 3 failures recorded");
    TEST_ASSERT(final_recoveries == 3, "Should have 3 recoveries recorded");
    
    // Test statistics reset
    pico_rtos_core_health_reset_stats(0xFF); // Reset all cores
    
    uint32_t reset_failures, reset_recoveries, reset_downtime;
    TEST_ASSERT(pico_rtos_core_health_get_failure_stats(0, &reset_failures, &reset_recoveries, &reset_downtime),
               "Failed to get statistics after reset");
    
    TEST_ASSERT(reset_failures == 0, "Failure count should be reset");
    TEST_ASSERT(reset_recoveries == 0, "Recovery count should be reset");
    
    TEST_PASS();
}

// =============================================================================
// TEST RUNNER
// =============================================================================

typedef struct {
    const char *name;
    bool (*test_func)(void);
} test_case_t;

static const test_case_t test_cases[] = {
    {"Complete SMP Initialization", test_complete_smp_initialization},
    {"Health Monitoring Integration", test_health_monitoring_integration},
    {"Watchdog Failure Recovery Cycle", test_watchdog_failure_recovery_cycle},
    {"Graceful Degradation Integration", test_graceful_degradation_integration},
    {"Load Balancing with Health Monitoring", test_load_balancing_with_health_monitoring},
    {"Task Affinity with Health Monitoring", test_task_affinity_with_health_monitoring},
    {"Comprehensive Failure Statistics", test_comprehensive_failure_statistics},
};

int main(void) {
    printf("=== Core Failure Detection and Recovery Integration Tests ===\n\n");
    
    int total_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed_tests = 0;
    
    for (int i = 0; i < total_tests; i++) {
        printf("Running test: %s\n", test_cases[i].name);
        
        if (test_cases[i].test_func()) {
            passed_tests++;
        } else {
            printf("Test failed: %s\n", test_cases[i].name);
        }
        
        printf("\n");
    }
    
    printf("=== Test Results ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    printf("Success rate: %.1f%%\n", (float)passed_tests / total_tests * 100.0f);
    
    if (passed_tests == total_tests) {
        printf("\n🎉 All core failure detection and recovery tests passed!\n");
        printf("✅ Task 6.4 implementation is complete and verified.\n");
    }
    
    return (passed_tests == total_tests) ? 0 : 1;
}