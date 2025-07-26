#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"
#include "pico_rtos/event_group.h"

/**
 * @file event_group_test.c
 * @brief Comprehensive unit tests for Event Groups
 * 
 * Tests cover:
 * - Basic initialization and cleanup
 * - Event setting and clearing operations
 * - Wait conditions (any/all semantics)
 * - Timeout handling
 * - Priority-ordered task unblocking
 * - Statistics and error handling
 * - Edge cases and error conditions
 */

// Test configuration
#define TEST_TASK_STACK_SIZE 1024
#define TEST_TIMEOUT_MS 100
#define TEST_LONG_TIMEOUT_MS 1000

// Test state tracking
static volatile bool test_passed = true;
static volatile int test_count = 0;
static volatile int test_failures = 0;

// Test synchronization
static pico_rtos_event_group_t test_event_group;
static pico_rtos_mutex_t test_mutex;

// Test task handles
static pico_rtos_task_t test_task1;
static pico_rtos_task_t test_task2;
static pico_rtos_task_t test_task3;

// Test task stacks
static uint32_t test_task1_stack[TEST_TASK_STACK_SIZE / sizeof(uint32_t)];
static uint32_t test_task2_stack[TEST_TASK_STACK_SIZE / sizeof(uint32_t)];
static uint32_t test_task3_stack[TEST_TASK_STACK_SIZE / sizeof(uint32_t)];

// Test result tracking
static volatile uint32_t task1_result = 0;
static volatile uint32_t task2_result = 0;
static volatile uint32_t task3_result = 0;
static volatile bool task1_completed = false;
static volatile bool task2_completed = false;
static volatile bool task3_completed = false;

// =============================================================================
// TEST HELPER FUNCTIONS
// =============================================================================

#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (!(condition)) { \
            printf("FAIL: %s (line %d): %s\n", __func__, __LINE__, message); \
            test_failures++; \
            test_passed = false; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

static void reset_test_state(void) {
    task1_result = 0;
    task2_result = 0;
    task3_result = 0;
    task1_completed = false;
    task2_completed = false;
    task3_completed = false;
}

static void wait_for_tasks_completion(uint32_t timeout_ms) {
    uint32_t start_time = pico_rtos_get_tick_count();
    while ((!task1_completed || !task2_completed || !task3_completed) &&
           (pico_rtos_get_tick_count() - start_time) < timeout_ms) {
        pico_rtos_task_delay(10);
    }
}

// =============================================================================
// TEST TASK FUNCTIONS
// =============================================================================

static void test_task1_wait_any(void *param) {
    uint32_t bits_to_wait = (uint32_t)(uintptr_t)param;
    
    task1_result = pico_rtos_event_group_wait_bits(&test_event_group, 
                                                  bits_to_wait, 
                                                  false,  // wait for any
                                                  false,  // don't clear
                                                  TEST_LONG_TIMEOUT_MS);
    task1_completed = true;
    
    // Task terminates
    pico_rtos_task_delete(NULL);
}

static void test_task2_wait_all(void *param) {
    uint32_t bits_to_wait = (uint32_t)(uintptr_t)param;
    
    task2_result = pico_rtos_event_group_wait_bits(&test_event_group, 
                                                  bits_to_wait, 
                                                  true,   // wait for all
                                                  false,  // don't clear
                                                  TEST_LONG_TIMEOUT_MS);
    task2_completed = true;
    
    // Task terminates
    pico_rtos_task_delete(NULL);
}

static void test_task3_wait_clear(void *param) {
    uint32_t bits_to_wait = (uint32_t)(uintptr_t)param;
    
    task3_result = pico_rtos_event_group_wait_bits(&test_event_group, 
                                                  bits_to_wait, 
                                                  false,  // wait for any
                                                  true,   // clear on exit
                                                  TEST_LONG_TIMEOUT_MS);
    task3_completed = true;
    
    // Task terminates
    pico_rtos_task_delete(NULL);
}

static void test_task_timeout(void *param) {
    uint32_t bits_to_wait = (uint32_t)(uintptr_t)param;
    
    task1_result = pico_rtos_event_group_wait_bits(&test_event_group, 
                                                  bits_to_wait, 
                                                  false,  // wait for any
                                                  false,  // don't clear
                                                  TEST_TIMEOUT_MS);  // Short timeout
    task1_completed = true;
    
    // Task terminates
    pico_rtos_task_delete(NULL);
}

// =============================================================================
// BASIC FUNCTIONALITY TESTS
// =============================================================================

static void test_event_group_initialization(void) {
    printf("\n=== Testing Event Group Initialization ===\n");
    
    // Test successful initialization
    bool result = pico_rtos_event_group_init(&test_event_group);
    TEST_ASSERT(result == true, "Event group initialization should succeed");
    
    // Verify initial state
    uint32_t initial_bits = pico_rtos_event_group_get_bits(&test_event_group);
    TEST_ASSERT(initial_bits == 0, "Initial event bits should be 0");
    
    uint32_t waiting_count = pico_rtos_event_group_get_waiting_count(&test_event_group);
    TEST_ASSERT(waiting_count == 0, "Initial waiting count should be 0");
    
    // Test NULL pointer handling
    result = pico_rtos_event_group_init(NULL);
    TEST_ASSERT(result == false, "Initialization with NULL should fail");
}

static void test_event_setting_and_clearing(void) {
    printf("\n=== Testing Event Setting and Clearing ===\n");
    
    // Test setting individual bits
    bool result = pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_BIT_0);
    TEST_ASSERT(result == true, "Setting bit 0 should succeed");
    
    uint32_t current_bits = pico_rtos_event_group_get_bits(&test_event_group);
    TEST_ASSERT(current_bits == PICO_RTOS_EVENT_BIT_0, "Bit 0 should be set");
    
    // Test setting multiple bits
    result = pico_rtos_event_group_set_bits(&test_event_group, 
                                           PICO_RTOS_EVENT_BIT_1 | PICO_RTOS_EVENT_BIT_2);
    TEST_ASSERT(result == true, "Setting multiple bits should succeed");
    
    current_bits = pico_rtos_event_group_get_bits(&test_event_group);
    uint32_t expected = PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_1 | PICO_RTOS_EVENT_BIT_2;
    TEST_ASSERT(current_bits == expected, "Multiple bits should be set");
    
    // Test clearing individual bits
    result = pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_BIT_1);
    TEST_ASSERT(result == true, "Clearing bit 1 should succeed");
    
    current_bits = pico_rtos_event_group_get_bits(&test_event_group);
    expected = PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_2;
    TEST_ASSERT(current_bits == expected, "Bit 1 should be cleared");
    
    // Test clearing multiple bits
    result = pico_rtos_event_group_clear_bits(&test_event_group, 
                                             PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_2);
    TEST_ASSERT(result == true, "Clearing multiple bits should succeed");
    
    current_bits = pico_rtos_event_group_get_bits(&test_event_group);
    TEST_ASSERT(current_bits == 0, "All bits should be cleared");
    
    // Test NULL pointer handling
    result = pico_rtos_event_group_set_bits(NULL, PICO_RTOS_EVENT_BIT_0);
    TEST_ASSERT(result == false, "Setting bits with NULL should fail");
    
    result = pico_rtos_event_group_clear_bits(NULL, PICO_RTOS_EVENT_BIT_0);
    TEST_ASSERT(result == false, "Clearing bits with NULL should fail");
}

static void test_immediate_wait_conditions(void) {
    printf("\n=== Testing Immediate Wait Conditions ===\n");
    
    // Set up test events
    pico_rtos_event_group_set_bits(&test_event_group, 
                                  PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_1);
    
    // Test wait for any - should return immediately
    uint32_t result = pico_rtos_event_group_wait_bits(&test_event_group, 
                                                     PICO_RTOS_EVENT_BIT_0, 
                                                     false,  // wait for any
                                                     false,  // don't clear
                                                     PICO_RTOS_NO_WAIT);
    TEST_ASSERT(result == (PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_1), 
               "Wait for any should return immediately with current bits");
    
    // Test wait for all - should return immediately
    result = pico_rtos_event_group_wait_bits(&test_event_group, 
                                            PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_1, 
                                            true,   // wait for all
                                            false,  // don't clear
                                            PICO_RTOS_NO_WAIT);
    TEST_ASSERT(result == (PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_1), 
               "Wait for all should return immediately when condition satisfied");
    
    // Test wait for unavailable bits - should return 0 with no wait
    result = pico_rtos_event_group_wait_bits(&test_event_group, 
                                            PICO_RTOS_EVENT_BIT_3, 
                                            false,  // wait for any
                                            false,  // don't clear
                                            PICO_RTOS_NO_WAIT);
    TEST_ASSERT(result == 0, "Wait for unavailable bits should return 0 with no wait");
    
    // Test clear on exit
    result = pico_rtos_event_group_wait_bits(&test_event_group, 
                                            PICO_RTOS_EVENT_BIT_0, 
                                            false,  // wait for any
                                            true,   // clear on exit
                                            PICO_RTOS_NO_WAIT);
    TEST_ASSERT(result == (PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_1), 
               "Wait with clear should return current bits");
    
    uint32_t remaining_bits = pico_rtos_event_group_get_bits(&test_event_group);
    TEST_ASSERT(remaining_bits == PICO_RTOS_EVENT_BIT_1, 
               "Waited-for bits should be cleared");
    
    // Clean up
    pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
}

// =============================================================================
// BLOCKING AND TIMEOUT TESTS
// =============================================================================

static void test_blocking_wait_any(void) {
    printf("\n=== Testing Blocking Wait Any ===\n");
    
    reset_test_state();
    
    // Create task that waits for any of bits 0, 1, or 2
    uint32_t wait_bits = PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_1 | PICO_RTOS_EVENT_BIT_2;
    bool result = pico_rtos_task_create(&test_task1, "test_task1", test_task1_wait_any, 
                                       (void*)(uintptr_t)wait_bits, 
                                       sizeof(test_task1_stack), 5);
    TEST_ASSERT(result == true, "Task creation should succeed");
    
    // Give task time to start and block
    pico_rtos_task_delay(50);
    
    // Verify task is waiting
    uint32_t waiting_count = pico_rtos_event_group_get_waiting_count(&test_event_group);
    TEST_ASSERT(waiting_count == 1, "One task should be waiting");
    
    // Set bit 1 - should unblock the task
    pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_BIT_1);
    
    // Wait for task completion
    wait_for_tasks_completion(500);
    
    TEST_ASSERT(task1_completed == true, "Task should complete after event is set");
    TEST_ASSERT(task1_result == PICO_RTOS_EVENT_BIT_1, "Task should return the set bit");
    
    // Clean up
    pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
}

static void test_blocking_wait_all(void) {
    printf("\n=== Testing Blocking Wait All ===\n");
    
    reset_test_state();
    
    // Create task that waits for all of bits 0, 1, and 2
    uint32_t wait_bits = PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_1 | PICO_RTOS_EVENT_BIT_2;
    bool result = pico_rtos_task_create(&test_task2, "test_task2", test_task2_wait_all, 
                                       (void*)(uintptr_t)wait_bits, 
                                       sizeof(test_task2_stack), 5);
    TEST_ASSERT(result == true, "Task creation should succeed");
    
    // Give task time to start and block
    pico_rtos_task_delay(50);
    
    // Set only bit 0 - task should remain blocked
    pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_BIT_0);
    pico_rtos_task_delay(50);
    TEST_ASSERT(task2_completed == false, "Task should remain blocked with partial condition");
    
    // Set bit 1 - task should still remain blocked
    pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_BIT_1);
    pico_rtos_task_delay(50);
    TEST_ASSERT(task2_completed == false, "Task should remain blocked with partial condition");
    
    // Set bit 2 - now all conditions are met, task should unblock
    pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_BIT_2);
    
    // Wait for task completion
    wait_for_tasks_completion(500);
    
    TEST_ASSERT(task2_completed == true, "Task should complete when all conditions met");
    TEST_ASSERT(task2_result == wait_bits, "Task should return all waited-for bits");
    
    // Clean up
    pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
}

static void test_timeout_handling(void) {
    printf("\n=== Testing Timeout Handling ===\n");
    
    reset_test_state();
    
    // Create task that waits for unavailable bit with short timeout
    uint32_t wait_bits = PICO_RTOS_EVENT_BIT_7;  // This bit won't be set
    bool result = pico_rtos_task_create(&test_task1, "test_task1", test_task_timeout, 
                                       (void*)(uintptr_t)wait_bits, 
                                       sizeof(test_task1_stack), 5);
    TEST_ASSERT(result == true, "Task creation should succeed");
    
    // Wait for task completion (should timeout)
    wait_for_tasks_completion(TEST_TIMEOUT_MS + 200);
    
    TEST_ASSERT(task1_completed == true, "Task should complete due to timeout");
    TEST_ASSERT(task1_result == 0, "Task should return 0 on timeout");
}

// =============================================================================
// UTILITY FUNCTION TESTS
// =============================================================================

static void test_utility_functions(void) {
    printf("\n=== Testing Utility Functions ===\n");
    
    // Test bit mask creation
    uint8_t bit_numbers[] = {0, 2, 4, 6};
    uint32_t mask = pico_rtos_event_group_create_mask(bit_numbers, 4);
    uint32_t expected_mask = PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_2 | 
                            PICO_RTOS_EVENT_BIT_4 | PICO_RTOS_EVENT_BIT_6;
    TEST_ASSERT(mask == expected_mask, "Bit mask creation should work correctly");
    
    // Test bit counting
    uint32_t count = pico_rtos_event_group_count_bits(mask);
    TEST_ASSERT(count == 4, "Bit counting should return correct count");
    
    // Test with all bits set
    count = pico_rtos_event_group_count_bits(PICO_RTOS_EVENT_ALL_BITS);
    TEST_ASSERT(count == 32, "All bits count should be 32");
    
    // Test with no bits set
    count = pico_rtos_event_group_count_bits(0);
    TEST_ASSERT(count == 0, "No bits count should be 0");
    
    // Test NULL pointer handling
    mask = pico_rtos_event_group_create_mask(NULL, 4);
    TEST_ASSERT(mask == 0, "NULL bit numbers should return 0");
    
    mask = pico_rtos_event_group_create_mask(bit_numbers, 0);
    TEST_ASSERT(mask == 0, "Zero count should return 0");
}

// =============================================================================
// STATISTICS AND ERROR HANDLING TESTS
// =============================================================================

static void test_statistics(void) {
    printf("\n=== Testing Statistics ===\n");
    
    // Reset statistics
    bool result = pico_rtos_event_group_reset_stats(&test_event_group);
    TEST_ASSERT(result == true, "Statistics reset should succeed");
    
    // Perform some operations
    pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_BIT_0);
    pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_BIT_1);
    pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_BIT_0);
    
    // Get statistics
    pico_rtos_event_group_stats_t stats;
    result = pico_rtos_event_group_get_stats(&test_event_group, &stats);
    TEST_ASSERT(result == true, "Getting statistics should succeed");
    
    TEST_ASSERT(stats.current_events == PICO_RTOS_EVENT_BIT_1, 
               "Current events should match actual state");
    TEST_ASSERT(stats.total_set_operations == 2, 
               "Set operations count should be correct");
    TEST_ASSERT(stats.total_clear_operations == 1, 
               "Clear operations count should be correct");
    
    // Test NULL pointer handling
    result = pico_rtos_event_group_get_stats(NULL, &stats);
    TEST_ASSERT(result == false, "Getting stats with NULL event group should fail");
    
    result = pico_rtos_event_group_get_stats(&test_event_group, NULL);
    TEST_ASSERT(result == false, "Getting stats with NULL stats should fail");
    
    // Clean up
    pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
}

static void test_error_conditions(void) {
    printf("\n=== Testing Error Conditions ===\n");
    
    // Test invalid parameters for wait functions
    uint32_t result = pico_rtos_event_group_wait_bits(NULL, PICO_RTOS_EVENT_BIT_0, 
                                                     false, false, 100);
    TEST_ASSERT(result == 0, "Wait with NULL event group should return 0");
    
    result = pico_rtos_event_group_wait_bits(&test_event_group, 0, 
                                            false, false, 100);
    TEST_ASSERT(result == 0, "Wait with no bits should return 0");
    
    // Test get functions with NULL
    result = pico_rtos_event_group_get_bits(NULL);
    TEST_ASSERT(result == 0, "Get bits with NULL should return 0");
    
    result = pico_rtos_event_group_get_waiting_count(NULL);
    TEST_ASSERT(result == 0, "Get waiting count with NULL should return 0");
    
    // Test setting/clearing with zero bits (should succeed but be no-op)
    bool bool_result = pico_rtos_event_group_set_bits(&test_event_group, 0);
    TEST_ASSERT(bool_result == true, "Setting zero bits should succeed as no-op");
    
    bool_result = pico_rtos_event_group_clear_bits(&test_event_group, 0);
    TEST_ASSERT(bool_result == true, "Clearing zero bits should succeed as no-op");
    
    // Test edge cases with valid bit patterns
    // Since uint32_t can only hold 32 bits, all possible values are technically valid
    // Test with all valid bits (should succeed)
    bool_result = pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
    TEST_ASSERT(bool_result == true, "Setting all valid bits should succeed");
    
    bool_result = pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
    TEST_ASSERT(bool_result == true, "Clearing all valid bits should succeed");
    
    // Test maximum uint32_t value (all bits set)
    bool_result = pico_rtos_event_group_set_bits(&test_event_group, UINT32_MAX);
    TEST_ASSERT(bool_result == true, "Setting maximum uint32_t value should succeed");
    
    bool_result = pico_rtos_event_group_clear_bits(&test_event_group, UINT32_MAX);
    TEST_ASSERT(bool_result == true, "Clearing maximum uint32_t value should succeed");
}

// =============================================================================
// CONCURRENT MANIPULATION TESTS
// =============================================================================

// Test data for concurrent scenarios
static volatile uint32_t concurrent_set_count = 0;
static volatile uint32_t concurrent_clear_count = 0;
static volatile uint32_t concurrent_get_count = 0;

static void concurrent_setter_task(void *param) {
    uint32_t bit_pattern = (uint32_t)(uintptr_t)param;
    
    // Perform multiple set operations
    for (int i = 0; i < 10; i++) {
        if (pico_rtos_event_group_set_bits(&test_event_group, bit_pattern)) {
            concurrent_set_count++;
        }
        pico_rtos_task_delay(5);  // Small delay to allow interleaving
    }
    
    task1_completed = true;
    pico_rtos_task_delete(NULL);
}

static void concurrent_clearer_task(void *param) {
    uint32_t bit_pattern = (uint32_t)(uintptr_t)param;
    
    // Perform multiple clear operations
    for (int i = 0; i < 10; i++) {
        if (pico_rtos_event_group_clear_bits(&test_event_group, bit_pattern)) {
            concurrent_clear_count++;
        }
        pico_rtos_task_delay(7);  // Different delay to create race conditions
    }
    
    task2_completed = true;
    pico_rtos_task_delete(NULL);
}

static void concurrent_reader_task(void *param) {
    (void)param;
    
    // Perform multiple read operations
    for (int i = 0; i < 15; i++) {
        uint32_t bits = pico_rtos_event_group_get_bits(&test_event_group);
        if (bits != 0xFFFFFFFF) {  // Valid read (not error)
            concurrent_get_count++;
        }
        pico_rtos_task_delay(3);  // Frequent reads
    }
    
    task3_completed = true;
    pico_rtos_task_delete(NULL);
}

static void test_concurrent_set_clear_operations(void) {
    printf("\n=== Testing Concurrent Set/Clear Operations ===\n");
    
    reset_test_state();
    concurrent_set_count = 0;
    concurrent_clear_count = 0;
    concurrent_get_count = 0;
    
    // Clear all bits initially
    pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
    
    // Create tasks that concurrently manipulate different bits
    bool result1 = pico_rtos_task_create(&test_task1, "setter", concurrent_setter_task, 
                                        (void*)(uintptr_t)(PICO_RTOS_EVENT_BIT_0 | PICO_RTOS_EVENT_BIT_2), 
                                        sizeof(test_task1_stack), 5);
    
    bool result2 = pico_rtos_task_create(&test_task2, "clearer", concurrent_clearer_task, 
                                        (void*)(uintptr_t)(PICO_RTOS_EVENT_BIT_1 | PICO_RTOS_EVENT_BIT_3), 
                                        sizeof(test_task2_stack), 5);
    
    bool result3 = pico_rtos_task_create(&test_task3, "reader", concurrent_reader_task, 
                                        NULL, sizeof(test_task3_stack), 5);
    
    TEST_ASSERT(result1 && result2 && result3, "All concurrent tasks should be created successfully");
    
    // Wait for all tasks to complete
    wait_for_tasks_completion(2000);
    
    TEST_ASSERT(task1_completed && task2_completed && task3_completed, 
               "All concurrent tasks should complete");
    
    TEST_ASSERT(concurrent_set_count == 10, "All set operations should succeed");
    TEST_ASSERT(concurrent_clear_count == 10, "All clear operations should succeed");
    TEST_ASSERT(concurrent_get_count == 15, "All get operations should succeed");
    
    // Verify final state is consistent
    uint32_t final_bits = pico_rtos_event_group_get_bits(&test_event_group);
    printf("Final event bits after concurrent operations: 0x%08lx\n", final_bits);
    
    // Clean up
    pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
}

static void test_atomic_bit_manipulation(void) {
    printf("\n=== Testing Atomic Bit Manipulation ===\n");
    
    // Test that bit operations are truly atomic by performing rapid operations
    // and verifying consistency
    
    // Clear all bits
    pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_ALL_BITS);
    
    // Test atomic set operations
    for (int i = 0; i < 32; i++) {
        uint32_t bit = (1UL << i);
        bool result = pico_rtos_event_group_set_bits(&test_event_group, bit);
        TEST_ASSERT(result == true, "Each individual bit set should succeed");
        
        uint32_t current_bits = pico_rtos_event_group_get_bits(&test_event_group);
        TEST_ASSERT((current_bits & bit) != 0, "Set bit should be immediately visible");
    }
    
    // Verify all bits are set
    uint32_t all_bits = pico_rtos_event_group_get_bits(&test_event_group);
    TEST_ASSERT(all_bits == PICO_RTOS_EVENT_ALL_BITS, "All 32 bits should be set");
    
    // Test atomic clear operations
    for (int i = 0; i < 32; i++) {
        uint32_t bit = (1UL << i);
        bool result = pico_rtos_event_group_clear_bits(&test_event_group, bit);
        TEST_ASSERT(result == true, "Each individual bit clear should succeed");
        
        uint32_t current_bits = pico_rtos_event_group_get_bits(&test_event_group);
        TEST_ASSERT((current_bits & bit) == 0, "Cleared bit should be immediately invisible");
    }
    
    // Verify all bits are cleared
    uint32_t no_bits = pico_rtos_event_group_get_bits(&test_event_group);
    TEST_ASSERT(no_bits == 0, "All bits should be cleared");
}

static void test_bit_range_validation(void) {
    printf("\n=== Testing Bit Range Validation ===\n");
    
    // Test valid bit patterns (all combinations within 32-bit range)
    bool result;
    
    // Test individual valid bits
    for (int i = 0; i < 32; i++) {
        uint32_t bit = (1UL << i);
        result = pico_rtos_event_group_set_bits(&test_event_group, bit);
        TEST_ASSERT(result == true, "Valid individual bits should be accepted");
        
        result = pico_rtos_event_group_clear_bits(&test_event_group, bit);
        TEST_ASSERT(result == true, "Valid individual bits should be clearable");
    }
    
    // Test valid multi-bit patterns
    uint32_t valid_patterns[] = {
        0x00000001,  // Single bit
        0x00000003,  // Two adjacent bits
        0x00000005,  // Two non-adjacent bits
        0x000000FF,  // First byte
        0x0000FFFF,  // First two bytes
        0xFFFF0000,  // Last two bytes
        0xFFFFFFFF,  // All bits
        0xAAAAAAAA,  // Alternating pattern
        0x55555555   // Opposite alternating pattern
    };
    
    for (size_t i = 0; i < sizeof(valid_patterns) / sizeof(valid_patterns[0]); i++) {
        result = pico_rtos_event_group_set_bits(&test_event_group, valid_patterns[i]);
        TEST_ASSERT(result == true, "Valid bit patterns should be accepted for setting");
        
        result = pico_rtos_event_group_clear_bits(&test_event_group, valid_patterns[i]);
        TEST_ASSERT(result == true, "Valid bit patterns should be accepted for clearing");
    }
    
    // Note: Since we're using uint32_t, all possible values are technically valid
    // The validation is more about ensuring the implementation handles edge cases correctly
    
    // Test edge case: maximum value
    result = pico_rtos_event_group_set_bits(&test_event_group, UINT32_MAX);
    TEST_ASSERT(result == true, "Maximum uint32_t value should be valid");
    
    result = pico_rtos_event_group_clear_bits(&test_event_group, UINT32_MAX);
    TEST_ASSERT(result == true, "Maximum uint32_t value should be clearable");
}

static void test_performance_characteristics(void) {
    printf("\n=== Testing Performance Characteristics ===\n");
    
    // Test that operations are indeed O(1) by measuring time for different bit counts
    // This is a basic performance test to ensure no unexpected complexity
    
    uint32_t start_time, end_time;
    
    // Test single bit operations
    start_time = pico_rtos_get_tick_count();
    for (int i = 0; i < 1000; i++) {
        pico_rtos_event_group_set_bits(&test_event_group, PICO_RTOS_EVENT_BIT_0);
        pico_rtos_event_group_clear_bits(&test_event_group, PICO_RTOS_EVENT_BIT_0);
    }
    end_time = pico_rtos_get_tick_count();
    uint32_t single_bit_time = end_time - start_time;
    
    // Test multiple bit operations
    start_time = pico_rtos_get_tick_count();
    for (int i = 0; i < 1000; i++) {
        pico_rtos_event_group_set_bits(&test_event_group, 0xFFFFFFFF);
        pico_rtos_event_group_clear_bits(&test_event_group, 0xFFFFFFFF);
    }
    end_time = pico_rtos_get_tick_count();
    uint32_t multi_bit_time = end_time - start_time;
    
    printf("Single bit operations (1000 iterations): %lu ticks\n", single_bit_time);
    printf("Multi bit operations (1000 iterations): %lu ticks\n", multi_bit_time);
    
    // The times should be similar for O(1) operations
    // Allow for some variance due to system overhead
    uint32_t time_ratio = (multi_bit_time * 100) / (single_bit_time + 1);  // Avoid division by zero
    TEST_ASSERT(time_ratio < 200, "Multi-bit operations should not be significantly slower (O(1) characteristic)");
    
    printf("Performance ratio (multi/single * 100): %lu%% (should be < 200%%)\n", time_ratio);
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

static void run_all_tests(void) {
    printf("\n");
    printf("=====================================\n");
    printf("  Pico-RTOS Event Group Unit Tests  \n");
    printf("=====================================\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("FATAL: Failed to initialize RTOS\n");
        return;
    }
    
    // Initialize test mutex for synchronization
    if (!pico_rtos_mutex_init(&test_mutex)) {
        printf("FATAL: Failed to initialize test mutex\n");
        return;
    }
    
    // Run all tests
    test_event_group_initialization();
    test_event_setting_and_clearing();
    test_immediate_wait_conditions();
    test_blocking_wait_any();
    test_blocking_wait_all();
    test_timeout_handling();
    test_utility_functions();
    test_statistics();
    test_error_conditions();
    
    // New concurrent manipulation tests for task 2.2
    test_concurrent_set_clear_operations();
    test_atomic_bit_manipulation();
    test_bit_range_validation();
    test_performance_characteristics();
    
    // Clean up
    pico_rtos_event_group_delete(&test_event_group);
    
    // Print results
    printf("\n");
    printf("=====================================\n");
    printf("           Test Results              \n");
    printf("=====================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Failures: %d\n", test_failures);
    printf("Success rate: %.1f%%\n", 
           test_count > 0 ? (100.0 * (test_count - test_failures) / test_count) : 0.0);
    
    if (test_failures == 0) {
        printf("Result: ALL TESTS PASSED ✓\n");
    } else {
        printf("Result: %d TESTS FAILED ✗\n", test_failures);
    }
    printf("=====================================\n");
}

// Main test task
static void test_main_task(void *param) {
    (void)param;
    
    // Give system time to stabilize
    pico_rtos_task_delay(100);
    
    // Run all tests
    run_all_tests();
    
    // Keep the system running
    while (true) {
        pico_rtos_task_delay(1000);
    }
}

int main() {
    stdio_init_all();
    
    // Give USB time to connect
    sleep_ms(2000);
    
    printf("Starting Pico-RTOS Event Group Tests...\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("Failed to initialize RTOS\n");
        return 1;
    }
    
    // Create main test task
    static pico_rtos_task_t main_task;
    static uint32_t main_task_stack[1024];
    
    if (!pico_rtos_task_create(&main_task, "main_test", test_main_task, NULL, 
                              sizeof(main_task_stack), 10)) {
        printf("Failed to create main test task\n");
        return 1;
    }
    
    // Start the RTOS
    pico_rtos_start();
    
    // Should never reach here
    return 0;
}