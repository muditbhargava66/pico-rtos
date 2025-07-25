/**
 * @file memory_pool_performance_test.c
 * @brief Performance tests for memory pool O(1) characteristics
 * 
 * This test validates that memory pool allocation and deallocation
 * operations maintain O(1) performance characteristics regardless
 * of pool size and allocation patterns.
 */

#include "pico_rtos/memory_pool.h"
#include "pico_rtos/error.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

// Test configuration
#define SMALL_POOL_SIZE 10
#define MEDIUM_POOL_SIZE 50
#define LARGE_POOL_SIZE 200
#define BLOCK_SIZE 64
#define PERFORMANCE_ITERATIONS 1000

// Test memory buffers
static uint8_t small_memory[SMALL_POOL_SIZE * BLOCK_SIZE + 64] __attribute__((aligned(4)));
static uint8_t medium_memory[MEDIUM_POOL_SIZE * BLOCK_SIZE + 64] __attribute__((aligned(4)));
static uint8_t large_memory[LARGE_POOL_SIZE * BLOCK_SIZE + 64] __attribute__((aligned(4)));

// Test pools
static pico_rtos_memory_pool_t small_pool;
static pico_rtos_memory_pool_t medium_pool;
static pico_rtos_memory_pool_t large_pool;

// Performance measurement structure
typedef struct {
    uint32_t min_cycles;
    uint32_t max_cycles;
    uint64_t total_cycles;
    uint32_t iterations;
    double avg_cycles;
} performance_stats_t;

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

#define TEST_ASSERT_PERFORMANCE(condition, message, actual, expected) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("PASS: %s (actual: %.2f, expected: < %.2f)\n", message, actual, expected); \
    } else { \
        tests_failed++; \
        printf("FAIL: %s (actual: %.2f, expected: < %.2f)\n", message, actual, expected); \
    } \
} while(0)

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Initialize performance statistics
 */
static void init_performance_stats(performance_stats_t *stats) {
    stats->min_cycles = UINT32_MAX;
    stats->max_cycles = 0;
    stats->total_cycles = 0;
    stats->iterations = 0;
    stats->avg_cycles = 0.0;
}

/**
 * @brief Update performance statistics with a new measurement
 */
static void update_performance_stats(performance_stats_t *stats, uint32_t cycles) {
    if (cycles < stats->min_cycles) {
        stats->min_cycles = cycles;
    }
    if (cycles > stats->max_cycles) {
        stats->max_cycles = cycles;
    }
    stats->total_cycles += cycles;
    stats->iterations++;
    stats->avg_cycles = (double)stats->total_cycles / stats->iterations;
}

/**
 * @brief Print performance statistics
 */
static void print_performance_stats(const char *operation, const char *pool_size, 
                                   const performance_stats_t *stats) {
    printf("  %s (%s pool): min=%u, max=%u, avg=%.2f cycles\n", 
           operation, pool_size, stats->min_cycles, stats->max_cycles, stats->avg_cycles);
}

/**
 * @brief Initialize all test pools
 */
static bool initialize_test_pools(void) {
    bool result = true;
    
    result &= pico_rtos_memory_pool_init(&small_pool, small_memory, BLOCK_SIZE, SMALL_POOL_SIZE);
    result &= pico_rtos_memory_pool_init(&medium_pool, medium_memory, BLOCK_SIZE, MEDIUM_POOL_SIZE);
    result &= pico_rtos_memory_pool_init(&large_pool, large_memory, BLOCK_SIZE, LARGE_POOL_SIZE);
    
    return result;
}

/**
 * @brief Destroy all test pools
 */
static void destroy_test_pools(void) {
    pico_rtos_memory_pool_destroy(&small_pool);
    pico_rtos_memory_pool_destroy(&medium_pool);
    pico_rtos_memory_pool_destroy(&large_pool);
}

// =============================================================================
// PERFORMANCE TESTS
// =============================================================================

/**
 * @brief Test allocation performance across different pool sizes
 */
static void test_allocation_performance(void) {
    printf("\n--- Testing Allocation Performance ---\n");
    
    performance_stats_t small_stats, medium_stats, large_stats;
    init_performance_stats(&small_stats);
    init_performance_stats(&medium_stats);
    init_performance_stats(&large_stats);
    
    // Test allocation performance for each pool size
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        uint32_t start_time, end_time;
        void *block;
        
        // Small pool allocation
        start_time = time_us_32();
        block = pico_rtos_memory_pool_alloc(&small_pool, PICO_RTOS_NO_WAIT);
        end_time = time_us_32();
        if (block) {
            update_performance_stats(&small_stats, end_time - start_time);
            pico_rtos_memory_pool_free(&small_pool, block);
        }
        
        // Medium pool allocation
        start_time = time_us_32();
        block = pico_rtos_memory_pool_alloc(&medium_pool, PICO_RTOS_NO_WAIT);
        end_time = time_us_32();
        if (block) {
            update_performance_stats(&medium_stats, end_time - start_time);
            pico_rtos_memory_pool_free(&medium_pool, block);
        }
        
        // Large pool allocation
        start_time = time_us_32();
        block = pico_rtos_memory_pool_alloc(&large_pool, PICO_RTOS_NO_WAIT);
        end_time = time_us_32();
        if (block) {
            update_performance_stats(&large_stats, end_time - start_time);
            pico_rtos_memory_pool_free(&large_pool, block);
        }
    }
    
    // Print results
    print_performance_stats("Allocation", "small", &small_stats);
    print_performance_stats("Allocation", "medium", &medium_stats);
    print_performance_stats("Allocation", "large", &large_stats);
    
    // Verify O(1) characteristics - allocation time should not significantly increase with pool size
    // Allow for some variance due to measurement noise and cache effects
    double size_factor_medium = (double)MEDIUM_POOL_SIZE / SMALL_POOL_SIZE;
    double size_factor_large = (double)LARGE_POOL_SIZE / SMALL_POOL_SIZE;
    
    // Performance should not scale linearly with pool size for O(1) operations
    // We allow up to 2x variance to account for cache effects and measurement noise
    TEST_ASSERT_PERFORMANCE(medium_stats.avg_cycles < small_stats.avg_cycles * 2.0,
                           "Medium pool allocation should maintain O(1) performance",
                           medium_stats.avg_cycles / small_stats.avg_cycles, 2.0);
    
    TEST_ASSERT_PERFORMANCE(large_stats.avg_cycles < small_stats.avg_cycles * 2.0,
                           "Large pool allocation should maintain O(1) performance",
                           large_stats.avg_cycles / small_stats.avg_cycles, 2.0);
}

/**
 * @brief Test deallocation performance across different pool sizes
 */
static void test_deallocation_performance(void) {
    printf("\n--- Testing Deallocation Performance ---\n");
    
    performance_stats_t small_stats, medium_stats, large_stats;
    init_performance_stats(&small_stats);
    init_performance_stats(&medium_stats);
    init_performance_stats(&large_stats);
    
    // Pre-allocate blocks for deallocation testing
    void *small_blocks[PERFORMANCE_ITERATIONS];
    void *medium_blocks[PERFORMANCE_ITERATIONS];
    void *large_blocks[PERFORMANCE_ITERATIONS];
    
    // Allocate blocks
    for (int i = 0; i < PERFORMANCE_ITERATIONS && i < SMALL_POOL_SIZE; i++) {
        small_blocks[i] = pico_rtos_memory_pool_alloc(&small_pool, PICO_RTOS_NO_WAIT);
    }
    for (int i = 0; i < PERFORMANCE_ITERATIONS && i < MEDIUM_POOL_SIZE; i++) {
        medium_blocks[i] = pico_rtos_memory_pool_alloc(&medium_pool, PICO_RTOS_NO_WAIT);
    }
    for (int i = 0; i < PERFORMANCE_ITERATIONS && i < LARGE_POOL_SIZE; i++) {
        large_blocks[i] = pico_rtos_memory_pool_alloc(&large_pool, PICO_RTOS_NO_WAIT);
    }
    
    // Test deallocation performance
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        uint32_t start_time, end_time;
        
        // Small pool deallocation
        if (i < SMALL_POOL_SIZE && small_blocks[i]) {
            start_time = time_us_32();
            pico_rtos_memory_pool_free(&small_pool, small_blocks[i]);
            end_time = time_us_32();
            update_performance_stats(&small_stats, end_time - start_time);
        }
        
        // Medium pool deallocation
        if (i < MEDIUM_POOL_SIZE && medium_blocks[i]) {
            start_time = time_us_32();
            pico_rtos_memory_pool_free(&medium_pool, medium_blocks[i]);
            end_time = time_us_32();
            update_performance_stats(&medium_stats, end_time - start_time);
        }
        
        // Large pool deallocation
        if (i < LARGE_POOL_SIZE && large_blocks[i]) {
            start_time = time_us_32();
            pico_rtos_memory_pool_free(&large_pool, large_blocks[i]);
            end_time = time_us_32();
            update_performance_stats(&large_stats, end_time - start_time);
        }
    }
    
    // Print results
    print_performance_stats("Deallocation", "small", &small_stats);
    print_performance_stats("Deallocation", "medium", &medium_stats);
    print_performance_stats("Deallocation", "large", &large_stats);
    
    // Verify O(1) characteristics for deallocation
    TEST_ASSERT_PERFORMANCE(medium_stats.avg_cycles < small_stats.avg_cycles * 2.0,
                           "Medium pool deallocation should maintain O(1) performance",
                           medium_stats.avg_cycles / small_stats.avg_cycles, 2.0);
    
    TEST_ASSERT_PERFORMANCE(large_stats.avg_cycles < small_stats.avg_cycles * 2.0,
                           "Large pool deallocation should maintain O(1) performance",
                           large_stats.avg_cycles / small_stats.avg_cycles, 2.0);
}

/**
 * @brief Test performance under different allocation patterns
 */
static void test_allocation_patterns_performance(void) {
    printf("\n--- Testing Allocation Pattern Performance ---\n");
    
    performance_stats_t sequential_stats, random_stats, interleaved_stats;
    init_performance_stats(&sequential_stats);
    init_performance_stats(&random_stats);
    init_performance_stats(&interleaved_stats);
    
    void *blocks[MEDIUM_POOL_SIZE];
    
    // Test 1: Sequential allocation/deallocation
    for (int iteration = 0; iteration < 10; iteration++) {
        // Allocate all blocks sequentially
        for (int i = 0; i < MEDIUM_POOL_SIZE; i++) {
            uint32_t start_time = time_us_32();
            blocks[i] = pico_rtos_memory_pool_alloc(&medium_pool, PICO_RTOS_NO_WAIT);
            uint32_t end_time = time_us_32();
            if (blocks[i]) {
                update_performance_stats(&sequential_stats, end_time - start_time);
            }
        }
        
        // Deallocate all blocks sequentially
        for (int i = 0; i < MEDIUM_POOL_SIZE; i++) {
            if (blocks[i]) {
                pico_rtos_memory_pool_free(&medium_pool, blocks[i]);
                blocks[i] = NULL;
            }
        }
    }
    
    // Test 2: Random allocation/deallocation pattern
    for (int iteration = 0; iteration < 100; iteration++) {
        int index = rand() % MEDIUM_POOL_SIZE;
        
        if (blocks[index] == NULL) {
            // Allocate
            uint32_t start_time = time_us_32();
            blocks[index] = pico_rtos_memory_pool_alloc(&medium_pool, PICO_RTOS_NO_WAIT);
            uint32_t end_time = time_us_32();
            if (blocks[index]) {
                update_performance_stats(&random_stats, end_time - start_time);
            }
        } else {
            // Deallocate
            pico_rtos_memory_pool_free(&medium_pool, blocks[index]);
            blocks[index] = NULL;
        }
    }
    
    // Clean up remaining blocks
    for (int i = 0; i < MEDIUM_POOL_SIZE; i++) {
        if (blocks[i]) {
            pico_rtos_memory_pool_free(&medium_pool, blocks[i]);
            blocks[i] = NULL;
        }
    }
    
    // Test 3: Interleaved allocation/deallocation
    for (int iteration = 0; iteration < 50; iteration++) {
        // Allocate 3 blocks
        void *temp_blocks[3];
        for (int i = 0; i < 3; i++) {
            uint32_t start_time = time_us_32();
            temp_blocks[i] = pico_rtos_memory_pool_alloc(&medium_pool, PICO_RTOS_NO_WAIT);
            uint32_t end_time = time_us_32();
            if (temp_blocks[i]) {
                update_performance_stats(&interleaved_stats, end_time - start_time);
            }
        }
        
        // Free middle block
        if (temp_blocks[1]) {
            pico_rtos_memory_pool_free(&medium_pool, temp_blocks[1]);
        }
        
        // Allocate another block (should reuse freed block)
        uint32_t start_time = time_us_32();
        void *reuse_block = pico_rtos_memory_pool_alloc(&medium_pool, PICO_RTOS_NO_WAIT);
        uint32_t end_time = time_us_32();
        if (reuse_block) {
            update_performance_stats(&interleaved_stats, end_time - start_time);
        }
        
        // Clean up
        if (temp_blocks[0]) pico_rtos_memory_pool_free(&medium_pool, temp_blocks[0]);
        if (temp_blocks[2]) pico_rtos_memory_pool_free(&medium_pool, temp_blocks[2]);
        if (reuse_block) pico_rtos_memory_pool_free(&medium_pool, reuse_block);
    }
    
    // Print results
    print_performance_stats("Sequential", "pattern", &sequential_stats);
    print_performance_stats("Random", "pattern", &random_stats);
    print_performance_stats("Interleaved", "pattern", &interleaved_stats);
    
    // All patterns should have similar performance for O(1) operations
    double max_variance = 3.0; // Allow 3x variance for different patterns
    
    TEST_ASSERT_PERFORMANCE(random_stats.avg_cycles < sequential_stats.avg_cycles * max_variance,
                           "Random pattern should maintain O(1) performance",
                           random_stats.avg_cycles / sequential_stats.avg_cycles, max_variance);
    
    TEST_ASSERT_PERFORMANCE(interleaved_stats.avg_cycles < sequential_stats.avg_cycles * max_variance,
                           "Interleaved pattern should maintain O(1) performance",
                           interleaved_stats.avg_cycles / sequential_stats.avg_cycles, max_variance);
}

/**
 * @brief Test block validation performance
 */
static void test_validation_performance(void) {
    printf("\n--- Testing Block Validation Performance ---\n");
    
    performance_stats_t validation_stats;
    init_performance_stats(&validation_stats);
    
    // Allocate some blocks for validation testing
    void *test_blocks[10];
    for (int i = 0; i < 10; i++) {
        test_blocks[i] = pico_rtos_memory_pool_alloc(&medium_pool, PICO_RTOS_NO_WAIT);
    }
    
    // Test validation performance
    for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) {
        uint32_t start_time = time_us_32();
        bool valid = pico_rtos_memory_pool_validate(&medium_pool);
        uint32_t end_time = time_us_32();
        
        if (valid) {
            update_performance_stats(&validation_stats, end_time - start_time);
        }
    }
    
    // Clean up
    for (int i = 0; i < 10; i++) {
        if (test_blocks[i]) {
            pico_rtos_memory_pool_free(&medium_pool, test_blocks[i]);
        }
    }
    
    print_performance_stats("Validation", "operation", &validation_stats);
    
    // Validation should be reasonably fast (less than 1000 cycles on average)
    TEST_ASSERT_PERFORMANCE(validation_stats.avg_cycles < 1000.0,
                           "Pool validation should be efficient",
                           validation_stats.avg_cycles, 1000.0);
}

/**
 * @brief Print test results summary
 */
static void print_test_summary(void) {
    printf("\n=== Memory Pool Performance Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    printf("============================================\n\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

/**
 * @brief Run all memory pool performance tests
 */
int main(void) {
    printf("Starting Memory Pool Performance Tests...\n");
    printf("========================================\n");
    
    // Initialize error system
    pico_rtos_error_init();
    
    // Initialize test pools
    if (!initialize_test_pools()) {
        printf("FATAL: Failed to initialize test pools\n");
        return 1;
    }
    
    printf("Test Configuration:\n");
    printf("  Small pool: %d blocks\n", SMALL_POOL_SIZE);
    printf("  Medium pool: %d blocks\n", MEDIUM_POOL_SIZE);
    printf("  Large pool: %d blocks\n", LARGE_POOL_SIZE);
    printf("  Block size: %d bytes\n", BLOCK_SIZE);
    printf("  Performance iterations: %d\n", PERFORMANCE_ITERATIONS);
    printf("\n");
    
    // Run performance tests
    test_allocation_performance();
    test_deallocation_performance();
    test_allocation_patterns_performance();
    test_validation_performance();
    
    // Clean up
    destroy_test_pools();
    
    // Print summary
    print_test_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}