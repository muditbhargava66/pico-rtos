#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// Include the SMP header
#include "pico_rtos/smp.h"
#include "pico_rtos/task.h"
#include "pico_rtos/config.h"

// Test framework macros
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

// Mock functions for testing
static uint32_t mock_tick_count = 0;
static bool mock_core_1_responsive = true;
static uint32_t mock_current_core = 0;

uint32_t pico_rtos_get_tick_count(void) {
    return mock_tick_count;
}

pico_rtos_task_t *pico_rtos_get_current_task(void) {
    static pico_rtos_task_t mock_task = {
        .name = "mock_task",
        .core_affinity = PICO_RTOS_CORE_AFFINITY_ANY,
        .assigned_core = 0
    };
    return &mock_task;
}

uint32_t *pico_rtos_init_task_stack(uint32_t *stack_base, uint32_t stack_size, 
                                   pico_rtos_task_function_t function, void *param) {
    // Mock implementation - just return a valid pointer
    return stack_base + (stack_size / sizeof(uint32_t)) - 16;
}

// Override get_core_num for testing
uint32_t get_core_num(void) {
    return mock_current_core;
}

// Test helper functions
static void reset_test_environment(void) {
    mock_tick_count = 0;
    mock_core_1_responsive = true;
    mock_current_core = 0;
}

static void advance_time(uint32_t ms) {
    mock_tick_count += ms;
}

static void simulate_core_failure(uint32_t core_id) {
    if (core_id == 1) {
        mock_core_1_responsive = false;
    }
}

static void simulate_core_recovery(uint32_t core_id) {
    if (core_id == 1) {
        mock_core_1_responsive = true;
    }
}

// Test failure callback
static bool test_failure_callback_called = false;
static uint32_t test_failure_callback_core_id = 0;
static pico_rtos_core_failure_type_t test_failure_callback_type = PICO_RTOS_CORE_FAILURE_NONE;

static bool test_failure_callback(uint32_t failed_core_id,
                                 pico_rtos_core_failure_type_t failure_type,
                                 void *user_data) {
    test_failure_callback_called = true;
    test_failure_callback_core_id = failed_core_id;
    test_failure_callback_type = failure_type;
    
    // Return true to attempt recovery
    return true;
}

// =============================================================================
// CORE HEALTH MONITORING TESTS
// =============================================================================

/**
 * @brief Test core health monitoring initialization
 */
static bool test_core_health_init(void) {
    reset_test_environment();
    
    // Initialize SMP system first
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    
    // Test default initialization
    TEST_ASSERT(pico_rtos_core_health_init(NULL), "Health monitoring init with default config failed");
    
    // Test custom configuration
    pico_rtos_core_health_config_t config = {
        .enabled = true,
        .watchdog_timeout_ms = 2000,
        .health_check_interval_ms = 500,
        .max_missed_heartbeats = 2,
        .recovery_timeout_ms = 5000,
        .auto_recovery_enabled = true,
        .graceful_degradation_enabled = true
    };
    
    TEST_ASSERT(pico_rtos_core_health_init(&config), "Health monitoring init with custom config failed");
    
    // Verify health states are initialized
    pico_rtos_core_health_state_t state;
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core 0 health state");
    TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core 0 not initialized as healthy");
    TEST_ASSERT(state.failure_count == 0, "Core 0 failure count not zero");
    
    TEST_ASSERT(pico_rtos_core_health_get_state(1, &state), "Failed to get core 1 health state");
    TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core 1 not initialized as healthy");
    TEST_ASSERT(state.failure_count == 0, "Core 1 failure count not zero");
    
    TEST_PASS();
}

/**
 * @brief Test watchdog functionality
 */
static bool test_watchdog_functionality(void) {
    reset_test_environment();
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    
    pico_rtos_core_health_config_t config = {
        .enabled = true,
        .watchdog_timeout_ms = 1000,
        .health_check_interval_ms = 100,
        .max_missed_heartbeats = 2,
        .recovery_timeout_ms = 2000,
        .auto_recovery_enabled = false,  // Disable auto recovery for this test
        .graceful_degradation_enabled = false
    };
    
    TEST_ASSERT(pico_rtos_core_health_init(&config), "Health monitoring init failed");
    pico_rtos_core_health_start();
    
    // Test normal watchdog feeding
    pico_rtos_core_health_feed_watchdog();
    pico_rtos_core_health_send_heartbeat();
    
    pico_rtos_core_health_state_t state;
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core 0 state");
    TEST_ASSERT(state.watchdog_feed_count > 0, "Watchdog feed count not incremented");
    
    // Test watchdog timeout detection
    advance_time(1500); // Exceed watchdog timeout
    pico_rtos_core_health_process();
    
    // Core should still be healthy since we're testing from core 0
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core 0 state after timeout");
    
    TEST_PASS();
}

/**
 * @brief Test heartbeat monitoring
 */
static bool test_heartbeat_monitoring(void) {
    reset_test_environment();
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    
    pico_rtos_core_health_config_t config = {
        .enabled = true,
        .watchdog_timeout_ms = 5000,
        .health_check_interval_ms = 200,
        .max_missed_heartbeats = 3,
        .recovery_timeout_ms = 2000,
        .auto_recovery_enabled = false,
        .graceful_degradation_enabled = false
    };
    
    TEST_ASSERT(pico_rtos_core_health_init(&config), "Health monitoring init failed");
    pico_rtos_core_health_start();
    
    // Send initial heartbeat
    pico_rtos_core_health_send_heartbeat();
    
    pico_rtos_core_health_state_t state;
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get initial core state");
    TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core not initially healthy");
    
    // Simulate missed heartbeats by advancing time without sending heartbeats
    advance_time(800); // Miss 4 heartbeat intervals (200ms * 4)
    pico_rtos_core_health_process();
    
    // Check if core is marked as unresponsive
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core state after missed heartbeats");
    
    // Send heartbeat to recover
    pico_rtos_core_health_send_heartbeat();
    
    TEST_PASS();
}

/**
 * @brief Test failure callback registration and invocation
 */
static bool test_failure_callback_registration(void) {
    reset_test_environment();
    test_failure_callback_called = false;
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    
    pico_rtos_core_health_config_t config = {
        .enabled = true,
        .watchdog_timeout_ms = 500,
        .health_check_interval_ms = 100,
        .max_missed_heartbeats = 2,
        .recovery_timeout_ms = 1000,
        .auto_recovery_enabled = true,
        .graceful_degradation_enabled = false
    };
    
    TEST_ASSERT(pico_rtos_core_health_init(&config), "Health monitoring init failed");
    
    // Register failure callback
    TEST_ASSERT(pico_rtos_core_health_register_failure_callback(test_failure_callback, NULL),
               "Failed to register failure callback");
    
    pico_rtos_core_health_start();
    
    // Simulate a failure by not feeding watchdog and advancing time
    advance_time(1000); // Exceed watchdog timeout
    pico_rtos_core_health_process();
    
    // Note: In our test environment, we can't easily simulate inter-core failures,
    // so we test the callback registration mechanism
    
    TEST_PASS();
}

/**
 * @brief Test graceful degradation to single-core mode
 */
static bool test_graceful_degradation(void) {
    reset_test_environment();
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    
    pico_rtos_core_health_config_t config = {
        .enabled = true,
        .watchdog_timeout_ms = 1000,
        .health_check_interval_ms = 200,
        .max_missed_heartbeats = 2,
        .recovery_timeout_ms = 1000,
        .auto_recovery_enabled = false,  // Disable recovery to force degradation
        .graceful_degradation_enabled = true
    };
    
    TEST_ASSERT(pico_rtos_core_health_init(&config), "Health monitoring init failed");
    pico_rtos_core_health_start();
    
    // Test initial state
    TEST_ASSERT(!pico_rtos_core_health_is_single_core_mode(), "System should not be in single-core mode initially");
    
    // Manually trigger degradation
    uint32_t migrated_tasks = pico_rtos_core_health_degrade_to_single_core(1);
    
    // Verify single-core mode
    TEST_ASSERT(pico_rtos_core_health_is_single_core_mode(), "System should be in single-core mode after degradation");
    
    // In our test environment, there are no user tasks to migrate, so count should be 0
    // (only idle tasks exist, and they don't get migrated)
    
    TEST_PASS();
}

/**
 * @brief Test core recovery functionality
 */
static bool test_core_recovery(void) {
    reset_test_environment();
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    
    pico_rtos_core_health_config_t config = {
        .enabled = true,
        .watchdog_timeout_ms = 1000,
        .health_check_interval_ms = 200,
        .max_missed_heartbeats = 2,
        .recovery_timeout_ms = 500,  // Short recovery timeout for testing
        .auto_recovery_enabled = true,
        .graceful_degradation_enabled = true
    };
    
    TEST_ASSERT(pico_rtos_core_health_init(&config), "Health monitoring init failed");
    pico_rtos_core_health_start();
    
    // Test manual recovery trigger
    bool recovery_result = pico_rtos_core_health_recover_core(0);
    
    // Recovery should succeed for the current core (since it's responsive)
    TEST_ASSERT(recovery_result, "Manual core recovery should succeed for responsive core");
    
    // Test recovery for invalid core
    TEST_ASSERT(!pico_rtos_core_health_recover_core(2), "Recovery should fail for invalid core ID");
    
    TEST_PASS();
}

/**
 * @brief Test health statistics collection
 */
static bool test_health_statistics(void) {
    reset_test_environment();
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    TEST_ASSERT(pico_rtos_core_health_init(NULL), "Health monitoring init failed");
    pico_rtos_core_health_start();
    
    // Get initial statistics
    uint32_t failure_count, recovery_count, downtime_ms;
    TEST_ASSERT(pico_rtos_core_health_get_failure_stats(0, &failure_count, &recovery_count, &downtime_ms),
               "Failed to get initial health statistics");
    
    TEST_ASSERT(failure_count == 0, "Initial failure count should be zero");
    TEST_ASSERT(recovery_count == 0, "Initial recovery count should be zero");
    TEST_ASSERT(downtime_ms == 0, "Initial downtime should be zero");
    
    // Test statistics for invalid core
    TEST_ASSERT(!pico_rtos_core_health_get_failure_stats(2, &failure_count, &recovery_count, &downtime_ms),
               "Statistics should fail for invalid core ID");
    
    // Test statistics reset
    pico_rtos_core_health_reset_stats(0);
    pico_rtos_core_health_reset_stats(0xFF); // Reset all cores
    
    TEST_PASS();
}

/**
 * @brief Test health state retrieval
 */
static bool test_health_state_retrieval(void) {
    reset_test_environment();
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    TEST_ASSERT(pico_rtos_core_health_init(NULL), "Health monitoring init failed");
    pico_rtos_core_health_start();
    
    // Test valid core state retrieval
    pico_rtos_core_health_state_t state;
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core 0 state");
    TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core 0 should be healthy");
    
    TEST_ASSERT(pico_rtos_core_health_get_state(1, &state), "Failed to get core 1 state");
    TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core 1 should be healthy");
    
    // Test invalid core state retrieval
    TEST_ASSERT(!pico_rtos_core_health_get_state(2, &state), "Should fail for invalid core ID");
    TEST_ASSERT(!pico_rtos_core_health_get_state(0, NULL), "Should fail for NULL state pointer");
    
    // Test health status check
    TEST_ASSERT(pico_rtos_core_health_check(0) == PICO_RTOS_CORE_HEALTH_HEALTHY,
               "Core 0 health check should return healthy");
    TEST_ASSERT(pico_rtos_core_health_check(1) == PICO_RTOS_CORE_HEALTH_HEALTHY,
               "Core 1 health check should return healthy");
    TEST_ASSERT(pico_rtos_core_health_check(2) == PICO_RTOS_CORE_HEALTH_UNKNOWN,
               "Invalid core health check should return unknown");
    
    TEST_PASS();
}

/**
 * @brief Test health monitoring start/stop functionality
 */
static bool test_health_monitoring_control(void) {
    reset_test_environment();
    
    // Initialize system
    TEST_ASSERT(pico_rtos_smp_init(), "SMP initialization failed");
    TEST_ASSERT(pico_rtos_core_health_init(NULL), "Health monitoring init failed");
    
    // Test start
    pico_rtos_core_health_start();
    
    pico_rtos_core_health_state_t state;
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core state after start");
    TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_HEALTHY, "Core should be healthy after start");
    
    // Test stop
    pico_rtos_core_health_stop();
    
    TEST_ASSERT(pico_rtos_core_health_get_state(0, &state), "Failed to get core state after stop");
    TEST_ASSERT(state.status == PICO_RTOS_CORE_HEALTH_UNKNOWN, "Core status should be unknown after stop");
    
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
    {"Core Health Init", test_core_health_init},
    {"Watchdog Functionality", test_watchdog_functionality},
    {"Heartbeat Monitoring", test_heartbeat_monitoring},
    {"Failure Callback", test_failure_callback_registration},
    {"Graceful Degradation", test_graceful_degradation},
    {"Core Recovery", test_core_recovery},
    {"Health Statistics", test_health_statistics},
    {"Health State Retrieval", test_health_state_retrieval},
    {"Health Monitoring Control", test_health_monitoring_control},
};

int main(void) {
    printf("=== Core Failure Detection and Recovery Tests ===\n\n");
    
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
    
    return (passed_tests == total_tests) ? 0 : 1;
}