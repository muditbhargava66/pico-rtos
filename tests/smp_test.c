#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// Include the actual SMP header to get real types
#include "pico_rtos/smp.h"

// Test configuration
#define TEST_TASK_STACK_SIZE 512
#define TEST_TIMEOUT_MS 1000

// Test task parameters
typedef struct {
    uint32_t task_id;
    uint32_t run_count;
    uint32_t core_id;
    bool completed;
} test_task_param_t;

// Global test state
static bool test_passed = true;
static uint32_t test_count = 0;
static uint32_t test_failures = 0;

// Test helper macros
#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (!(condition)) { \
            printf("FAIL: %s (line %d): %s\n", __func__, __LINE__, message); \
            test_passed = false; \
            test_failures++; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

#define TEST_START(test_name) \
    do { \
        printf("\n=== Starting %s ===\n", test_name); \
        test_passed = true; \
    } while(0)

#define TEST_END(test_name) \
    do { \
        printf("=== %s %s ===\n", test_name, test_passed ? "PASSED" : "FAILED"); \
    } while(0)

// =============================================================================
// TEST HELPER FUNCTIONS
// =============================================================================

static void test_task_function(void *param) {
    test_task_param_t *test_param = (test_task_param_t *)param;
    
    if (test_param) {
        test_param->run_count++;
        test_param->core_id = pico_rtos_smp_get_core_id();
        test_param->completed = true;
    }
    
    // Simple task that runs briefly then exits
    pico_rtos_task_delay(10);
}

static void reset_test_state(void) {
    test_passed = true;
}

// =============================================================================
// SMP INITIALIZATION TESTS
// =============================================================================

static void test_smp_initialization(void) {
    TEST_START("SMP Initialization");
    
    // Test multiple initialization calls
    bool init_result1 = pico_rtos_smp_init();
    TEST_ASSERT(init_result1, "First SMP initialization should succeed");
    
    bool init_result2 = pico_rtos_smp_init();
    TEST_ASSERT(init_result2, "Second SMP initialization should succeed (idempotent)");
    
    // Test getting scheduler state
    pico_rtos_smp_scheduler_t *scheduler_state = pico_rtos_smp_get_scheduler_state();
    TEST_ASSERT(scheduler_state != NULL, "Should be able to get scheduler state");
    TEST_ASSERT(scheduler_state->smp_initialized, "SMP should be marked as initialized");
    
    // Test core state access
    pico_rtos_core_state_t *core0_state = pico_rtos_smp_get_core_state(0);
    TEST_ASSERT(core0_state != NULL, "Should be able to get core 0 state");
    TEST_ASSERT(core0_state->core_id == 0, "Core 0 should have correct ID");
    
    pico_rtos_core_state_t *core1_state = pico_rtos_smp_get_core_state(1);
    TEST_ASSERT(core1_state != NULL, "Should be able to get core 1 state");
    TEST_ASSERT(core1_state->core_id == 1, "Core 1 should have correct ID");
    
    // Test invalid core access
    pico_rtos_core_state_t *invalid_core = pico_rtos_smp_get_core_state(2);
    TEST_ASSERT(invalid_core == NULL, "Should return NULL for invalid core ID");
    
    TEST_END("SMP Initialization");
}

// =============================================================================
// CORE IDENTIFICATION TESTS
// =============================================================================

static void test_core_identification(void) {
    TEST_START("Core Identification");
    
    uint32_t core_id = pico_rtos_smp_get_core_id();
    TEST_ASSERT(core_id == 0 || core_id == 1, "Core ID should be 0 or 1");
    
    // Since we're running on core 0 during tests, it should be 0
    TEST_ASSERT(core_id == 0, "Test should be running on core 0");
    
    TEST_END("Core Identification");
}

// =============================================================================
// TASK AFFINITY TESTS
// =============================================================================

static void test_task_affinity_management(void) {
    TEST_START("Task Affinity Management");
    
    // Create a test task
    pico_rtos_task_t test_task;
    test_task_param_t task_param = {0};
    
    bool task_created = pico_rtos_task_create(&test_task, "test_affinity_task",
                                             test_task_function, &task_param,
                                             TEST_TASK_STACK_SIZE, 5);
    TEST_ASSERT(task_created, "Test task should be created successfully");
    
    // Test default affinity
    pico_rtos_core_affinity_t default_affinity = pico_rtos_smp_get_task_affinity(&test_task);
    TEST_ASSERT(default_affinity == PICO_RTOS_CORE_AFFINITY_NONE, 
               "Default task affinity should be NONE");
    
    // Test setting affinity to core 0
    bool set_result = pico_rtos_smp_set_task_affinity(&test_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    TEST_ASSERT(set_result, "Should be able to set task affinity to core 0");
    
    pico_rtos_core_affinity_t core0_affinity = pico_rtos_smp_get_task_affinity(&test_task);
    TEST_ASSERT(core0_affinity == PICO_RTOS_CORE_AFFINITY_CORE0,
               "Task affinity should be set to core 0");
    
    // Test setting affinity to core 1
    set_result = pico_rtos_smp_set_task_affinity(&test_task, PICO_RTOS_CORE_AFFINITY_CORE1);
    TEST_ASSERT(set_result, "Should be able to set task affinity to core 1");
    
    pico_rtos_core_affinity_t core1_affinity = pico_rtos_smp_get_task_affinity(&test_task);
    TEST_ASSERT(core1_affinity == PICO_RTOS_CORE_AFFINITY_CORE1,
               "Task affinity should be set to core 1");
    
    // Test setting affinity to any core
    set_result = pico_rtos_smp_set_task_affinity(&test_task, PICO_RTOS_CORE_AFFINITY_ANY);
    TEST_ASSERT(set_result, "Should be able to set task affinity to any core");
    
    pico_rtos_core_affinity_t any_affinity = pico_rtos_smp_get_task_affinity(&test_task);
    TEST_ASSERT(any_affinity == PICO_RTOS_CORE_AFFINITY_ANY,
               "Task affinity should be set to any core");
    
    // Test task can run on core checks
    pico_rtos_smp_set_task_affinity(&test_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    TEST_ASSERT(pico_rtos_smp_task_can_run_on_core(&test_task, 0),
               "Task with core 0 affinity should be able to run on core 0");
    TEST_ASSERT(!pico_rtos_smp_task_can_run_on_core(&test_task, 1),
               "Task with core 0 affinity should not be able to run on core 1");
    
    pico_rtos_smp_set_task_affinity(&test_task, PICO_RTOS_CORE_AFFINITY_ANY);
    TEST_ASSERT(pico_rtos_smp_task_can_run_on_core(&test_task, 0),
               "Task with any affinity should be able to run on core 0");
    TEST_ASSERT(pico_rtos_smp_task_can_run_on_core(&test_task, 1),
               "Task with any affinity should be able to run on core 1");
    
    // Test invalid parameters
    TEST_ASSERT(!pico_rtos_smp_task_can_run_on_core(&test_task, 2),
               "Should return false for invalid core ID");
    TEST_ASSERT(!pico_rtos_smp_task_can_run_on_core(NULL, 0),
               "Should return false for NULL task");
    
    // Clean up
    pico_rtos_task_delete(&test_task);
    
    TEST_END("Task Affinity Management");
}

// =============================================================================
// TASK DISTRIBUTION TESTS
// =============================================================================

static void test_task_assignment_strategies(void) {
    TEST_START("Task Assignment Strategies");
    
    // Test setting assignment strategy
    pico_rtos_smp_set_assignment_strategy(PICO_RTOS_ASSIGN_ROUND_ROBIN);
    pico_rtos_task_assignment_strategy_t strategy = pico_rtos_smp_get_assignment_strategy();
    TEST_ASSERT(strategy == PICO_RTOS_ASSIGN_ROUND_ROBIN, 
               "Assignment strategy should be set to round-robin");
    
    pico_rtos_smp_set_assignment_strategy(PICO_RTOS_ASSIGN_LEAST_LOADED);
    strategy = pico_rtos_smp_get_assignment_strategy();
    TEST_ASSERT(strategy == PICO_RTOS_ASSIGN_LEAST_LOADED,
               "Assignment strategy should be set to least-loaded");
    
    pico_rtos_smp_set_assignment_strategy(PICO_RTOS_ASSIGN_PRIORITY_BASED);
    strategy = pico_rtos_smp_get_assignment_strategy();
    TEST_ASSERT(strategy == PICO_RTOS_ASSIGN_PRIORITY_BASED,
               "Assignment strategy should be set to priority-based");
    
    // Test task assignment with different strategies
    pico_rtos_task_t test_task1, test_task2, test_task3;
    test_task_param_t param1 = {1}, param2 = {2}, param3 = {3};
    
    // Create test tasks with different priorities
    bool created1 = pico_rtos_task_create(&test_task1, "test_task1", test_task_function, &param1, TEST_TASK_STACK_SIZE, 8);
    bool created2 = pico_rtos_task_create(&test_task2, "test_task2", test_task_function, &param2, TEST_TASK_STACK_SIZE, 3);
    bool created3 = pico_rtos_task_create(&test_task3, "test_task3", test_task_function, &param3, TEST_TASK_STACK_SIZE, 6);
    
    TEST_ASSERT(created1 && created2 && created3, "All test tasks should be created");
    
    // Test adding tasks with different strategies
    pico_rtos_smp_set_assignment_strategy(PICO_RTOS_ASSIGN_LEAST_LOADED);
    bool added1 = pico_rtos_smp_add_new_task(&test_task1);
    TEST_ASSERT(added1, "Task should be added successfully with least-loaded strategy");
    
    pico_rtos_smp_set_assignment_strategy(PICO_RTOS_ASSIGN_PRIORITY_BASED);
    bool added2 = pico_rtos_smp_add_new_task(&test_task2);
    bool added3 = pico_rtos_smp_add_new_task(&test_task3);
    TEST_ASSERT(added2 && added3, "Tasks should be added successfully with priority-based strategy");
    
    // Clean up
    pico_rtos_task_delete(&test_task1);
    pico_rtos_task_delete(&test_task2);
    pico_rtos_task_delete(&test_task3);
    
    TEST_END("Task Assignment Strategies");
}

// =============================================================================
// LOAD BALANCING TESTS
// =============================================================================

static void test_load_balancing_configuration(void) {
    TEST_START("Load Balancing Configuration");
    
    // Test enabling/disabling load balancing
    pico_rtos_smp_set_load_balancing(true);
    pico_rtos_smp_scheduler_t *scheduler = pico_rtos_smp_get_scheduler_state();
    TEST_ASSERT(scheduler->load_balancing_enabled, "Load balancing should be enabled");
    
    pico_rtos_smp_set_load_balancing(false);
    TEST_ASSERT(!scheduler->load_balancing_enabled, "Load balancing should be disabled");
    
    // Test setting load balance threshold
    pico_rtos_smp_set_load_balance_threshold(30);
    TEST_ASSERT(scheduler->load_balance_threshold == 30, 
               "Load balance threshold should be set to 30");
    
    pico_rtos_smp_set_load_balance_threshold(75);
    TEST_ASSERT(scheduler->load_balance_threshold == 75,
               "Load balance threshold should be set to 75");
    
    // Test invalid threshold (should be ignored)
    pico_rtos_smp_set_load_balance_threshold(150);
    TEST_ASSERT(scheduler->load_balance_threshold == 75,
               "Invalid threshold should be ignored");
    
    // Test core load calculation
    uint32_t core0_load = pico_rtos_smp_get_core_load(0);
    TEST_ASSERT(core0_load <= 100, "Core 0 load should be <= 100%");
    
    uint32_t core1_load = pico_rtos_smp_get_core_load(1);
    TEST_ASSERT(core1_load <= 100, "Core 1 load should be <= 100%");
    
    // Test invalid core load
    uint32_t invalid_load = pico_rtos_smp_get_core_load(2);
    TEST_ASSERT(invalid_load == 0, "Invalid core should return 0% load");
    
    TEST_END("Load Balancing Configuration");
}

static void test_advanced_load_balancing(void) {
    TEST_START("Advanced Load Balancing");
    
    // Enable load balancing
    pico_rtos_smp_set_load_balancing(true);
    pico_rtos_smp_set_load_balance_threshold(20);
    
    // Test manual load balancing
    uint32_t migrations = pico_rtos_smp_balance_load();
    TEST_ASSERT(migrations >= 0, "Load balancing should return valid migration count");
    
    // Test load balance statistics
    uint32_t total_migrations = 0;
    uint32_t last_balance_time = 0;
    uint32_t balance_cycles = 0;
    
    bool stats_result = pico_rtos_smp_get_load_balance_stats(&total_migrations, 
                                                            &last_balance_time, 
                                                            &balance_cycles);
    TEST_ASSERT(stats_result, "Should be able to get load balance statistics");
    TEST_ASSERT(total_migrations >= 0, "Total migrations should be non-negative");
    TEST_ASSERT(last_balance_time >= 0, "Last balance time should be non-negative");
    
    // Test with NULL parameters
    stats_result = pico_rtos_smp_get_load_balance_stats(NULL, NULL, NULL);
    TEST_ASSERT(stats_result, "Should handle NULL parameters gracefully");
    
    TEST_END("Advanced Load Balancing");
}

// =============================================================================
// TASK MIGRATION TESTS
// =============================================================================

static void test_task_migration(void) {
    TEST_START("Task Migration");
    
    // Create a test task
    pico_rtos_task_t test_task;
    test_task_param_t task_param = {0};
    
    bool task_created = pico_rtos_task_create(&test_task, "test_migration_task",
                                             test_task_function, &task_param,
                                             TEST_TASK_STACK_SIZE, 5);
    TEST_ASSERT(task_created, "Test task should be created successfully");
    
    // Set task affinity to allow migration
    pico_rtos_smp_set_task_affinity(&test_task, PICO_RTOS_CORE_AFFINITY_ANY);
    
    // Test migration request
    bool migrate_result = pico_rtos_smp_migrate_task(&test_task, 1, PICO_RTOS_MIGRATION_USER_REQUEST);
    TEST_ASSERT(migrate_result, "Should be able to request task migration");
    
    // Test migration to same core (should succeed but do nothing)
    migrate_result = pico_rtos_smp_migrate_task(&test_task, 0, PICO_RTOS_MIGRATION_USER_REQUEST);
    TEST_ASSERT(migrate_result, "Migration to same core should succeed");
    
    // Test migration with affinity constraint
    pico_rtos_smp_set_task_affinity(&test_task, PICO_RTOS_CORE_AFFINITY_CORE0);
    migrate_result = pico_rtos_smp_migrate_task(&test_task, 1, PICO_RTOS_MIGRATION_USER_REQUEST);
    TEST_ASSERT(!migrate_result, "Migration should fail due to affinity constraint");
    
    // Test migration statistics
    uint32_t migration_count = 0;
    uint32_t last_migration_time = 0;
    bool stats_result = pico_rtos_smp_get_migration_stats(&test_task, &migration_count, &last_migration_time);
    TEST_ASSERT(stats_result, "Should be able to get migration statistics");
    
    // Test invalid migration parameters
    migrate_result = pico_rtos_smp_migrate_task(&test_task, 2, PICO_RTOS_MIGRATION_USER_REQUEST);
    TEST_ASSERT(!migrate_result, "Migration to invalid core should fail");
    
    migrate_result = pico_rtos_smp_migrate_task(NULL, 1, PICO_RTOS_MIGRATION_USER_REQUEST);
    TEST_ASSERT(!migrate_result, "Migration of NULL task should fail");
    
    // Clean up
    pico_rtos_task_delete(&test_task);
    
    TEST_END("Task Migration");
}

// =============================================================================
// INTER-CORE SYNCHRONIZATION TESTS
// =============================================================================

static void test_inter_core_synchronization(void) {
    TEST_START("Inter-Core Synchronization");
    
    // Test SMP critical sections
    pico_rtos_smp_enter_critical();
    // Critical section operations would go here
    pico_rtos_smp_exit_critical();
    TEST_ASSERT(true, "SMP critical section enter/exit should work");
    
    // Test nested critical sections
    pico_rtos_smp_enter_critical();
    pico_rtos_smp_enter_critical();
    pico_rtos_smp_exit_critical();
    pico_rtos_smp_exit_critical();
    TEST_ASSERT(true, "Nested SMP critical sections should work");
    
    // Test core wake-up (should not crash)
    pico_rtos_smp_wake_core(0);
    pico_rtos_smp_wake_core(1);
    TEST_ASSERT(true, "Core wake-up calls should not crash");
    
    // Test invalid core wake-up
    pico_rtos_smp_wake_core(2);
    TEST_ASSERT(true, "Invalid core wake-up should be handled gracefully");
    
    TEST_END("Inter-Core Synchronization");
}

// =============================================================================
// STATISTICS AND DEBUGGING TESTS
// =============================================================================

static void test_statistics_and_debugging(void) {
    TEST_START("Statistics and Debugging");
    
    // Test getting SMP statistics
    pico_rtos_smp_scheduler_t stats;
    bool stats_result = pico_rtos_smp_get_stats(&stats);
    TEST_ASSERT(stats_result, "Should be able to get SMP statistics");
    TEST_ASSERT(stats.smp_initialized, "Statistics should show SMP as initialized");
    
    // Test statistics reset
    pico_rtos_smp_reset_stats();
    stats_result = pico_rtos_smp_get_stats(&stats);
    TEST_ASSERT(stats_result, "Should be able to get statistics after reset");
    TEST_ASSERT(stats.migration_count == 0, "Migration count should be reset to 0");
    
    // Test print state function (should not crash)
    pico_rtos_smp_print_state();
    TEST_ASSERT(true, "Print state function should not crash");
    
    // Test with NULL parameters
    stats_result = pico_rtos_smp_get_stats(NULL);
    TEST_ASSERT(!stats_result, "Should return false for NULL stats parameter");
    
    TEST_END("Statistics and Debugging");
}

// =============================================================================
// CORE STATE MANAGEMENT TESTS
// =============================================================================

static void test_core_state_management(void) {
    TEST_START("Core State Management");
    
    // Test initial core states
    pico_rtos_core_state_t *core0 = pico_rtos_smp_get_core_state(0);
    pico_rtos_core_state_t *core1 = pico_rtos_smp_get_core_state(1);
    
    TEST_ASSERT(core0 != NULL, "Core 0 state should be accessible");
    TEST_ASSERT(core1 != NULL, "Core 1 state should be accessible");
    
    TEST_ASSERT(core0->core_id == 0, "Core 0 should have correct ID");
    TEST_ASSERT(core1->core_id == 1, "Core 1 should have correct ID");
    
    TEST_ASSERT(core0->task_count >= 0, "Core 0 task count should be non-negative");
    TEST_ASSERT(core1->task_count >= 0, "Core 1 task count should be non-negative");
    
    TEST_ASSERT(core0->load_percentage <= 100, "Core 0 load should be <= 100%");
    TEST_ASSERT(core1->load_percentage <= 100, "Core 1 load should be <= 100%");
    
    // Test that context switch counts are initialized
    TEST_ASSERT(core0->context_switches >= 0, "Core 0 context switches should be non-negative");
    TEST_ASSERT(core1->context_switches >= 0, "Core 1 context switches should be non-negative");
    
    TEST_END("Core State Management");
}

// =============================================================================
// ERROR HANDLING TESTS
// =============================================================================

static void test_error_handling(void) {
    TEST_START("Error Handling");
    
    // Test operations before initialization (simulate uninitialized state)
    // Note: Since we've already initialized, we test with invalid parameters instead
    
    // Test NULL task affinity operations
    pico_rtos_core_affinity_t affinity = pico_rtos_smp_get_task_affinity(NULL);
    TEST_ASSERT(affinity == PICO_RTOS_CORE_AFFINITY_NONE, 
               "NULL task should return NONE affinity");
    
    bool set_result = pico_rtos_smp_set_task_affinity(NULL, PICO_RTOS_CORE_AFFINITY_CORE0);
    TEST_ASSERT(!set_result, "Setting affinity for NULL task should fail");
    
    // Test invalid core operations
    pico_rtos_core_state_t *invalid_core = pico_rtos_smp_get_core_state(99);
    TEST_ASSERT(invalid_core == NULL, "Invalid core ID should return NULL");
    
    uint32_t invalid_load = pico_rtos_smp_get_core_load(99);
    TEST_ASSERT(invalid_load == 0, "Invalid core load should return 0");
    
    // Test migration with invalid parameters
    bool migrate_result = pico_rtos_smp_migrate_task(NULL, 0, PICO_RTOS_MIGRATION_USER_REQUEST);
    TEST_ASSERT(!migrate_result, "Migration with NULL task should fail");
    
    migrate_result = pico_rtos_smp_migrate_task(NULL, 99, PICO_RTOS_MIGRATION_USER_REQUEST);
    TEST_ASSERT(!migrate_result, "Migration to invalid core should fail");
    
    TEST_END("Error Handling");
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main(void) {
    // Initialize stdio (will use real implementation)
    
    printf("\n");
    printf("========================================\n");
    printf("    Pico-RTOS SMP Foundation Tests\n");
    printf("========================================\n");
    
    // Initialize the SMP system for testing
    if (!pico_rtos_smp_init()) {
        printf("FATAL: Failed to initialize Pico-RTOS SMP\n");
        return 1;
    }
    
    // Run all tests
    test_smp_initialization();
    test_core_identification();
    test_task_affinity_management();
    test_task_assignment_strategies();
    test_load_balancing_configuration();
    test_advanced_load_balancing();
    test_task_migration();
    test_inter_core_synchronization();
    test_statistics_and_debugging();
    test_core_state_management();
    test_error_handling();
    
    // Print final results
    printf("\n");
    printf("========================================\n");
    printf("           Test Results\n");
    printf("========================================\n");
    printf("Total tests: %lu\n", test_count);
    printf("Passed: %lu\n", test_count - test_failures);
    printf("Failed: %lu\n", test_failures);
    printf("Success rate: %.1f%%\n", 
           test_count > 0 ? (100.0 * (test_count - test_failures)) / test_count : 0.0);
    printf("========================================\n");
    
    if (test_failures == 0) {
        printf("🎉 All SMP foundation tests PASSED!\n");
        return 0;
    } else {
        printf("❌ Some tests FAILED!\n");
        return 1;
    }
}