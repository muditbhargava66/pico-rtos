/**
 * @file memory_pool_test.c
 * @brief Unit tests for memory pool functionality
 * 
 * Comprehensive test suite for memory pool creation, allocation,
 * deallocation, validation, and statistics.
 */

#include "pico_rtos/memory_pool.h"
#include "pico_rtos/task.h"
#include "pico_rtos/error.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test configuration
#define TEST_POOL_BLOCK_SIZE 64
#define TEST_POOL_BLOCK_COUNT 10
#define TEST_POOL_MEMORY_SIZE (TEST_POOL_BLOCK_SIZE * TEST_POOL_BLOCK_COUNT + 64) // Extra for alignment

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

#define TEST_ASSERT_NOT_NULL(ptr, message) do { \
    tests_run++; \
    if ((ptr) != NULL) { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } else { \
        tests_failed++; \
        printf("FAIL: %s (pointer is NULL)\n", message); \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr, message) do { \
    tests_run++; \
    if ((ptr) == NULL) { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } else { \
        tests_failed++; \
        printf("FAIL: %s (pointer is not NULL)\n", message); \
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
    printf("\n=== Memory Pool Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    printf("================================\n\n");
}

// =============================================================================
// BASIC FUNCTIONALITY TESTS
// =============================================================================

/**
 * @brief Test memory pool initialization
 */
static void test_memory_pool_init(void) {
    printf("\n--- Testing Memory Pool Initialization ---\n");
    
    reset_test_environment();
    
    // Test successful initialization
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Memory pool initialization should succeed");
    
    // Verify pool parameters
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, pico_rtos_memory_pool_get_total_count(&test_pool),
                     "Total block count should match initialization parameter");
    
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, pico_rtos_memory_pool_get_free_count(&test_pool),
                     "All blocks should be free after initialization");
    
    TEST_ASSERT_EQUAL(0, pico_rtos_memory_pool_get_allocated_count(&test_pool),
                     "No blocks should be allocated after initialization");
    
    TEST_ASSERT(pico_rtos_memory_pool_get_block_size(&test_pool) >= TEST_POOL_BLOCK_SIZE,
               "Block size should be at least the requested size (may be larger due to alignment)");
    
    TEST_ASSERT(pico_rtos_memory_pool_is_full(&test_pool),
               "Pool should be full (all blocks free) after initialization");
    
    TEST_ASSERT(!pico_rtos_memory_pool_is_empty(&test_pool),
               "Pool should not be empty after initialization");
    
    // Test invalid parameters
    reset_test_environment();
    
    result = pico_rtos_memory_pool_init(NULL, test_memory, TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(!result, "Initialization with NULL pool should fail");
    
    result = pico_rtos_memory_pool_init(&test_pool, NULL, TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(!result, "Initialization with NULL memory should fail");
    
    result = pico_rtos_memory_pool_init(&test_pool, test_memory, 0, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(!result, "Initialization with zero block size should fail");
    
    result = pico_rtos_memory_pool_init(&test_pool, test_memory, TEST_POOL_BLOCK_SIZE, 0);
    TEST_ASSERT(!result, "Initialization with zero block count should fail");
}

/**
 * @brief Test basic allocation and deallocation
 */
static void test_basic_allocation_deallocation(void) {
    printf("\n--- Testing Basic Allocation and Deallocation ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Test single allocation
    void *block1 = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT_NOT_NULL(block1, "First allocation should succeed");
    
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT - 1, pico_rtos_memory_pool_get_free_count(&test_pool),
                     "Free count should decrease after allocation");
    
    TEST_ASSERT_EQUAL(1, pico_rtos_memory_pool_get_allocated_count(&test_pool),
                     "Allocated count should increase after allocation");
    
    // Test deallocation
    result = pico_rtos_memory_pool_free(&test_pool, block1);
    TEST_ASSERT(result, "Deallocation should succeed");
    
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, pico_rtos_memory_pool_get_free_count(&test_pool),
                     "Free count should return to original after deallocation");
    
    TEST_ASSERT_EQUAL(0, pico_rtos_memory_pool_get_allocated_count(&test_pool),
                     "Allocated count should return to zero after deallocation");
    
    // Test multiple allocations
    void *blocks[TEST_POOL_BLOCK_COUNT];
    for (int i = 0; i < TEST_POOL_BLOCK_COUNT; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
        TEST_ASSERT_NOT_NULL(blocks[i], "Multiple allocations should succeed");
    }
    
    TEST_ASSERT_EQUAL(0, pico_rtos_memory_pool_get_free_count(&test_pool),
                     "No blocks should be free after allocating all");
    
    TEST_ASSERT(pico_rtos_memory_pool_is_empty(&test_pool),
               "Pool should be empty after allocating all blocks");
    
    // Test allocation when pool is empty
    void *block_fail = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT_NULL(block_fail, "Allocation should fail when pool is empty");
    
    // Free all blocks
    for (int i = 0; i < TEST_POOL_BLOCK_COUNT; i++) {
        result = pico_rtos_memory_pool_free(&test_pool, blocks[i]);
        TEST_ASSERT(result, "Deallocation should succeed");
    }
    
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, pico_rtos_memory_pool_get_free_count(&test_pool),
                     "All blocks should be free after deallocation");
}

/**
 * @brief Test allocation and deallocation patterns
 */
static void test_allocation_patterns(void) {
    printf("\n--- Testing Allocation Patterns ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Test interleaved allocation and deallocation
    void *block1 = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    void *block2 = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    void *block3 = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    
    TEST_ASSERT_NOT_NULL(block1, "First allocation should succeed");
    TEST_ASSERT_NOT_NULL(block2, "Second allocation should succeed");
    TEST_ASSERT_NOT_NULL(block3, "Third allocation should succeed");
    
    // Free middle block
    result = pico_rtos_memory_pool_free(&test_pool, block2);
    TEST_ASSERT(result, "Free middle block should succeed");
    
    // Allocate again (should reuse freed block)
    void *block4 = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT_NOT_NULL(block4, "Allocation after partial free should succeed");
    
    // Verify all blocks are different
    TEST_ASSERT(block1 != block3 && block1 != block4 && block3 != block4,
               "All allocated blocks should have different addresses");
    
    // Clean up
    pico_rtos_memory_pool_free(&test_pool, block1);
    pico_rtos_memory_pool_free(&test_pool, block3);
    pico_rtos_memory_pool_free(&test_pool, block4);
}

/**
 * @brief Test error conditions and validation
 */
static void test_error_conditions(void) {
    printf("\n--- Testing Error Conditions ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Test invalid free operations
    result = pico_rtos_memory_pool_free(&test_pool, NULL);
    TEST_ASSERT(!result, "Freeing NULL pointer should fail");
    
    // Test freeing invalid pointer (not from pool)
    uint8_t invalid_memory[64];
    result = pico_rtos_memory_pool_free(&test_pool, invalid_memory);
    TEST_ASSERT(!result, "Freeing pointer not from pool should fail");
    
    // Test double free
    void *block = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT_NOT_NULL(block, "Allocation should succeed");
    
    result = pico_rtos_memory_pool_free(&test_pool, block);
    TEST_ASSERT(result, "First free should succeed");
    
    result = pico_rtos_memory_pool_free(&test_pool, block);
    TEST_ASSERT(!result, "Double free should fail");
    
    // Test operations on uninitialized pool
    pico_rtos_memory_pool_t uninit_pool;
    memset(&uninit_pool, 0, sizeof(uninit_pool));
    
    void *fail_block = pico_rtos_memory_pool_alloc(&uninit_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT_NULL(fail_block, "Allocation from uninitialized pool should fail");
    
    result = pico_rtos_memory_pool_free(&uninit_pool, block);
    TEST_ASSERT(!result, "Free to uninitialized pool should fail");
}

/**
 * @brief Test pool validation functionality
 */
static void test_pool_validation(void) {
    printf("\n--- Testing Pool Validation ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Test validation of valid pool
    result = pico_rtos_memory_pool_validate(&test_pool);
    TEST_ASSERT(result, "Validation of valid pool should succeed");
    
    // Test pointer ownership checking
    void *block = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT_NOT_NULL(block, "Allocation should succeed");
    
    result = pico_rtos_memory_pool_contains_pointer(&test_pool, block);
    TEST_ASSERT(result, "Pool should contain allocated block");
    
    uint8_t external_memory[64];
    result = pico_rtos_memory_pool_contains_pointer(&test_pool, external_memory);
    TEST_ASSERT(!result, "Pool should not contain external pointer");
    
    // Test block allocation status
    result = pico_rtos_memory_pool_is_block_allocated(&test_pool, block);
    TEST_ASSERT(result, "Block should be marked as allocated");
    
    pico_rtos_memory_pool_free(&test_pool, block);
    
    result = pico_rtos_memory_pool_is_block_allocated(&test_pool, block);
    TEST_ASSERT(!result, "Block should be marked as free after deallocation");
}

#if PICO_RTOS_ENABLE_MEMORY_TRACKING
/**
 * @brief Test statistics functionality
 */
static void test_statistics(void) {
    printf("\n--- Testing Statistics ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Get initial statistics
    pico_rtos_memory_pool_stats_t stats;
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, stats.total_blocks, "Total blocks should match");
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT, stats.free_blocks, "All blocks should be free initially");
    TEST_ASSERT_EQUAL(0, stats.allocated_blocks, "No blocks should be allocated initially");
    TEST_ASSERT_EQUAL(0, stats.allocation_count, "No allocations should have occurred");
    TEST_ASSERT_EQUAL(0, stats.deallocation_count, "No deallocations should have occurred");
    
    // Perform some allocations and check statistics
    void *blocks[3];
    for (int i = 0; i < 3; i++) {
        blocks[i] = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
        TEST_ASSERT_NOT_NULL(blocks[i], "Allocation should succeed");
    }
    
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics should succeed");
    
    TEST_ASSERT_EQUAL(3, stats.allocation_count, "Allocation count should be 3");
    TEST_ASSERT_EQUAL(3, stats.allocated_blocks, "3 blocks should be allocated");
    TEST_ASSERT_EQUAL(TEST_POOL_BLOCK_COUNT - 3, stats.free_blocks, "Free blocks should decrease");
    TEST_ASSERT_EQUAL(3, stats.peak_allocated, "Peak allocated should be 3");
    
    // Test peak tracking
    void *block4 = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT_NOT_NULL(block4, "Fourth allocation should succeed");
    
    uint32_t peak = pico_rtos_memory_pool_get_peak_allocated(&test_pool);
    TEST_ASSERT_EQUAL(4, peak, "Peak allocated should be 4");
    
    // Free one block and check that peak is maintained
    pico_rtos_memory_pool_free(&test_pool, block4);
    
    peak = pico_rtos_memory_pool_get_peak_allocated(&test_pool);
    TEST_ASSERT_EQUAL(4, peak, "Peak should be maintained after freeing");
    
    // Test statistics reset
    pico_rtos_memory_pool_reset_stats(&test_pool);
    
    result = pico_rtos_memory_pool_get_stats(&test_pool, &stats);
    TEST_ASSERT(result, "Getting statistics after reset should succeed");
    
    TEST_ASSERT_EQUAL(0, stats.allocation_count, "Allocation count should be reset");
    TEST_ASSERT_EQUAL(0, stats.deallocation_count, "Deallocation count should be reset");
    TEST_ASSERT_EQUAL(0, stats.peak_allocated, "Peak allocated should be reset");
    
    // Current state should be preserved
    TEST_ASSERT_EQUAL(3, stats.allocated_blocks, "Current allocated blocks should be preserved");
    
    // Clean up
    for (int i = 0; i < 3; i++) {
        pico_rtos_memory_pool_free(&test_pool, blocks[i]);
    }
}
#endif // PICO_RTOS_ENABLE_MEMORY_TRACKING

/**
 * @brief Test utility functions
 */
static void test_utility_functions(void) {
    printf("\n--- Testing Utility Functions ---\n");
    
    // Test size calculation
    uint32_t required_size = pico_rtos_memory_pool_calculate_size(TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(required_size > 0, "Required size calculation should return positive value");
    TEST_ASSERT(required_size >= TEST_POOL_BLOCK_SIZE * TEST_POOL_BLOCK_COUNT,
               "Required size should be at least block_size * block_count");
    
    // Test aligned block size
    uint32_t aligned_size = pico_rtos_memory_pool_get_aligned_block_size(TEST_POOL_BLOCK_SIZE);
    TEST_ASSERT(aligned_size >= TEST_POOL_BLOCK_SIZE, "Aligned size should be at least requested size");
    TEST_ASSERT(aligned_size % PICO_RTOS_MEMORY_POOL_ALIGNMENT == 0, "Aligned size should be properly aligned");
    
    // Test max blocks calculation
    uint32_t max_blocks = pico_rtos_memory_pool_calculate_max_blocks(TEST_POOL_MEMORY_SIZE, TEST_POOL_BLOCK_SIZE);
    TEST_ASSERT(max_blocks >= TEST_POOL_BLOCK_COUNT, "Max blocks should be at least our test count");
    
    // Test invalid parameters
    uint32_t invalid_size = pico_rtos_memory_pool_calculate_size(0, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT_EQUAL(0, invalid_size, "Size calculation with zero block size should return 0");
    
    invalid_size = pico_rtos_memory_pool_calculate_size(TEST_POOL_BLOCK_SIZE, 0);
    TEST_ASSERT_EQUAL(0, invalid_size, "Size calculation with zero block count should return 0");
    
    uint32_t invalid_blocks = pico_rtos_memory_pool_calculate_max_blocks(0, TEST_POOL_BLOCK_SIZE);
    TEST_ASSERT_EQUAL(0, invalid_blocks, "Max blocks calculation with zero memory size should return 0");
    
    invalid_blocks = pico_rtos_memory_pool_calculate_max_blocks(TEST_POOL_MEMORY_SIZE, 0);
    TEST_ASSERT_EQUAL(0, invalid_blocks, "Max blocks calculation with zero block size should return 0");
}

/**
 * @brief Test pool destruction
 */
static void test_pool_destruction(void) {
    printf("\n--- Testing Pool Destruction ---\n");
    
    reset_test_environment();
    
    // Initialize pool
    bool result = pico_rtos_memory_pool_init(&test_pool, test_memory, 
                                           TEST_POOL_BLOCK_SIZE, TEST_POOL_BLOCK_COUNT);
    TEST_ASSERT(result, "Pool initialization should succeed");
    
    // Allocate some blocks
    void *block1 = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    void *block2 = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    
    TEST_ASSERT_NOT_NULL(block1, "First allocation should succeed");
    TEST_ASSERT_NOT_NULL(block2, "Second allocation should succeed");
    
    // Destroy pool
    pico_rtos_memory_pool_destroy(&test_pool);
    
    // Test that operations fail after destruction
    void *fail_block = pico_rtos_memory_pool_alloc(&test_pool, PICO_RTOS_NO_WAIT);
    TEST_ASSERT_NULL(fail_block, "Allocation should fail after destruction");
    
    result = pico_rtos_memory_pool_free(&test_pool, block1);
    TEST_ASSERT(!result, "Free should fail after destruction");
    
    // Test that info functions return safe values
    TEST_ASSERT_EQUAL(0, pico_rtos_memory_pool_get_free_count(&test_pool),
                     "Free count should be 0 after destruction");
    
    TEST_ASSERT_EQUAL(0, pico_rtos_memory_pool_get_total_count(&test_pool),
                     "Total count should be 0 after destruction");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

/**
 * @brief Run all memory pool tests
 */
int main(void) {
    printf("Starting Memory Pool Tests...\n");
    printf("==============================\n");
    
    // Initialize error system
    pico_rtos_error_init();
    
    // Run all tests
    test_memory_pool_init();
    test_basic_allocation_deallocation();
    test_allocation_patterns();
    test_error_conditions();
    test_pool_validation();
    
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
    test_statistics();
#endif
    
    test_utility_functions();
    test_pool_destruction();
    
    // Print summary
    print_test_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}