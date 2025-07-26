/**
 * @file memory_pool_statistics_test.c
 * @brief Comprehensive tests for memory pool statistics and monitoring
 * 
 * This test validates all statistics and monitoring functionality
 * including allocation tracking, peak usage monitoring, fragmentation
 * metrics, and runtime pool inspection.
 */

#include "pico_rtos/memory_pool.h"
#include "pico_rtos/error.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test configuration
#define TEST_POOL_BLOCK_SIZE 64
#define TEST_POOL_BLOCK_COUNT 20
#define TEST_POOL_MEMORY_SIZE (TEST_POOL_BLOCK_SIZE * TEST_POOL_BLOCK_COUNT + 64)

// Test memory buffer
static uint8_t test_memory[TEST_POOL_MEMORY_SIZE] __attribute__((aligned(4)));
static pico_rtos_memory_pool_t test_pool;

// Test statistics
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test result macros
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

#define TEST_ASSERT_EQUAL(expected, actual, message) do { \
    tests_run++; \
    if ((expected) == (actual)) { \
        tests_passed++; \
        printf("PASS: %s (expected: %d, actual: %d)\n", message, (int)(expected), (int)(actual)); \
    } else { \
        tests_failed++; \
        printf("FAIL: %s (expected: %d, actual: %d)\n", message, (int)(expected), (int)(actual)); \
    } \
} while(0)

#define TEST_ASSERT_RANGE(value, min, max, message) do { \
    tests_run++; \
    if ((value) >= (min) && (value) <= (max)) { \
        tests_passed++; \
        printf("PASS: %s (value: %d, range: %d-%d)\n", message, (int)(value), (int)(min), (int)(max)); \
    } else { \
        tests_failed++; \
        printf("FAIL: %s (value: %d, range: %d-%d)\n", message, (int)(value), (int)(min), (int)(max)); \
    } \
} while(0)

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Reset test environment
 */
static void reset_test_environment(void) {
    memset(&test_pool, 0, sizeof(test_pool));
    memset(test_memory, 0, sizeof(test_memory));
    pico_rtos_clear_last_error();
}

/**
 * @brief Print test results summary
 */
static void print_test_summary(void) {
    printf("\n=== Memory Pool Statistics Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    printf("===========================================\n\n");
}

// =============================================================================
// STATISTICS TESTS
// =============================================================================

#if PICO_RTOS_ENABLE_MEMORY_TRACKING

/**
 * @brief Test basic statistics initialization and structure
 */
static void test_basic_statistics(void) {
    printf("\n--- Testing Basic Statistics ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Get initial statistics
    pico_rtos_memory_pool_stats_t stats;
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    
    // Verify initial statistics
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, stats.total_blocks, "Total blocks should match initialization");
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, stats.free_blocks, "All blocks should be free initially");
    TEST_ASSERT_EQUAL(0, stats.allocated_blocks, "No blocks should be allocated initially");
    TEST_ASSERT_EQUAL(0, stats.peak_allocated, "Peak allocated should be zero initially");
    TEST_ASSERT_EQUAL(0, stats.allocation_count, "Allocation count should be zero initially");
    TEST_ASSERT_EQUAL(0, stats.deallocation_count, "Deallocation count should be zero initially");
    TEST_ASSERT_EQUAL(0, stats.allocation_failures, "No allocation failures initially");
    TEST_ASSERT_EQUAL(0, stats.corruption_detected, "No corruption detected initially");
    TEST_ASSERT_EQUAL(0, stats.double_free_detected, "No double-free detected initially");
    
    // Verify pool configuration in statistics
    TEST_ASSERT(stats.block_size >= TEST_POOL_BLOCK_SIZE, "Block size should be at least requested size");
    TEST_ASSERT_EQUAL(stats.block_size * TEST_POOL_BLOCK_COUNT, stats.pool_size, 
                     "Pool size should equal block_size * total_blocks");
    TEST_ASSERT_EQUAL(0.0f, stats.fragmentation_ratio, "Fragmentation should be zero for memory pools");
}

/**
 * @brief Test allocation and deallocation statistics tracking
 */
static void test_allocation_statistics(void) {
    printf("\n--- Testing Allocation Statistics ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    pico_rtos_memory_pool_stats_t stats;
    
    // Perform some allocations
    void *blocks[5];
    for (int i = 0; i < 5; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
        TEST_ASSERT(blocks[i] != NULL, "Allocation should succeed");
        
        // Check statistics after each allocation
        result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
        TEST_ASSERT(result, "Getting statistics should succeed");
        
        TEST_ASSERT_EQUAL(i + 1, stats.allocation_count, "Allocation count should increment");
        TEST_ASSERT_EQUAL(i + 1, stats.allocated_blocks, "Allocated blocks should increment");
        TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT - (i + 1), stats.free_blocks, "Free blocks should decrement");
        TEST_ASSERT_EQUAL(i + 1, stats.peak_allocated, "Peak allocated should track maximum");
    }
    
    // Perform some deallocations
    for (int i = 0; i < 3; i++) {
        result = pico_rtos_memory_pool_free(&test_pool, blocks[i]);
        TEST_ASSERT(result, "Deallocation should succeed");
        
        // Check statistics after each deallocation
        result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
        TEST_ASSERT(result, "Getting statistics should succeed");
        
        TEST_ASSERT_EQUAL(i + 1, stats.deallocation_count, "Deallocation count should increment");
        TEST_ASSERT_EQUAL(5 - (i + 1), stats.allocated_blocks, "Allocated blocks should decrement");
        TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT - (5 - (i + 1)), stats.free_blocks, "Free blocks should increment");
        TEST_ASSERT_EQUAL(5, stats.peak_allocated, "Peak allocated should be maintained");
    }
    
    // Clean up remaining blocks
    for (int i = 3; i < 5; i++) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
}

/**
 * @brief Test peak allocation tracking
 */
static void test_peak_allocation_tracking(void) {
    printf("\n--- Testing Peak Allocation Tracking ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    void *blocks[10];
    
    // Allocate blocks in waves to test peak tracking
    
    // Wave 1: Allocate 3 blocks
    for (int i = 0; i < 3; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    }
    
    uint32_t peak = pico_rtos_memory_pool_get_peak_allocated(&test_pool);
    TEST_ASSERT_EQUAL(3, peak, "Peak should be 3 after first wave");
    
    // Wave 2: Allocate 4 more blocks (total 7)
    for (int i = 3; i < 7; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    }
    
    peak = pico_rtos_memory_pool_get_peak_allocated(&test_pool);
    TEST_ASSERT_EQUAL(7, peak, "Peak should be 7 after second wave");
    
    // Free some blocks (down to 4 allocated)
    for (int i = 0; i < 3; i++) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
    
    peak = pico_rtos_memory_pool_get_peak_allocated(&test_pool);
    TEST_ASSERT_EQUAL(7, peak, "Peak should remain 7 after freeing blocks");
    
    // Wave 3: Allocate 2 more blocks (total 6, still less than peak)
    for (int i = 0; i < 2; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    }
    
    peak = pico_rtos_memory_pool_get_peak_allocated(&test_pool);
    TEST_ASSERT_EQUAL(7, peak, "Peak should remain 7 when not exceeded");
    
    // Wave 4: Allocate 3 more blocks (total 9, new peak)
    for (int i = 7; i < 10; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    }
    
    peak = pico_rtos_memory_pool_get_peak_allocated(&test_pool);
    TEST_ASSERT_EQUAL(9, peak, "Peak should be 9 after exceeding previous peak");
    
    // Clean up
    for (int i = 0; i < 10; i++) {
        if (blocks[i]) {
            pico_rtos_memory_pool_free(&test_pool, blocks[i]);
        }
    }
}

/**
 * @brief Test statistics reset functionality
 */
static void test_statistics_reset(void) {
    printf("\n--- Testing Statistics Reset ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Perform some operations to generate statistics
    void *blocks[5];
    for (int i = 0; i < 5; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    }
    
    for (int i = 0; i < 2; i++) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
    
    // Verify statistics before reset
    pico_rtos_memory_pool_stats_t stats;
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    
    TEST_ASSERT_EQUAL(5, stats.allocation_count, "Should have 5 allocations before reset");
    TEST_ASSERT_EQUAL(2, stats.deallocation_count, "Should have 2 deallocations before reset");
    TEST_ASSERT_EQUAL(5, stats.peak_allocated, "Should have peak of 5 before reset");
    
    // Reset statistics
    pico_rtos_memory_pool_reset_stats(&test_pool);
    
    // Verify statistics after reset
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    
    // Counters should be reset
    TEST_ASSERT_EQUAL(0, stats.allocation_count, "Allocation count should be reset");
    TEST_ASSERT_EQUAL(0, stats.deallocation_count, "Deallocation count should be reset");
    TEST_ASSERT_EQUAL(0, stats.peak_allocated, "Peak allocated should be reset");
    TEST_ASSERT_EQUAL(0, stats.allocation_failures, "Allocation failures should be reset");
    TEST_ASSERT_EQUAL(0, stats.corruption_detected, "Corruption count should be reset");
    TEST_ASSERT_EQUAL(0, stats.double_free_detected, "Double-free count should be reset");
    
    // Current state should be preserved
    TEST_ASSERT_EQUAL(3, stats.allocated_blocks, "Current allocated blocks should be preserved");
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT - 3, stats.free_blocks, "Current free blocks should be preserved");
    
    // Pool configuration should be preserved
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, stats.total_blocks, "Total blocks should be preserved");
    TEST_ASSERT(stats.block_size >= TEST_POOL_BLOCK_SIZE, "Block size should be preserved");
    
    // Clean up
    for (int i = 2; i < 5; i++) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
}

/**
 * @brief Test error statistics tracking
 */
static void test_error_statistics(void) {
    printf("\n--- Testing Error Statistics ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    pico_rtos_memory_pool_stats_t stats;
    
    // Test allocation failure tracking
    void *blocks[TEST_POOL_BLOCK_COUNT];
    
    // Allocate all blocks
    for (int i = 0; i < TEST_POOL_BLOCK_COUNT; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
        TEST_ASSERT(blocks[i] != NULL, "Allocation should succeed");
    }
    
    // Try to allocate one more (should fail)
    void *fail_block = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT(fail_block == NULL, "Allocation should fail when pool is full");
    
    // Check that failure was tracked
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    TEST_ASSERT_EQUAL(1, stats.allocation_failures, "Should track allocation failure");
    
    // Test double-free detection
    result = pico_rtos_memory_pool_free(&test_pool, blocks[0]);
    TEST_ASSERT(result, "First free should succeed");
    
    result = pico_rtos_memory_pool_free(&test_pool, blocks[0]);
    TEST_ASSERT(!result, "Double free should fail");
    
    // Check that double-free was tracked
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    TEST_ASSERT_EQUAL(1, stats.double_free_detected, "Should track double-free attempt");
    
    // Test invalid block free
    uint8_t invalid_memory[64];
    result = pico_rtos_memory_pool_free(&test_pool, invalid_memory);
    TEST_ASSERT(!result, "Free of invalid block should fail");
    
    // Clean up
    for (int i = 1; i < TEST_POOL_BLOCK_COUNT; i++) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
}

/**
 * @brief Test timing statistics (if available)
 */
static void test_timing_statistics(void) {
    printf("\n--- Testing Timing Statistics ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Perform some operations to generate timing data
    void *blocks[10];
    for (int i = 0; i < 10; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    }
    
    for (int i = 0; i < 5; i++) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
    
    // Get statistics
    pico_rtos_memory_pool_stats_t stats;
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    
    // Timing statistics should be reasonable (not zero, not extremely large)
    TEST_ASSERT(stats.avg_alloc_time_cycles > 0, "Average allocation time should be positive");
    TEST_ASSERT(stats.avg_alloc_time_cycles < 10000, "Average allocation time should be reasonable");
    TEST_ASSERT(stats.avg_free_time_cycles > 0, "Average free time should be positive");
    TEST_ASSERT(stats.avg_free_time_cycles < 10000, "Average free time should be reasonable");
    
    printf("  Average allocation time: %u cycles\n", stats.avg_alloc_time_cycles);
    printf("  Average deallocation time: %u cycles\n", stats.avg_free_time_cycles);
    
    // Clean up
    for (int i = 5; i < 10; i++) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
}

/**
 * @brief Test fragmentation ratio calculation
 */
static void test_fragmentation_ratio(void) {
    printf("\n--- Testing Fragmentation Ratio ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // For memory pools, fragmentation should always be 0.0 since blocks are fixed-size
    float fragmentation = pico_rtos_memory_pool_get_fragmentation_ratio(&test_pool);
    TEST_ASSERT_EQUAL(0.0f, fragmentation, "Memory pools should have zero fragmentation");
    
    // Allocate and free some blocks
    void *blocks[5];
    for (int i = 0; i < 5; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    }
    
    // Free every other block
    for (int i = 0; i < 5; i += 2) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
    
    // Fragmentation should still be 0.0 for memory pools
    fragmentation = pico_rtos_memory_pool_get_fragmentation_ratio(&test_pool);
    TEST_ASSERT_EQUAL(0.0f, fragmentation, "Memory pools should always have zero fragmentation");
    
    // Clean up
    for (int i = 1; i < 5; i += 2) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
}

#endif // PICO_RTOS_ENABLE_MEMORY_TRACKING

/**
 * @brief Test statistics with memory tracking disabled
 */
static void test_statistics_disabled(void) {
    printf("\n--- Testing Statistics When Disabled ---\n");
    
#if !PICO_RTOS_ENABLE_MEMORY_TRACKING
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Basic info functions should still work
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, pico_rtos_memory_pool_get_total_count(&test_pool),
                     "Total count should work without memory tracking");
    
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, pico_rtos_memory_pool_get_free_count(&test_pool),
                     "Free count should work without memory tracking");
    
    TEST_ASSERT_EQUAL(0, pico_rtos_memory_pool_get_allocated_count(&test_pool),
                     "Allocated count should work without memory tracking");
    
    printf("  Statistics functions gracefully handle disabled memory tracking\n");
#else
    printf("  Memory tracking is enabled - skipping disabled test\n");
#endif
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

/**
 * @brief Run all memory pool statistics tests
 */
int main(void) {
    printf("Starting Memory Pool Statistics Tests...\n");
    printf("======================================\n");
    
    // Initialize error system
    pico_rtos_error_init();
    
    // Run statistics tests
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
    test_basic_statistics();
    test_allocation_statistics();
    test_peak_allocation_tracking();
    test_statistics_reset();
    test_error_statistics();
    test_timing_statistics();
    test_fragmentation_ratio();
#else
    printf("Memory tracking is disabled - skipping most statistics tests\n");
#endif
    
    test_statistics_disabled();
    
    // Print summary
    print_test_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}