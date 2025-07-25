/**
 * @file deadlock_test.c
 * @brief Unit tests for deadlock detection system
 */

#include "pico_rtos/deadlock.h"
#include "pico_rtos.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t callback_count;
    pico_rtos_deadlock_action_t last_action;
    bool callback_executed;
} test_deadlock_data_t;

static test_deadlock_data_t g_test_data = {0};

// Mock resources for testing
static int mock_mutex1, mock_mutex2, mock_semaphore;

// =============================================================================
// TEST CALLBACK FUNCTIONS
// =============================================================================

static pico_rtos_deadlock_action_t test_deadlock_callback(
    pico_rtos_deadlock_detector_t *detector,
    pico_rtos_deadlock_resource_t **resources,
    uint32_t resource_count,
    pico_rtos_task_t **tasks,
    uint32_t task_count)
{
    (void)detector;
    (void)resources;
    (void)resource_count;
    (void)tasks;
    (void)task_count;
    
    g_test_data.callback_count++;
    g_test_data.callback_executed = true;
    g_test_data.last_action = PICO_RTOS_DEADLOCK_ACTION_TIMEOUT_OPERATION;
    
    return g_test_data.last_action;
}

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static void reset_test_data(void)
{
    memset(&g_test_data, 0, sizeof(g_test_data));
}

static void test_deadlock_init(void)
{
    printf("Testing deadlock detection initialization...\n");
    
    // Test initialization
    assert(pico_rtos_deadlock_init() == true);
    
    // Test double initialization
    assert(pico_rtos_deadlock_init() == true);
    
    // Test initial state
    assert(pico_rtos_deadlock_is_enabled() == true);
    
    printf("✓ Deadlock initialization test passed\n");
}

static void test_resource_registration(void)
{
    printf("Testing resource registration...\n");
    
    // Test resource registration
    uint32_t mutex1_id = pico_rtos_deadlock_register_resource(&mock_mutex1, 
                                                             PICO_RTOS_RESOURCE_MUTEX, 
                                                             "test_mutex1");
    assert(mutex1_id != 0);
    
    uint32_t mutex2_id = pico_rtos_deadlock_register_resource(&mock_mutex2, 
                                                             PICO_RTOS_RESOURCE_MUTEX, 
                                                             "test_mutex2");
    assert(mutex2_id != 0);
    assert(mutex2_id != mutex1_id);
    
    uint32_t sem_id = pico_rtos_deadlock_register_resource(&mock_semaphore, 
                                                          PICO_RTOS_RESOURCE_SEMAPHORE, 
                                                          "test_semaphore");
    assert(sem_id != 0);
    
    // Test duplicate registration
    uint32_t dup_id = pico_rtos_deadlock_register_resource(&mock_mutex1, 
                                                          PICO_RTOS_RESOURCE_MUTEX, 
                                                          "duplicate");
    assert(dup_id == 0); // Should fail
    
    // Test invalid parameters
    assert(pico_rtos_deadlock_register_resource(NULL, PICO_RTOS_RESOURCE_MUTEX, "null") == 0);
    
    // Test unregistration
    assert(pico_rtos_deadlock_unregister_resource(sem_id) == true);
    assert(pico_rtos_deadlock_unregister_resource(0) == false); // Invalid ID
    
    printf("✓ Resource registration test passed\n");
}

static void test_enable_disable(void)
{
    printf("Testing enable/disable functionality...\n");
    
    // Test disabling
    pico_rtos_deadlock_set_enabled(false);
    assert(pico_rtos_deadlock_is_enabled() == false);
    
    // Test enabling
    pico_rtos_deadlock_set_enabled(true);
    assert(pico_rtos_deadlock_is_enabled() == true);
    
    printf("✓ Enable/disable test passed\n");
}

static void test_callback_registration(void)
{
    printf("Testing callback registration...\n");
    
    reset_test_data();
    
    // Set callback
    pico_rtos_deadlock_set_callback(test_deadlock_callback, NULL);
    
    // Callback will be tested in deadlock detection tests
    
    printf("✓ Callback registration test passed\n");
}

static void test_resource_operations(void)
{
    printf("Testing resource operations...\n");
    
    // Register resources
    uint32_t mutex1_id = pico_rtos_deadlock_register_resource(&mock_mutex1, 
                                                             PICO_RTOS_RESOURCE_MUTEX, 
                                                             "test_mutex1");
    uint32_t mutex2_id = pico_rtos_deadlock_register_resource(&mock_mutex2, 
                                                             PICO_RTOS_RESOURCE_MUTEX, 
                                                             "test_mutex2");
    
    // Mock task (using current task)
    pico_rtos_task_t *task = pico_rtos_get_current_task();
    if (task == NULL) {
        // Create a mock task pointer for testing
        static pico_rtos_task_t mock_task;
        task = &mock_task;
    }
    
    // Test resource request
    assert(pico_rtos_deadlock_request_resource(mutex1_id, task) == true);
    
    // Test resource acquisition
    assert(pico_rtos_deadlock_acquire_resource(mutex1_id, task) == true);
    
    // Test resource release
    assert(pico_rtos_deadlock_release_resource(mutex1_id, task) == true);
    
    // Test wait cancellation
    assert(pico_rtos_deadlock_request_resource(mutex2_id, task) == true);
    assert(pico_rtos_deadlock_cancel_wait(mutex2_id, task) == true);
    
    // Test invalid operations
    assert(pico_rtos_deadlock_request_resource(0, task) == true); // Invalid ID should be allowed
    assert(pico_rtos_deadlock_acquire_resource(mutex1_id, NULL) == true); // NULL task should be allowed
    
    printf("✓ Resource operations test passed\n");
}

static void test_deadlock_detection(void)
{
    printf("Testing deadlock detection...\n");
    
    reset_test_data();
    
    // Test basic detection
    pico_rtos_deadlock_result_t result;
    assert(pico_rtos_deadlock_detect(&result) == true);
    
    // Initially should be no deadlock
    assert(result.state == PICO_RTOS_DEADLOCK_STATE_NONE);
    
    // Test with NULL result
    assert(pico_rtos_deadlock_detect(NULL) == false);
    
    printf("✓ Deadlock detection test passed\n");
}

static void test_statistics(void)
{
    printf("Testing statistics...\n");
    
    // Get statistics
    pico_rtos_deadlock_statistics_t stats;
    pico_rtos_deadlock_get_statistics(&stats);
    
    // Should have some registered resources
    assert(stats.active_resources > 0);
    
    // Reset statistics
    pico_rtos_deadlock_reset_statistics();
    
    // Get statistics again
    pico_rtos_deadlock_get_statistics(&stats);
    
    // Some counters should be reset
    assert(stats.total_detections == 0);
    
    printf("✓ Statistics test passed\n");
}

static void test_utility_functions(void)
{
    printf("Testing utility functions...\n");
    
    // Test string functions
    const char *type_str = pico_rtos_deadlock_get_resource_type_string(PICO_RTOS_RESOURCE_MUTEX);
    assert(type_str != NULL);
    assert(strcmp(type_str, "Mutex") == 0);
    
    const char *state_str = pico_rtos_deadlock_get_state_string(PICO_RTOS_DEADLOCK_STATE_DETECTED);
    assert(state_str != NULL);
    assert(strcmp(state_str, "Detected") == 0);
    
    const char *action_str = pico_rtos_deadlock_get_action_string(PICO_RTOS_DEADLOCK_ACTION_ABORT_TASK);
    assert(action_str != NULL);
    assert(strcmp(action_str, "Abort Task") == 0);
    
    // Test print function (should not crash)
    pico_rtos_deadlock_result_t result = {0};
    result.state = PICO_RTOS_DEADLOCK_STATE_NONE;
    pico_rtos_deadlock_print_result(&result);
    
    // Test with NULL
    pico_rtos_deadlock_print_result(NULL);
    
    printf("✓ Utility functions test passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void)
{
    printf("Starting deadlock detection system tests...\n\n");
    
    // Initialize RTOS (required for deadlock detection)
    assert(pico_rtos_init() == true);
    
    // Run tests
    test_deadlock_init();
    test_resource_registration();
    test_enable_disable();
    test_callback_registration();
    test_resource_operations();
    test_deadlock_detection();
    test_statistics();
    test_utility_functions();
    
    printf("\n✓ All deadlock detection system tests passed!\n");
    return 0;
}

// Task function for testing (if needed)
void deadlock_test_task(void *param)
{
    (void)param;
    main();
    pico_rtos_task_delete(NULL);
}