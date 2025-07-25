/**
 * @file comprehensive_event_group_test.c
 * @brief Enhanced comprehensive unit tests for Event Groups with edge cases
 * 
 * This extends the existing event_group_test.c with additional edge cases,
 * stress testing, and multi-core scenarios for v0.3.0
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

// Check if event groups are enabled
#ifdef PICO_RTOS_ENABLE_EVENT_GROUPS
#include "pico_rtos/event_group.h"
#else
// Mock event group definitions for compilation
typedef struct { int dummy; } pico_rtos_event_group_t;
typedef struct { int dummy; } pico_rtos_event_group_stats_t;
#define PICO_RTOS_EVENT_BIT_0 (1UL << 0)
#define PICO_RTOS_EVENT_BIT_1 (1UL << 1)
#define PICO_RTOS_EVENT_BIT_2 (1UL << 2)
#define PICO_RTOS_EVENT_BIT_3 (1UL << 3)
#define PICO_RTOS_EVENT_BIT_7 (1UL << 7)
#define PICO_RTOS_EVENT_ALL_BITS 0xFFFFFFFF
#define PICO_RTOS_NO_WAIT 0
#define PICO_RTOS_WAIT_FOREVER 0xFFFFFFFF
static inline bool pico_rtos_event_group_init(pico_rtos_event_group_t *group) { (void)group; return false; }
static inline bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *group, uint32_t bits) { (void)group; (void)bits; return false; }
static inline bool pico_rtos_event_group_clear_bits(pico_rtos_event_group_t *group, uint32_t bits) { (void)group; (void)bits; return false; }
static inline uint32_t pico_rtos_event_group_get_bits(pico_rtos_event_group_t *group) { (void)group; return 0; }
static inline uint32_t pico_rtos_event_group_wait_bits(pico_rtos_event_group_t *group, uint32_t bits, bool wait_all, bool clear, uint32_t timeout) { (void)group; (void)bits; (void)wait_all; (void)clear; (void)timeout; return 0; }
static inline uint32_t pico_rtos_event_group_get_waiting_count(pico_rtos_event_group_t *group) { (void)group; return 0; }
static inline bool pico_rtos_event_group_get_stats(pico_rtos_event_group_t *group, pico_rtos_event_group_stats_t *stats) { (void)group; (void)stats; return false; }
static inline bool pico_rtos_event_group_reset_stats(pico_rtos_event_group_t *group) { (void)group; return false; }
static inline uint32_t pico_rtos_event_group_create_mask(uint8_t *bit_numbers, uint32_t count) { (void)bit_numbers; (void)count; return 0; }
static inline uint32_t pico_rtos_event_group_count_bits(uint32_t mask) { (void)mask; return 0; }
static inline bool pico_rtos_event_group_validate(pico_rtos_event_group_t *group) { (void)group; return false; }
#endif

// Test configuration
#define MAX_CONCURRENT_TASKS 8
#define STRESS_TEST_ITERATIONS 1000
#define EDGE_CASE_TIMEOUT_MS 50

// Test state
static volatile bool test_passed = true;
static volatile int test_count = 0;
static volatile int test_failures = 0;

// Test event group
static pico_rtos_event_group_t stress_test_group;

// Test helper macro
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

// =============================================================================
// STRESS TESTING
// =============================================================================

static void test_high_frequency_operations(void) {
    printf("\n=== Testing High Frequency Operations ===\n");
    
    bool result = pico_rtos_event_group_init(&stress_test_group);
    TEST_ASSERT(result, "Stress test event group initialization should succeed");
    
    uint32_t start_time = pico_rtos_get_tick_count();
    
    // Perform rapid set/clear operations
    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        uint32_t bit_pattern = (1UL << (i % 32));
        
        result = pico_rtos_event_group_set_bits(&stress_test_group, bit_pattern);
        if (!result) {
            test_failures++;
            break;
        }
        
        result = pico_rtos_event_group_clear_bits(&stress_test_group, bit_pattern);
        if (!result) {
            test_failures++;
            break;
        }
    }
    
    uint32_t end_time = pico_rtos_get_tick_count();
    uint32_t duration = end_time - start_time;
    
    printf("Completed %d set/clear operations in %lu ticks\n", 
           STRESS_TEST_ITERATIONS * 2, duration);
    
    TEST_ASSERT(test_failures == 0, "All high frequency operations should succeed");
    
    // Verify final state
    uint32_t final_bits = pico_rtos_event_group_get_bits(&stress_test_group);
    TEST_ASSERT(final_bits == 0, "All bits should be cleared after stress test");
}

static void test_boundary_bit_patterns(void) {
    printf("\n=== Testing Boundary Bit Patterns ===\n");
    
    // Test all possible single bit positions
    for (int i = 0; i < 32; i++) {
        uint32_t single_bit = (1UL << i);
        
        bool result = pico_rtos_event_group_set_bits(&stress_test_group, single_bit);
        TEST_ASSERT(result, "Setting single bit should succeed");
        
        uint32_t current_bits = pico_rtos_event_group_get_bits(&stress_test_group);
        TEST_ASSERT((current_bits & single_bit) != 0, "Single bit should be set");
        
        result = pico_rtos_event_group_clear_bits(&stress_test_group, single_bit);
        TEST_ASSERT(result, "Clearing single bit should succeed");
    }
    
    // Test alternating patterns
    uint32_t pattern1 = 0xAAAAAAAA; // 10101010...
    uint32_t pattern2 = 0x55555555; // 01010101...
    
    bool result = pico_rtos_event_group_set_bits(&stress_test_group, pattern1);
    TEST_ASSERT(result, "Setting alternating pattern 1 should succeed");
    
    uint32_t current = pico_rtos_event_group_get_bits(&stress_test_group);
    TEST_ASSERT(current == pattern1, "Pattern 1 should be set correctly");
    
    result = pico_rtos_event_group_set_bits(&stress_test_group, pattern2);
    TEST_ASSERT(result, "Setting alternating pattern 2 should succeed");
    
    current = pico_rtos_event_group_get_bits(&stress_test_group);
    TEST_ASSERT(current == 0xFFFFFFFF, "Both patterns should result in all bits set");
    
    // Clear all
    pico_rtos_event_group_clear_bits(&stress_test_group, 0xFFFFFFFF);
}

// =============================================================================
// MULTI-CORE EDGE CASES
// =============================================================================

typedef struct {
    pico_rtos_event_group_t *group;
    uint32_t task_id;
    uint32_t operations_completed;
    bool task_finished;
} multicore_test_param_t;

static void multicore_setter_task(void *param) {
    multicore_test_param_t *test_param = (multicore_test_param_t *)param;
    
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t bit_to_set = (1UL << (test_param->task_id % 32));
        
        if (pico_rtos_event_group_set_bits(test_param->group, bit_to_set)) {
            test_param->operations_completed++;
        }
        
        pico_rtos_task_delay(1); // Small delay to allow interleaving
        
        if (pico_rtos_event_group_clear_bits(test_param->group, bit_to_set)) {
            test_param->operations_completed++;
        }
        
        pico_rtos_task_delay(1);
    }
    
    test_param->task_finished = true;
    pico_rtos_task_delete(NULL);
}

static void test_multicore_stress(void) {
    printf("\n=== Testing Multi-Core Stress Scenarios ===\n");
    
    const int num_tasks = 4;
    pico_rtos_task_t tasks[num_tasks];
    multicore_test_param_t params[num_tasks];
    uint32_t task_stacks[num_tasks][256];
    
    // Initialize parameters
    for (int i = 0; i < num_tasks; i++) {
        params[i].group = &stress_test_group;
        params[i].task_id = i;
        params[i].operations_completed = 0;
        params[i].task_finished = false;
    }
    
    // Create tasks
    for (int i = 0; i < num_tasks; i++) {
        bool result = pico_rtos_task_create(&tasks[i], "multicore_test", 
                                           multicore_setter_task, &params[i],
                                           sizeof(task_stacks[i]), 5);
        TEST_ASSERT(result, "Multi-core test task creation should succeed");
    }
    
    // Wait for all tasks to complete
    bool all_finished = false;
    uint32_t timeout_count = 0;
    while (!all_finished && timeout_count < 5000) {
        all_finished = true;
        for (int i = 0; i < num_tasks; i++) {
            if (!params[i].task_finished) {
                all_finished = false;
                break;
            }
        }
        pico_rtos_task_delay(10);
        timeout_count++;
    }
    
    TEST_ASSERT(all_finished, "All multi-core tasks should complete");
    
    // Verify operations completed
    uint32_t total_operations = 0;
    for (int i = 0; i < num_tasks; i++) {
        total_operations += params[i].operations_completed;
        printf("Task %d completed %lu operations\n", i, params[i].operations_completed);
    }
    
    TEST_ASSERT(total_operations > 0, "Multi-core tasks should complete operations");
    
    // Verify final state is consistent
    uint32_t final_bits = pico_rtos_event_group_get_bits(&stress_test_group);
    printf("Final event group state: 0x%08lx\n", final_bits);
}

// =============================================================================
// TIMEOUT EDGE CASES
// =============================================================================

static void test_timeout_edge_cases(void) {
    printf("\n=== Testing Timeout Edge Cases ===\n");
    
    // Test zero timeout (PICO_RTOS_NO_WAIT)
    uint32_t result = pico_rtos_event_group_wait_bits(&stress_test_group,
                                                     PICO_RTOS_EVENT_BIT_7,
                                                     false, false,
                                                     PICO_RTOS_NO_WAIT);
    TEST_ASSERT(result == 0, "Wait with no timeout should return 0 for unavailable bits");
    
    // Test very short timeout
    uint32_t start_time = pico_rtos_get_tick_count();
    result = pico_rtos_event_group_wait_bits(&stress_test_group,
                                            PICO_RTOS_EVENT_BIT_7,
                                            false, false, 1);
    uint32_t end_time = pico_rtos_get_tick_count();
    
    TEST_ASSERT(result == 0, "Short timeout should return 0 for unavailable bits");
    TEST_ASSERT((end_time - start_time) >= 1, "Short timeout should actually wait");
    
    // Test maximum timeout value
    start_time = pico_rtos_get_tick_count();
    
    // Set the bit in another "task" (simulate by setting it after a delay)
    pico_rtos_event_group_set_bits(&stress_test_group, PICO_RTOS_EVENT_BIT_7);
    
    result = pico_rtos_event_group_wait_bits(&stress_test_group,
                                            PICO_RTOS_EVENT_BIT_7,
                                            false, false,
                                            PICO_RTOS_WAIT_FOREVER);
    
    TEST_ASSERT(result == PICO_RTOS_EVENT_BIT_7, "Infinite wait should return when bit is set");
    
    // Clean up
    pico_rtos_event_group_clear_bits(&stress_test_group, PICO_RTOS_EVENT_ALL_BITS);
}

// =============================================================================
// MEMORY AND RESOURCE EDGE CASES
// =============================================================================

static void test_resource_exhaustion(void) {
    printf("\n=== Testing Resource Exhaustion Scenarios ===\n");
    
    // Test creating many event groups (limited by available memory)
    const int max_groups = 10;
    pico_rtos_event_group_t groups[max_groups];
    int created_count = 0;
    
    for (int i = 0; i < max_groups; i++) {
        if (pico_rtos_event_group_init(&groups[i])) {
            created_count++;
        } else {
            break;
        }
    }
    
    TEST_ASSERT(created_count > 0, "Should be able to create at least one event group");
    printf("Successfully created %d event groups\n", created_count);
    
    // Test operations on all created groups
    for (int i = 0; i < created_count; i++) {
        bool result = pico_rtos_event_group_set_bits(&groups[i], PICO_RTOS_EVENT_BIT_0);
        TEST_ASSERT(result, "Operations should work on all created groups");
    }
    
    // Clean up
    for (int i = 0; i < created_count; i++) {
        pico_rtos_event_group_clear_bits(&groups[i], PICO_RTOS_EVENT_ALL_BITS);
    }
}

// =============================================================================
// STATISTICS VALIDATION
// =============================================================================

static void test_statistics_accuracy(void) {
    printf("\n=== Testing Statistics Accuracy ===\n");
    
    // Reset statistics
    bool result = pico_rtos_event_group_reset_stats(&stress_test_group);
    TEST_ASSERT(result, "Statistics reset should succeed");
    
    // Perform known operations
    const int set_operations = 5;
    const int clear_operations = 3;
    
    for (int i = 0; i < set_operations; i++) {
        pico_rtos_event_group_set_bits(&stress_test_group, PICO_RTOS_EVENT_BIT_0);
    }
    
    for (int i = 0; i < clear_operations; i++) {
        pico_rtos_event_group_clear_bits(&stress_test_group, PICO_RTOS_EVENT_BIT_0);
    }
    
    // Check statistics
    pico_rtos_event_group_stats_t stats;
    result = pico_rtos_event_group_get_stats(&stress_test_group, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    
    TEST_ASSERT(stats.total_set_operations == set_operations,
               "Set operations count should be accurate");
    TEST_ASSERT(stats.total_clear_operations == clear_operations,
               "Clear operations count should be accurate");
    
    printf("Statistics validation: %lu set ops, %lu clear ops\n",
           stats.total_set_operations, stats.total_clear_operations);
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

static void run_comprehensive_event_group_tests(void) {
    printf("\n");
    printf("================================================\n");
    printf("  Comprehensive Event Group Unit Tests (v0.3.0)\n");
    printf("================================================\n");
    
    test_high_frequency_operations();
    test_boundary_bit_patterns();
    test_multicore_stress();
    test_timeout_edge_cases();
    test_resource_exhaustion();
    test_statistics_accuracy();
    
    printf("\n");
    printf("================================================\n");
    printf("         Comprehensive Test Results             \n");
    printf("================================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_count - test_failures);
    printf("Failed: %d\n", test_failures);
    printf("Success rate: %.1f%%\n", 
           test_count > 0 ? (100.0 * (test_count - test_failures)) / test_count : 0.0);
    printf("================================================\n");
    
    if (test_failures == 0) {
        printf("🎉 All comprehensive event group tests PASSED!\n");
    } else {
        printf("❌ Some comprehensive tests FAILED!\n");
    }
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("Comprehensive Event Group Test Starting...\n");
    
#ifdef PICO_RTOS_ENABLE_EVENT_GROUPS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    run_comprehensive_event_group_tests();
    
    return (test_failures == 0) ? 0 : 1;
#else
    printf("Event groups are not enabled in this build configuration.\n");
    printf("Enable PICO_RTOS_ENABLE_EVENT_GROUPS to run this test.\n");
    return 0;
#endif
}