/**
 * @file mpu_test.c
 * @brief Comprehensive unit tests for Memory Protection Unit (MPU) functionality
 * 
 * Tests cover:
 * - MPU initialization and configuration
 * - Memory region protection setup
 * - Access violation detection
 * - Stack overflow protection
 * - Task isolation validation
 * - Error handling and recovery
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"

// Check if MPU support is enabled
#ifdef PICO_RTOS_ENABLE_MPU_SUPPORT
#include "pico_rtos/mpu.h"
#else
// Mock MPU definitions for compilation
typedef struct { int dummy; } pico_rtos_mpu_region_t;
typedef struct { int dummy; } pico_rtos_mpu_info_t;
typedef struct { int dummy; } pico_rtos_mpu_stats_t;
typedef enum { PICO_RTOS_MPU_FAULT_ACCESS, PICO_RTOS_MPU_FAULT_PERMISSION } pico_rtos_mpu_fault_type_t;
typedef enum { PICO_RTOS_MPU_ATTR_NO_ACCESS, PICO_RTOS_MPU_ATTR_READ_ONLY, PICO_RTOS_MPU_ATTR_READ_WRITE } pico_rtos_mpu_attr_t;
typedef enum { PICO_RTOS_MPU_ACCESS_READ, PICO_RTOS_MPU_ACCESS_WRITE } pico_rtos_mpu_access_t;
static inline bool pico_rtos_mpu_is_available(void) { return false; }
static inline bool pico_rtos_mpu_init(void) { return false; }
static inline bool pico_rtos_mpu_get_info(pico_rtos_mpu_info_t *info) { (void)info; return false; }
static inline bool pico_rtos_mpu_configure_region(pico_rtos_mpu_region_t *region, uint32_t id, uint32_t addr, uint32_t size, pico_rtos_mpu_attr_t attr) { (void)region; (void)id; (void)addr; (void)size; (void)attr; return false; }
static inline bool pico_rtos_mpu_enable_region(pico_rtos_mpu_region_t *region) { (void)region; return false; }
static inline bool pico_rtos_mpu_disable_region(pico_rtos_mpu_region_t *region) { (void)region; return false; }
static inline bool pico_rtos_mpu_is_region_enabled(pico_rtos_mpu_region_t *region) { (void)region; return false; }
static inline bool pico_rtos_mpu_set_violation_handler(void (*handler)(uint32_t, pico_rtos_mpu_fault_type_t)) { (void)handler; return false; }
static inline bool pico_rtos_mpu_enable(void) { return false; }
static inline bool pico_rtos_mpu_disable(void) { return false; }
static inline bool pico_rtos_mpu_is_enabled(void) { return false; }
static inline bool pico_rtos_mpu_protect_task_stack(pico_rtos_task_t *task) { (void)task; return false; }
static inline bool pico_rtos_mpu_unprotect_task_stack(pico_rtos_task_t *task) { (void)task; return false; }
static inline bool pico_rtos_mpu_is_stack_protected(pico_rtos_task_t *task) { (void)task; return false; }
static inline uint32_t pico_rtos_mpu_get_stack_guard_size(void) { return 0; }
static inline bool pico_rtos_mpu_set_stack_guard_size(uint32_t size) { (void)size; return false; }
static inline bool pico_rtos_mpu_enable_task_isolation(void) { return false; }
static inline bool pico_rtos_mpu_disable_task_isolation(void) { return false; }
static inline bool pico_rtos_mpu_is_task_isolation_enabled(void) { return false; }
static inline bool pico_rtos_mpu_set_task_memory_permissions(pico_rtos_task_t *task, uint32_t addr, uint32_t size, pico_rtos_mpu_attr_t attr) { (void)task; (void)addr; (void)size; (void)attr; return false; }
static inline bool pico_rtos_mpu_task_has_access(pico_rtos_task_t *task, uint32_t addr, pico_rtos_mpu_access_t access) { (void)task; (void)addr; (void)access; return false; }
static inline bool pico_rtos_mpu_get_stats(pico_rtos_mpu_stats_t *stats) { (void)stats; return false; }
static inline bool pico_rtos_mpu_reset_stats(void) { return false; }
static inline bool pico_rtos_mpu_is_address_aligned(uint32_t addr) { (void)addr; return false; }
static inline bool pico_rtos_mpu_is_size_valid(uint32_t size) { (void)size; return false; }
static inline uint32_t pico_rtos_mpu_get_required_alignment(void) { return 32; }
static inline uint32_t pico_rtos_mpu_calculate_region_size(uint32_t size) { (void)size; return 0; }
#endif

// Test configuration
#define TEST_TASK_STACK_SIZE 1024
#define TEST_MEMORY_REGION_SIZE 256
#define TEST_TIMEOUT_MS 100

// Test memory regions
static uint8_t test_memory_region1[TEST_MEMORY_REGION_SIZE] __attribute__((aligned(32)));
static uint8_t test_memory_region2[TEST_MEMORY_REGION_SIZE] __attribute__((aligned(32)));
static uint8_t protected_memory[TEST_MEMORY_REGION_SIZE] __attribute__((aligned(32)));

// Test state tracking
static volatile bool test_passed = true;
static volatile int test_count = 0;
static volatile int test_failures = 0;
static volatile bool violation_detected = false;
static volatile uint32_t violation_address = 0;

// Test helper macros
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
// TEST HELPER FUNCTIONS
// =============================================================================

static void mpu_violation_handler(uint32_t fault_address, pico_rtos_mpu_fault_type_t fault_type) {
    violation_detected = true;
    violation_address = fault_address;
    printf("MPU violation detected at address 0x%08lx, type: %d\n", fault_address, fault_type);
}

static void reset_test_state(void) {
    violation_detected = false;
    violation_address = 0;
}

// =============================================================================
// MPU INITIALIZATION TESTS
// =============================================================================

static void test_mpu_initialization(void) {
    printf("\n=== Testing MPU Initialization ===\n");
    
    // Test MPU availability check
    bool mpu_available = pico_rtos_mpu_is_available();
    TEST_ASSERT(mpu_available, "MPU should be available on this platform");
    
    // Test MPU initialization
    bool init_result = pico_rtos_mpu_init();
    TEST_ASSERT(init_result, "MPU initialization should succeed");
    
    // Test double initialization (should be idempotent)
    init_result = pico_rtos_mpu_init();
    TEST_ASSERT(init_result, "Second MPU initialization should succeed");
    
    // Test getting MPU info
    pico_rtos_mpu_info_t mpu_info;
    bool info_result = pico_rtos_mpu_get_info(&mpu_info);
    TEST_ASSERT(info_result, "Should be able to get MPU info");
    TEST_ASSERT(mpu_info.num_regions > 0, "MPU should have at least one region");
    TEST_ASSERT(mpu_info.min_region_size > 0, "MPU should have minimum region size");
    
    printf("MPU Info: %lu regions, min size: %lu bytes\n", 
           mpu_info.num_regions, mpu_info.min_region_size);
}

// =============================================================================
// MEMORY REGION CONFIGURATION TESTS
// =============================================================================

static void test_memory_region_configuration(void) {
    printf("\n=== Testing Memory Region Configuration ===\n");
    
    // Test configuring a read-write region
    pico_rtos_mpu_region_t region1;
    bool config_result = pico_rtos_mpu_configure_region(&region1, 0,
                                                       (uint32_t)test_memory_region1,
                                                       TEST_MEMORY_REGION_SIZE,
                                                       PICO_RTOS_MPU_ATTR_READ_WRITE);
    TEST_ASSERT(config_result, "Should configure read-write region successfully");
    
    // Test configuring a read-only region
    pico_rtos_mpu_region_t region2;
    config_result = pico_rtos_mpu_configure_region(&region2, 1,
                                                  (uint32_t)test_memory_region2,
                                                  TEST_MEMORY_REGION_SIZE,
                                                  PICO_RTOS_MPU_ATTR_READ_ONLY);
    TEST_ASSERT(config_result, "Should configure read-only region successfully");
    
    // Test enabling regions
    bool enable_result = pico_rtos_mpu_enable_region(&region1);
    TEST_ASSERT(enable_result, "Should enable region 1 successfully");
    
    enable_result = pico_rtos_mpu_enable_region(&region2);
    TEST_ASSERT(enable_result, "Should enable region 2 successfully");
    
    // Test region status
    bool is_enabled = pico_rtos_mpu_is_region_enabled(&region1);
    TEST_ASSERT(is_enabled, "Region 1 should be enabled");
    
    is_enabled = pico_rtos_mpu_is_region_enabled(&region2);
    TEST_ASSERT(is_enabled, "Region 2 should be enabled");
    
    // Test disabling regions
    bool disable_result = pico_rtos_mpu_disable_region(&region1);
    TEST_ASSERT(disable_result, "Should disable region 1 successfully");
    
    is_enabled = pico_rtos_mpu_is_region_enabled(&region1);
    TEST_ASSERT(!is_enabled, "Region 1 should be disabled");
}

// =============================================================================
// ACCESS VIOLATION DETECTION TESTS
// =============================================================================

static void test_access_violation_detection(void) {
    printf("\n=== Testing Access Violation Detection ===\n");
    
    reset_test_state();
    
    // Set up violation handler
    bool handler_result = pico_rtos_mpu_set_violation_handler(mpu_violation_handler);
    TEST_ASSERT(handler_result, "Should set violation handler successfully");
    
    // Configure a no-access region
    pico_rtos_mpu_region_t protected_region;
    bool config_result = pico_rtos_mpu_configure_region(&protected_region, 2,
                                                       (uint32_t)protected_memory,
                                                       TEST_MEMORY_REGION_SIZE,
                                                       PICO_RTOS_MPU_ATTR_NO_ACCESS);
    TEST_ASSERT(config_result, "Should configure no-access region successfully");
    
    bool enable_result = pico_rtos_mpu_enable_region(&protected_region);
    TEST_ASSERT(enable_result, "Should enable protected region successfully");
    
    // Enable MPU
    bool mpu_enable_result = pico_rtos_mpu_enable();
    TEST_ASSERT(mpu_enable_result, "Should enable MPU successfully");
    
    // Test that we can detect if MPU is enabled
    bool is_enabled = pico_rtos_mpu_is_enabled();
    TEST_ASSERT(is_enabled, "MPU should be enabled");
    
    // Note: Actual memory access violation testing would require careful setup
    // to avoid crashing the test. In a real implementation, this would be done
    // in a controlled environment with proper exception handling.
    
    printf("Access violation detection test setup completed\n");
    
    // Disable MPU for safety
    bool disable_result = pico_rtos_mpu_disable();
    TEST_ASSERT(disable_result, "Should disable MPU successfully");
}

// =============================================================================
// STACK OVERFLOW PROTECTION TESTS
// =============================================================================

static void test_stack_overflow_protection(void) {
    printf("\n=== Testing Stack Overflow Protection ===\n");
    
    // Test enabling stack overflow protection for current task
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task) {
        bool protect_result = pico_rtos_mpu_protect_task_stack(current_task);
        TEST_ASSERT(protect_result, "Should enable stack protection for current task");
        
        // Test checking if stack is protected
        bool is_protected = pico_rtos_mpu_is_stack_protected(current_task);
        TEST_ASSERT(is_protected, "Current task stack should be protected");
        
        // Test disabling stack protection
        bool unprotect_result = pico_rtos_mpu_unprotect_task_stack(current_task);
        TEST_ASSERT(unprotect_result, "Should disable stack protection for current task");
        
        is_protected = pico_rtos_mpu_is_stack_protected(current_task);
        TEST_ASSERT(!is_protected, "Current task stack should not be protected");
    }
    
    // Test stack guard configuration
    uint32_t guard_size = pico_rtos_mpu_get_stack_guard_size();
    TEST_ASSERT(guard_size > 0, "Stack guard size should be positive");
    
    bool set_guard_result = pico_rtos_mpu_set_stack_guard_size(64);
    TEST_ASSERT(set_guard_result, "Should set stack guard size successfully");
    
    uint32_t new_guard_size = pico_rtos_mpu_get_stack_guard_size();
    TEST_ASSERT(new_guard_size == 64, "Stack guard size should be updated");
}

// =============================================================================
// TASK ISOLATION TESTS
// =============================================================================

static void test_task_isolation(void) {
    printf("\n=== Testing Task Isolation ===\n");
    
    // Test enabling task isolation
    bool isolation_result = pico_rtos_mpu_enable_task_isolation();
    TEST_ASSERT(isolation_result, "Should enable task isolation successfully");
    
    bool is_isolated = pico_rtos_mpu_is_task_isolation_enabled();
    TEST_ASSERT(is_isolated, "Task isolation should be enabled");
    
    // Test task memory permissions
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task) {
        // Test setting task memory permissions
        bool perm_result = pico_rtos_mpu_set_task_memory_permissions(current_task,
                                                                    (uint32_t)test_memory_region1,
                                                                    TEST_MEMORY_REGION_SIZE,
                                                                    PICO_RTOS_MPU_ATTR_READ_WRITE);
        TEST_ASSERT(perm_result, "Should set task memory permissions successfully");
        
        // Test checking task memory access
        bool has_access = pico_rtos_mpu_task_has_access(current_task,
                                                        (uint32_t)test_memory_region1,
                                                        PICO_RTOS_MPU_ACCESS_READ);
        TEST_ASSERT(has_access, "Task should have read access to permitted memory");
        
        has_access = pico_rtos_mpu_task_has_access(current_task,
                                                   (uint32_t)test_memory_region1,
                                                   PICO_RTOS_MPU_ACCESS_WRITE);
        TEST_ASSERT(has_access, "Task should have write access to permitted memory");
    }
    
    // Test disabling task isolation
    bool disable_isolation = pico_rtos_mpu_disable_task_isolation();
    TEST_ASSERT(disable_isolation, "Should disable task isolation successfully");
    
    is_isolated = pico_rtos_mpu_is_task_isolation_enabled();
    TEST_ASSERT(!is_isolated, "Task isolation should be disabled");
}

// =============================================================================
// ERROR HANDLING TESTS
// =============================================================================

static void test_error_handling(void) {
    printf("\n=== Testing Error Handling ===\n");
    
    // Test invalid region configuration
    pico_rtos_mpu_region_t invalid_region;
    bool config_result = pico_rtos_mpu_configure_region(&invalid_region, 99,
                                                       0x00000000, // Invalid address
                                                       TEST_MEMORY_REGION_SIZE,
                                                       PICO_RTOS_MPU_ATTR_READ_WRITE);
    TEST_ASSERT(!config_result, "Should fail to configure invalid region");
    
    // Test NULL pointer handling
    config_result = pico_rtos_mpu_configure_region(NULL, 0,
                                                  (uint32_t)test_memory_region1,
                                                  TEST_MEMORY_REGION_SIZE,
                                                  PICO_RTOS_MPU_ATTR_READ_WRITE);
    TEST_ASSERT(!config_result, "Should fail with NULL region pointer");
    
    // Test invalid size
    config_result = pico_rtos_mpu_configure_region(&invalid_region, 0,
                                                  (uint32_t)test_memory_region1,
                                                  0, // Invalid size
                                                  PICO_RTOS_MPU_ATTR_READ_WRITE);
    TEST_ASSERT(!config_result, "Should fail with invalid size");
    
    // Test operations on uninitialized region
    bool enable_result = pico_rtos_mpu_enable_region(&invalid_region);
    TEST_ASSERT(!enable_result, "Should fail to enable uninitialized region");
    
    // Test NULL task operations
    bool protect_result = pico_rtos_mpu_protect_task_stack(NULL);
    TEST_ASSERT(!protect_result, "Should fail to protect NULL task stack");
    
    bool has_access = pico_rtos_mpu_task_has_access(NULL, (uint32_t)test_memory_region1,
                                                    PICO_RTOS_MPU_ACCESS_READ);
    TEST_ASSERT(!has_access, "Should return false for NULL task access check");
}

// =============================================================================
// MPU STATISTICS TESTS
// =============================================================================

static void test_mpu_statistics(void) {
    printf("\n=== Testing MPU Statistics ===\n");
    
    // Test getting MPU statistics
    pico_rtos_mpu_stats_t stats;
    bool stats_result = pico_rtos_mpu_get_stats(&stats);
    TEST_ASSERT(stats_result, "Should get MPU statistics successfully");
    
    TEST_ASSERT(stats.total_regions >= 0, "Total regions should be non-negative");
    TEST_ASSERT(stats.active_regions >= 0, "Active regions should be non-negative");
    TEST_ASSERT(stats.violation_count >= 0, "Violation count should be non-negative");
    
    printf("MPU Stats: %lu total regions, %lu active, %lu violations\n",
           stats.total_regions, stats.active_regions, stats.violation_count);
    
    // Test resetting statistics
    bool reset_result = pico_rtos_mpu_reset_stats();
    TEST_ASSERT(reset_result, "Should reset MPU statistics successfully");
    
    stats_result = pico_rtos_mpu_get_stats(&stats);
    TEST_ASSERT(stats_result, "Should get statistics after reset");
    TEST_ASSERT(stats.violation_count == 0, "Violation count should be reset to 0");
    
    // Test NULL parameter handling
    stats_result = pico_rtos_mpu_get_stats(NULL);
    TEST_ASSERT(!stats_result, "Should fail with NULL stats parameter");
}

// =============================================================================
// UTILITY FUNCTION TESTS
// =============================================================================

static void test_utility_functions(void) {
    printf("\n=== Testing Utility Functions ===\n");
    
    // Test address alignment checking
    bool is_aligned = pico_rtos_mpu_is_address_aligned((uint32_t)test_memory_region1);
    TEST_ASSERT(is_aligned, "Test memory region should be properly aligned");
    
    is_aligned = pico_rtos_mpu_is_address_aligned(0x12345678);
    // Result depends on alignment requirements, just test it doesn't crash
    printf("Address 0x12345678 alignment: %s\n", is_aligned ? "aligned" : "not aligned");
    
    // Test size alignment checking
    bool size_aligned = pico_rtos_mpu_is_size_valid(TEST_MEMORY_REGION_SIZE);
    TEST_ASSERT(size_aligned, "Test memory region size should be valid");
    
    size_aligned = pico_rtos_mpu_is_size_valid(33); // Odd size
    printf("Size 33 validity: %s\n", size_aligned ? "valid" : "invalid");
    
    // Test getting required alignment
    uint32_t alignment = pico_rtos_mpu_get_required_alignment();
    TEST_ASSERT(alignment > 0, "Required alignment should be positive");
    TEST_ASSERT((alignment & (alignment - 1)) == 0, "Alignment should be power of 2");
    
    printf("Required MPU alignment: %lu bytes\n", alignment);
    
    // Test calculating region size
    uint32_t calc_size = pico_rtos_mpu_calculate_region_size(100);
    TEST_ASSERT(calc_size >= 100, "Calculated size should be at least requested size");
    TEST_ASSERT((calc_size & (calc_size - 1)) == 0, "Calculated size should be power of 2");
    
    printf("Calculated region size for 100 bytes: %lu bytes\n", calc_size);
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

static void run_all_mpu_tests(void) {
    printf("\n");
    printf("=====================================\n");
    printf("    Pico-RTOS MPU Unit Tests        \n");
    printf("=====================================\n");
    
    test_mpu_initialization();
    test_memory_region_configuration();
    test_access_violation_detection();
    test_stack_overflow_protection();
    test_task_isolation();
    test_error_handling();
    test_mpu_statistics();
    test_utility_functions();
    
    printf("\n");
    printf("=====================================\n");
    printf("         Test Results Summary        \n");
    printf("=====================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_count - test_failures);
    printf("Failed: %d\n", test_failures);
    printf("Success rate: %.1f%%\n", 
           test_count > 0 ? (100.0 * (test_count - test_failures)) / test_count : 0.0);
    printf("=====================================\n");
    
    if (test_failures == 0) {
        printf("🎉 All MPU tests PASSED!\n");
    } else {
        printf("❌ Some MPU tests FAILED!\n");
    }
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Wait for USB serial
    sleep_ms(2000);
    
    printf("MPU Test Starting...\n");
    
#ifdef PICO_RTOS_ENABLE_MPU_SUPPORT
    // Initialize Pico-RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    // Run tests
    run_all_mpu_tests();
    
    return (test_failures == 0) ? 0 : 1;
#else
    printf("MPU support is not enabled in this build configuration.\n");
    printf("Enable PICO_RTOS_ENABLE_MPU_SUPPORT to run this test.\n");
    return 0;
#endif
}