/**
 * @file deprecation_test.c
 * @brief Test suite for the deprecation warning system
 * 
 * Tests deprecation warnings, migration helpers, and backward compatibility
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"
#include "pico_rtos/deprecation.h"

// Test configuration
#define TEST_TIMEOUT 1000

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

/**
 * @brief Test deprecation information lookup
 */
void test_deprecation_info_lookup(void) {
    TEST_SECTION("Deprecation Information Lookup");
    
    // Test valid deprecation lookup
    const pico_rtos_deprecation_info_t *info = pico_rtos_get_deprecation_info("pico_rtos_timer_create_precise");
    TEST_ASSERT(info != NULL, "Should find deprecation info for known deprecated function");
    
    if (info) {
        TEST_ASSERT(strcmp(info->feature, "pico_rtos_timer_create_precise") == 0, 
                   "Deprecation info should match requested feature");
        TEST_ASSERT(info->replacement != NULL, "Deprecation info should have replacement");
        TEST_ASSERT(info->migration_notes != NULL, "Deprecation info should have migration notes");
    }
    
    // Test invalid deprecation lookup
    info = pico_rtos_get_deprecation_info("non_existent_function");
    TEST_ASSERT(info == NULL, "Should return NULL for non-existent deprecated function");
    
    // Test NULL parameter
    info = pico_rtos_get_deprecation_info(NULL);
    TEST_ASSERT(info == NULL, "Should return NULL for NULL parameter");
}

/**
 * @brief Test deprecation status checking
 */
void test_deprecation_status_check(void) {
    TEST_SECTION("Deprecation Status Check");
    
    // Test known deprecated feature
    bool is_deprecated = pico_rtos_is_feature_deprecated("pico_rtos_timer_create_precise");
    TEST_ASSERT(is_deprecated == true, "Known deprecated function should return true");
    
    // Test non-deprecated feature
    is_deprecated = pico_rtos_is_feature_deprecated("pico_rtos_task_create");
    TEST_ASSERT(is_deprecated == false, "Non-deprecated function should return false");
    
    // Test NULL parameter
    is_deprecated = pico_rtos_is_feature_deprecated(NULL);
    TEST_ASSERT(is_deprecated == false, "NULL parameter should return false");
}

/**
 * @brief Test deprecated API functions (if migration warnings are enabled)
 */
void test_deprecated_api_functions(void) {
    TEST_SECTION("Deprecated API Functions");
    
    #ifdef PICO_RTOS_ENABLE_MIGRATION_WARNINGS
    
    // Test deprecated timer function
    // Note: This should generate a compile-time warning
    pico_rtos_timer_t timer;
    bool result = pico_rtos_timer_create_precise(&timer, "TestTimer", NULL, NULL, 1000, true);
    TEST_ASSERT(result == false, "Deprecated timer function should return false to force migration");
    
    // Test deprecated logging function
    // Note: This should generate a compile-time warning
    pico_rtos_simple_log("Test message");
    TEST_ASSERT(true, "Deprecated logging function should not crash");
    
    // Test deprecated memory function
    // Note: This should generate a compile-time warning
    uint32_t memory_usage = pico_rtos_get_memory_usage();
    TEST_ASSERT(memory_usage >= 0, "Deprecated memory function should return valid value");
    
    #else
    
    printf("Migration warnings disabled - skipping deprecated API tests\n");
    
    #endif
}

/**
 * @brief Test deprecated macro definitions
 */
void test_deprecated_macros(void) {
    TEST_SECTION("Deprecated Macro Definitions");
    
    #ifdef PICO_RTOS_ENABLE_MIGRATION_WARNINGS
    
    // Test deprecated timeout constants
    // Note: These should generate compile-time warnings
    uint32_t timeout1 = PICO_RTOS_WAIT_INFINITE;
    uint32_t timeout2 = PICO_RTOS_NO_TIMEOUT;
    
    TEST_ASSERT(timeout1 == PICO_RTOS_WAIT_FOREVER, "WAIT_INFINITE should equal WAIT_FOREVER");
    TEST_ASSERT(timeout2 == PICO_RTOS_NO_WAIT, "NO_TIMEOUT should equal NO_WAIT");
    
    // Test deprecated priority constants
    // Note: These should generate compile-time warnings
    uint32_t priority1 = PICO_RTOS_TASK_PRIORITY_IDLE;
    uint32_t priority2 = PICO_RTOS_TASK_PRIORITY_NORMAL;
    uint32_t priority3 = PICO_RTOS_TASK_PRIORITY_HIGH;
    
    TEST_ASSERT(priority1 == 0, "TASK_PRIORITY_IDLE should be 0");
    TEST_ASSERT(priority2 == 5, "TASK_PRIORITY_NORMAL should be 5");
    TEST_ASSERT(priority3 == 10, "TASK_PRIORITY_HIGH should be 10");
    
    #else
    
    printf("Migration warnings disabled - skipping deprecated macro tests\n");
    
    #endif
}

/**
 * @brief Test deprecation warning output
 */
void test_deprecation_warning_output(void) {
    TEST_SECTION("Deprecation Warning Output");
    
    printf("Testing deprecation warning output (should see warnings below):\n");
    
    // This should print deprecation warnings if any deprecated features are configured
    pico_rtos_print_deprecation_warnings();
    
    // This should print configuration warnings
    // pico_rtos_check_configuration_warnings();
    
    TEST_ASSERT(true, "Deprecation warning output should not crash");
}

/**
 * @brief Test backward compatibility
 */
void test_backward_compatibility(void) {
    TEST_SECTION("Backward Compatibility");
    
    // Test that v0.2.1 API patterns still work
    pico_rtos_task_t task;
    pico_rtos_mutex_t mutex;
    pico_rtos_semaphore_t semaphore;
    pico_rtos_queue_t queue;
    uint32_t queue_buffer[5];
    
    // These should work without warnings (not deprecated)
    bool result;
    
    result = pico_rtos_mutex_init(&mutex);
    TEST_ASSERT(result == true, "v0.2.1 mutex init should work");
    
    result = pico_rtos_semaphore_init(&semaphore, 1, 1);
    TEST_ASSERT(result == true, "v0.2.1 semaphore init should work");
    
    result = pico_rtos_queue_init(&queue, queue_buffer, sizeof(uint32_t), 5);
    TEST_ASSERT(result == true, "v0.2.1 queue init should work");
    
    // Test timeout constants (non-deprecated ones)
    uint32_t timeout1 = PICO_RTOS_WAIT_FOREVER;
    uint32_t timeout2 = PICO_RTOS_NO_WAIT;
    
    TEST_ASSERT(timeout1 == UINT32_MAX, "WAIT_FOREVER should be UINT32_MAX");
    TEST_ASSERT(timeout2 == 0, "NO_WAIT should be 0");
    
    // Test that system functions work
    uint32_t tick_count = pico_rtos_get_tick_count();
    uint32_t uptime = pico_rtos_get_uptime_ms();
    const char *version = pico_rtos_get_version_string();
    
    TEST_ASSERT(tick_count >= 0, "Tick count should be valid");
    TEST_ASSERT(uptime >= 0, "Uptime should be valid");
    TEST_ASSERT(version != NULL, "Version string should not be NULL");
    TEST_ASSERT(strncmp(version, "0.3.1", 5) == 0, "Version should be 0.3.1");
}

/**
 * @brief Test migration helper functions
 */
void test_migration_helpers(void) {
    TEST_SECTION("Migration Helper Functions");
    
    // Test deprecation initialization
    pico_rtos_deprecation_init();
    TEST_ASSERT(true, "Deprecation init should not crash");
    
    // Test that all deprecation schedule entries are valid
    for (size_t i = 0; i < PICO_RTOS_DEPRECATION_COUNT; i++) {
        const pico_rtos_deprecation_info_t *info = &pico_rtos_deprecation_schedule[i];
        
        TEST_ASSERT(info->feature != NULL, "Deprecation entry should have feature name");
        TEST_ASSERT(info->deprecated_version != NULL, "Deprecation entry should have deprecated version");
        TEST_ASSERT(info->removal_version != NULL, "Deprecation entry should have removal version");
        TEST_ASSERT(info->replacement != NULL, "Deprecation entry should have replacement");
    }
    
    printf("Found %zu entries in deprecation schedule\n", PICO_RTOS_DEPRECATION_COUNT);
}

/**
 * @brief Test configuration validation warnings
 */
void test_configuration_warnings(void) {
    TEST_SECTION("Configuration Validation Warnings");
    
    // Test configuration warning system
    printf("Testing configuration warnings (may see warnings below):\n");
    pico_rtos_check_configuration_warnings();
    
    TEST_ASSERT(true, "Configuration warning check should not crash");
    
    // Test individual warning conditions
    #if defined(PICO_RTOS_TICK_RATE_HZ) && (PICO_RTOS_TICK_RATE_HZ > 2000)
        printf("High tick rate warning should be shown\n");
    #endif
    
    #if defined(PICO_RTOS_MAX_TASKS) && (PICO_RTOS_MAX_TASKS > 32)
        printf("High task count warning should be shown\n");
    #endif
    
    #if defined(PICO_RTOS_ENABLE_MULTI_CORE) && !defined(PICO_RTOS_ENABLE_MEMORY_TRACKING)
        printf("Multi-core memory tracking recommendation should be shown\n");
    #endif
}

/**
 * @brief Main deprecation test function
 */
void deprecation_test_task(void *param) {
    printf("\n=== Pico-RTOS v0.3.1 Deprecation System Test Suite ===\n");
    printf("Testing deprecation warnings and backward compatibility...\n");
    
    // Run all deprecation tests
    test_deprecation_info_lookup();
    test_deprecation_status_check();
    test_deprecated_api_functions();
    test_deprecated_macros();
    test_deprecation_warning_output();
    test_backward_compatibility();
    test_migration_helpers();
    test_configuration_warnings();
    
    // Print final results
    printf("\n=== DEPRECATION TEST RESULTS ===\n");
    printf("Tests run: %lu\n", tests_run);
    printf("Tests passed: %lu\n", tests_passed);
    printf("Tests failed: %lu\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("SUCCESS: All deprecation tests passed!\n");
        printf("Deprecation system is working correctly\n");
    } else {
        printf("FAILURE: %lu deprecation tests failed!\n", tests_failed);
        printf("Deprecation system has issues\n");
    }
    
    printf("=== Deprecation test complete ===\n");
    
    // Keep task alive
    while (1) {
        pico_rtos_task_delay(10000);
    }
}

/**
 * @brief Initialize and run deprecation tests
 */
int main() {
    stdio_init_all();
    
    printf("Initializing Pico-RTOS for deprecation testing...\n");
    
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize deprecation system (should show warnings if any)
    pico_rtos_deprecation_init();
    
    // Create deprecation test task
    static pico_rtos_task_t deprecation_test_task_handle;
    if (!pico_rtos_task_create(&deprecation_test_task_handle, "DeprecationTest", 
                              deprecation_test_task, NULL, 2048, 5)) {
        printf("ERROR: Failed to create deprecation test task\n");
        return -1;
    }
    
    printf("Starting deprecation tests...\n");
    pico_rtos_start();
    
    return 0;
}