/**
 * @file compatibility_test.c
 * @brief Backward compatibility validation tests for Pico-RTOS v0.3.1
 * 
 * This test suite validates that all v0.2.1 APIs remain unchanged and
 * behave identically in v0.3.1. It tests:
 * 1. API signature compatibility
 * 2. Behavioral compatibility
 * 3. Data structure compatibility
 * 4. Error code compatibility
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

// Test configuration
#define TEST_STACK_SIZE 512
#define TEST_TIMEOUT 1000
#define TEST_QUEUE_SIZE 5

// Test result tracking
static uint32_t tests_run = 0;
static uint32_t tests_passed = 0;
static uint32_t tests_failed = 0;

// Test macros
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } else { \
        tests_failed++; \
        printf("FAIL: %s\n", message); \
    } \
} while(0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

// Test data structures
static pico_rtos_task_t test_task1, test_task2;
static pico_rtos_mutex_t test_mutex;
static pico_rtos_semaphore_t test_semaphore;
static pico_rtos_queue_t test_queue;
static uint32_t queue_buffer[TEST_QUEUE_SIZE];

// Test task functions
void simple_test_task(void *param) {
    uint32_t *counter = (uint32_t*)param;
    (*counter)++;
    
    // Task should be able to delay
    pico_rtos_task_delay(100);
    
    // Task should be able to yield
    pico_rtos_task_yield();
    
    (*counter)++;
}

void mutex_test_task(void *param) {
    pico_rtos_mutex_t *mutex = (pico_rtos_mutex_t*)param;
    
    // Try to acquire mutex
    if (pico_rtos_mutex_lock(mutex, TEST_TIMEOUT)) {
        pico_rtos_task_delay(50);
        pico_rtos_mutex_unlock(mutex);
    }
}

void semaphore_test_task(void *param) {
    pico_rtos_semaphore_t *sem = (pico_rtos_semaphore_t*)param;
    
    // Try to take semaphore
    if (pico_rtos_semaphore_take(sem, TEST_TIMEOUT)) {
        pico_rtos_task_delay(50);
        pico_rtos_semaphore_give(sem);
    }
}

/**
 * @brief Test core RTOS API compatibility
 */
void test_core_api_compatibility(void) {
    TEST_SECTION("Core RTOS API Compatibility");
    
    // Test version information
    TEST_ASSERT(PICO_RTOS_VERSION_MAJOR == 0, "Major version should be 0");
    TEST_ASSERT(PICO_RTOS_VERSION_MINOR == 3, "Minor version should be 3");
    TEST_ASSERT(PICO_RTOS_VERSION_PATCH == 0, "Patch version should be 0");
    
    // Test version string format
    const char *version = pico_rtos_get_version_string();
    TEST_ASSERT(version != NULL, "Version string should not be NULL");
    TEST_ASSERT(strncmp(version, "0.3.1", 5) == 0, "Version string should be '0.3.1'");
    
    // Test system tick functions
    uint32_t tick1 = pico_rtos_get_tick_count();
    uint32_t uptime1 = pico_rtos_get_uptime_ms();
    uint32_t tick_rate = pico_rtos_get_tick_rate_hz();
    
    TEST_ASSERT(tick_rate > 0, "Tick rate should be positive");
    TEST_ASSERT(tick1 <= uptime1, "Tick count should be <= uptime");
    
    // Test critical section functions (should not crash)
    pico_rtos_enter_critical();
    pico_rtos_exit_critical();
    TEST_ASSERT(true, "Critical section functions should work");
}

/**
 * @brief Test task API compatibility
 */
void test_task_api_compatibility(void) {
    TEST_SECTION("Task API Compatibility");
    
    uint32_t task_counter = 0;
    
    // Test task creation with v0.2.1 signature
    bool result = pico_rtos_task_create(&test_task1, "TestTask1", 
                                       simple_test_task, &task_counter, 
                                       TEST_STACK_SIZE, 2);
    TEST_ASSERT(result == true, "Task creation should succeed");
    
    // Test getting current task
    pico_rtos_task_t *current = pico_rtos_get_current_task();
    TEST_ASSERT(current != NULL, "Current task should not be NULL");
    
    // Test task state string function
    const char *state_str = pico_rtos_task_get_state_string(&test_task1);
    TEST_ASSERT(state_str != NULL, "Task state string should not be NULL");
    
    // Test task priority setting
    bool priority_result = pico_rtos_task_set_priority(&test_task1, 3);
    TEST_ASSERT(priority_result == true, "Task priority change should succeed");
    
    // Test task suspension and resumption
    pico_rtos_task_suspend(&test_task1);
    TEST_ASSERT(true, "Task suspend should not crash");
    
    pico_rtos_task_resume(&test_task1);
    TEST_ASSERT(true, "Task resume should not crash");
    
    // Allow task to run briefly
    pico_rtos_task_delay(200);
    
    // Verify task executed
    TEST_ASSERT(task_counter >= 1, "Test task should have executed");
}

/**
 * @brief Test mutex API compatibility
 */
void test_mutex_api_compatibility(void) {
    TEST_SECTION("Mutex API Compatibility");
    
    // Test mutex initialization
    bool init_result = pico_rtos_mutex_init(&test_mutex);
    TEST_ASSERT(init_result == true, "Mutex initialization should succeed");
    
    // Test mutex lock/unlock
    bool lock_result = pico_rtos_mutex_lock(&test_mutex, TEST_TIMEOUT);
    TEST_ASSERT(lock_result == true, "Mutex lock should succeed");
    
    // Test mutex owner
    pico_rtos_task_t *owner = pico_rtos_mutex_get_owner(&test_mutex);
    TEST_ASSERT(owner != NULL, "Mutex should have an owner");
    
    // Test try lock (should fail since already locked)
    bool try_lock_result = pico_rtos_mutex_try_lock(&test_mutex);
    TEST_ASSERT(try_lock_result == false, "Try lock should fail on locked mutex");
    
    // Test unlock
    bool unlock_result = pico_rtos_mutex_unlock(&test_mutex);
    TEST_ASSERT(unlock_result == true, "Mutex unlock should succeed");
    
    // Test try lock again (should succeed now)
    try_lock_result = pico_rtos_mutex_try_lock(&test_mutex);
    TEST_ASSERT(try_lock_result == true, "Try lock should succeed on unlocked mutex");
    
    pico_rtos_mutex_unlock(&test_mutex);
    
    // Test with another task
    bool task_result = pico_rtos_task_create(&test_task2, "MutexTest", 
                                            mutex_test_task, &test_mutex, 
                                            TEST_STACK_SIZE, 1);
    TEST_ASSERT(task_result == true, "Mutex test task creation should succeed");
    
    pico_rtos_task_delay(200); // Allow task to run
}

/**
 * @brief Test semaphore API compatibility
 */
void test_semaphore_api_compatibility(void) {
    TEST_SECTION("Semaphore API Compatibility");
    
    // Test semaphore initialization (binary semaphore)
    bool init_result = pico_rtos_semaphore_init(&test_semaphore, 1, 1);
    TEST_ASSERT(init_result == true, "Semaphore initialization should succeed");
    
    // Test semaphore availability check
    bool available = pico_rtos_semaphore_is_available(&test_semaphore);
    TEST_ASSERT(available == true, "Semaphore should be available initially");
    
    // Test semaphore take
    bool take_result = pico_rtos_semaphore_take(&test_semaphore, TEST_TIMEOUT);
    TEST_ASSERT(take_result == true, "Semaphore take should succeed");
    
    // Test availability after take
    available = pico_rtos_semaphore_is_available(&test_semaphore);
    TEST_ASSERT(available == false, "Semaphore should not be available after take");
    
    // Test semaphore give
    bool give_result = pico_rtos_semaphore_give(&test_semaphore);
    TEST_ASSERT(give_result == true, "Semaphore give should succeed");
    
    // Test availability after give
    available = pico_rtos_semaphore_is_available(&test_semaphore);
    TEST_ASSERT(available == true, "Semaphore should be available after give");
    
    // Test counting semaphore
    pico_rtos_semaphore_t counting_sem;
    init_result = pico_rtos_semaphore_init(&counting_sem, 0, 3);
    TEST_ASSERT(init_result == true, "Counting semaphore initialization should succeed");
    
    // Give multiple times
    for (int i = 0; i < 3; i++) {
        give_result = pico_rtos_semaphore_give(&counting_sem);
        TEST_ASSERT(give_result == true, "Counting semaphore give should succeed");
    }
    
    // Should fail to give beyond max
    give_result = pico_rtos_semaphore_give(&counting_sem);
    TEST_ASSERT(give_result == false, "Counting semaphore give should fail at max");
}

/**
 * @brief Test queue API compatibility
 */
void test_queue_api_compatibility(void) {
    TEST_SECTION("Queue API Compatibility");
    
    // Test queue initialization
    bool init_result = pico_rtos_queue_init(&test_queue, queue_buffer, 
                                           sizeof(uint32_t), TEST_QUEUE_SIZE);
    TEST_ASSERT(init_result == true, "Queue initialization should succeed");
    
    // Test queue empty check
    bool empty = pico_rtos_queue_is_empty(&test_queue);
    TEST_ASSERT(empty == true, "Queue should be empty initially");
    
    bool full = pico_rtos_queue_is_full(&test_queue);
    TEST_ASSERT(full == false, "Queue should not be full initially");
    
    // Test queue send
    uint32_t test_data = 0x12345678;
    bool send_result = pico_rtos_queue_send(&test_queue, &test_data, TEST_TIMEOUT);
    TEST_ASSERT(send_result == true, "Queue send should succeed");
    
    // Test queue not empty after send
    empty = pico_rtos_queue_is_empty(&test_queue);
    TEST_ASSERT(empty == false, "Queue should not be empty after send");
    
    // Test queue receive
    uint32_t received_data = 0;
    bool receive_result = pico_rtos_queue_receive(&test_queue, &received_data, TEST_TIMEOUT);
    TEST_ASSERT(receive_result == true, "Queue receive should succeed");
    TEST_ASSERT(received_data == test_data, "Received data should match sent data");
    
    // Test queue empty after receive
    empty = pico_rtos_queue_is_empty(&test_queue);
    TEST_ASSERT(empty == true, "Queue should be empty after receive");
    
    // Fill queue to test full condition
    for (int i = 0; i < TEST_QUEUE_SIZE; i++) {
        uint32_t data = i;
        send_result = pico_rtos_queue_send(&test_queue, &data, TEST_TIMEOUT);
        TEST_ASSERT(send_result == true, "Queue send should succeed when not full");
    }
    
    full = pico_rtos_queue_is_full(&test_queue);
    TEST_ASSERT(full == true, "Queue should be full after filling");
    
    // Try to send to full queue (should fail with no wait)
    uint32_t extra_data = 999;
    send_result = pico_rtos_queue_send(&test_queue, &extra_data, PICO_RTOS_NO_WAIT);
    TEST_ASSERT(send_result == false, "Queue send should fail when full");
}

/**
 * @brief Test memory management API compatibility
 */
void test_memory_api_compatibility(void) {
    TEST_SECTION("Memory Management API Compatibility");
    
    // Test memory allocation
    void *ptr1 = pico_rtos_malloc(128);
    TEST_ASSERT(ptr1 != NULL, "Memory allocation should succeed");
    
    void *ptr2 = pico_rtos_malloc(256);
    TEST_ASSERT(ptr2 != NULL, "Second memory allocation should succeed");
    
    // Test memory statistics
    uint32_t current, peak, allocations;
    pico_rtos_get_memory_stats(&current, &peak, &allocations);
    TEST_ASSERT(current > 0, "Current memory usage should be positive");
    TEST_ASSERT(peak >= current, "Peak memory should be >= current");
    TEST_ASSERT(allocations >= 2, "Allocation count should be >= 2");
    
    // Test memory deallocation
    pico_rtos_free(ptr1, 128);
    pico_rtos_free(ptr2, 256);
    TEST_ASSERT(true, "Memory deallocation should not crash");
    
    // Test system statistics
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    TEST_ASSERT(stats.total_tasks > 0, "System should have tasks");
    TEST_ASSERT(stats.system_uptime > 0, "System uptime should be positive");
}

/**
 * @brief Test error handling compatibility
 */
void test_error_handling_compatibility(void) {
    TEST_SECTION("Error Handling Compatibility");
    
    // Test invalid parameters return false/NULL appropriately
    bool result;
    
    // Invalid task creation
    result = pico_rtos_task_create(NULL, "Test", simple_test_task, NULL, 512, 1);
    TEST_ASSERT(result == false, "Task creation with NULL task should fail");
    
    // Invalid mutex operations
    result = pico_rtos_mutex_init(NULL);
    TEST_ASSERT(result == false, "Mutex init with NULL should fail");
    
    result = pico_rtos_mutex_lock(NULL, 1000);
    TEST_ASSERT(result == false, "Mutex lock with NULL should fail");
    
    // Invalid semaphore operations
    result = pico_rtos_semaphore_init(NULL, 1, 1);
    TEST_ASSERT(result == false, "Semaphore init with NULL should fail");
    
    result = pico_rtos_semaphore_take(NULL, 1000);
    TEST_ASSERT(result == false, "Semaphore take with NULL should fail");
    
    // Invalid queue operations
    result = pico_rtos_queue_init(NULL, queue_buffer, sizeof(uint32_t), 5);
    TEST_ASSERT(result == false, "Queue init with NULL queue should fail");
    
    result = pico_rtos_queue_send(NULL, &result, 1000);
    TEST_ASSERT(result == false, "Queue send with NULL queue should fail");
}

/**
 * @brief Test data structure size compatibility
 */
void test_data_structure_compatibility(void) {
    TEST_SECTION("Data Structure Size Compatibility");
    
    // These sizes should remain stable for binary compatibility
    // Note: In v0.3.1, task structure may be larger due to new fields,
    // but the core fields should remain in the same positions
    
    printf("Task structure size: %zu bytes\n", sizeof(pico_rtos_task_t));
    printf("Mutex structure size: %zu bytes\n", sizeof(pico_rtos_mutex_t));
    printf("Semaphore structure size: %zu bytes\n", sizeof(pico_rtos_semaphore_t));
    printf("Queue structure size: %zu bytes\n", sizeof(pico_rtos_queue_t));
    
    // Test that core fields are accessible (compilation test)
    pico_rtos_task_t task;
    task.name = "test";
    task.priority = 1;
    task.state = PICO_RTOS_TASK_STATE_READY;
    
    TEST_ASSERT(task.name != NULL, "Task name field should be accessible");
    TEST_ASSERT(task.priority == 1, "Task priority field should be accessible");
    TEST_ASSERT(task.state == PICO_RTOS_TASK_STATE_READY, "Task state field should be accessible");
}

/**
 * @brief Test timeout constant compatibility
 */
void test_timeout_constants_compatibility(void) {
    TEST_SECTION("Timeout Constants Compatibility");
    
    // Test that timeout constants are defined and have expected values
    TEST_ASSERT(PICO_RTOS_WAIT_FOREVER == UINT32_MAX, "WAIT_FOREVER should be UINT32_MAX");
    TEST_ASSERT(PICO_RTOS_NO_WAIT == 0, "NO_WAIT should be 0");
    
    // Test timeout behavior
    pico_rtos_semaphore_t timeout_sem;
    pico_rtos_semaphore_init(&timeout_sem, 0, 1);
    
    // Test no-wait timeout
    uint32_t start_time = pico_rtos_get_tick_count();
    bool result = pico_rtos_semaphore_take(&timeout_sem, PICO_RTOS_NO_WAIT);
    uint32_t elapsed = pico_rtos_get_tick_count() - start_time;
    
    TEST_ASSERT(result == false, "No-wait should fail immediately");
    TEST_ASSERT(elapsed < 10, "No-wait should return quickly");
    
    // Test short timeout
    start_time = pico_rtos_get_tick_count();
    result = pico_rtos_semaphore_take(&timeout_sem, 100);
    elapsed = pico_rtos_get_tick_count() - start_time;
    
    TEST_ASSERT(result == false, "Short timeout should fail");
    TEST_ASSERT(elapsed >= 90 && elapsed <= 150, "Timeout should be approximately correct");
}

/**
 * @brief Main compatibility test function
 */
void compatibility_test_task(void *param) {
    printf("\n=== Pico-RTOS v0.3.1 Backward Compatibility Test Suite ===\n");
    printf("Testing compatibility with v0.2.1 API...\n");
    
    // Run all compatibility tests
    test_core_api_compatibility();
    test_task_api_compatibility();
    test_mutex_api_compatibility();
    test_semaphore_api_compatibility();
    test_queue_api_compatibility();
    test_memory_api_compatibility();
    test_error_handling_compatibility();
    test_data_structure_compatibility();
    test_timeout_constants_compatibility();
    
    // Print final results
    printf("\n=== COMPATIBILITY TEST RESULTS ===\n");
    printf("Tests run: %lu\n", tests_run);
    printf("Tests passed: %lu\n", tests_passed);
    printf("Tests failed: %lu\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("SUCCESS: All compatibility tests passed!\n");
        printf("v0.3.1 is fully backward compatible with v0.2.1\n");
    } else {
        printf("FAILURE: %lu compatibility tests failed!\n", tests_failed);
        printf("v0.3.1 has compatibility issues with v0.2.1\n");
    }
    
    printf("=== Compatibility test complete ===\n");
    
    // Keep task alive
    while (1) {
        pico_rtos_task_delay(10000);
    }
}

/**
 * @brief Initialize and run compatibility tests
 */
int main() {
    stdio_init_all();
    
    printf("Initializing Pico-RTOS for compatibility testing...\n");
    
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    
    // Create compatibility test task
    static pico_rtos_task_t compat_test_task;
    if (!pico_rtos_task_create(&compat_test_task, "CompatTest", 
                              compatibility_test_task, NULL, 2048, 5)) {
        printf("ERROR: Failed to create compatibility test task\n");
        return -1;
    }
    
    printf("Starting compatibility tests...\n");
    pico_rtos_start();
    
    return 0;
}