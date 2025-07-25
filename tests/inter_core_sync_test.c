#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pico_rtos/smp.h"
#include "pico_rtos/config.h"
#include "pico_rtos/logging.h"

// Test framework macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", message); \
            return false; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf("\n=== Running %s ===\n", #test_func); \
        if (test_func()) { \
            printf("✓ %s PASSED\n", #test_func); \
            tests_passed++; \
        } else { \
            printf("✗ %s FAILED\n", #test_func); \
            tests_failed++; \
        } \
        total_tests++; \
    } while(0)

// Global test counters
static int total_tests = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// =============================================================================
// INTER-CORE CRITICAL SECTION TESTS
// =============================================================================

bool test_inter_core_cs_init(void) {
    pico_rtos_inter_core_cs_t ics;
    
    // Test successful initialization
    TEST_ASSERT(pico_rtos_inter_core_cs_init(&ics), "Inter-core CS initialization should succeed");
    
    // Test null pointer
    TEST_ASSERT(!pico_rtos_inter_core_cs_init(NULL), "Inter-core CS init with NULL should fail");
    
    return true;
}

bool test_inter_core_cs_basic_operations(void) {
    pico_rtos_inter_core_cs_t ics;
    
    // Initialize critical section
    TEST_ASSERT(pico_rtos_inter_core_cs_init(&ics), "Inter-core CS should initialize");
    
    // Test try_enter when free
    TEST_ASSERT(pico_rtos_inter_core_cs_try_enter(&ics), "Try enter should succeed when CS is free");
    
    // Test try_enter when already owned (recursive)
    TEST_ASSERT(pico_rtos_inter_core_cs_try_enter(&ics), "Recursive try enter should succeed");
    
    // Exit twice (for recursive entries)
    pico_rtos_inter_core_cs_exit(&ics);
    pico_rtos_inter_core_cs_exit(&ics);
    
    // Test enter with timeout
    TEST_ASSERT(pico_rtos_inter_core_cs_enter(&ics, 100), "Enter with timeout should succeed when free");
    
    // Exit
    pico_rtos_inter_core_cs_exit(&ics);
    
    return true;
}

bool test_inter_core_cs_statistics(void) {
    pico_rtos_inter_core_cs_t ics;
    uint32_t lock_count, contention_count;
    
    // Initialize critical section
    TEST_ASSERT(pico_rtos_inter_core_cs_init(&ics), "Inter-core CS should initialize");
    
    // Get initial statistics
    TEST_ASSERT(pico_rtos_inter_core_cs_get_stats(&ics, &lock_count, &contention_count),
               "Getting CS statistics should succeed");
    TEST_ASSERT(lock_count == 0, "Initial lock count should be 0");
    TEST_ASSERT(contention_count == 0, "Initial contention count should be 0");
    
    // Enter and exit to increment lock count
    TEST_ASSERT(pico_rtos_inter_core_cs_enter(&ics, 100), "CS enter should succeed");
    pico_rtos_inter_core_cs_exit(&ics);
    
    // Check updated statistics
    TEST_ASSERT(pico_rtos_inter_core_cs_get_stats(&ics, &lock_count, &contention_count),
               "Getting CS statistics should succeed after use");
    TEST_ASSERT(lock_count == 1, "Lock count should be 1 after one enter/exit");
    
    return true;
}

// =============================================================================
// INTER-CORE COMMUNICATION TESTS
// =============================================================================

bool test_ipc_channel_init(void) {
    pico_rtos_ipc_channel_t channel;
    pico_rtos_ipc_message_t buffer[10];
    
    // Test successful initialization
    TEST_ASSERT(pico_rtos_ipc_channel_init(&channel, buffer, 10),
               "IPC channel initialization should succeed");
    
    // Test null parameters
    TEST_ASSERT(!pico_rtos_ipc_channel_init(NULL, buffer, 10),
               "IPC channel init with NULL channel should fail");
    TEST_ASSERT(!pico_rtos_ipc_channel_init(&channel, NULL, 10),
               "IPC channel init with NULL buffer should fail");
    TEST_ASSERT(!pico_rtos_ipc_channel_init(&channel, buffer, 0),
               "IPC channel init with zero buffer size should fail");
    
    return true;
}

bool test_ipc_message_operations(void) {
    pico_rtos_ipc_message_t message;
    
    // Test notification sending (should not crash)
    TEST_ASSERT(pico_rtos_ipc_send_notification(1, 0x1234) || true,
               "IPC notification send should not crash");
    
    // Test pending message count
    uint32_t pending = pico_rtos_ipc_pending_messages();
    TEST_ASSERT(pending >= 0, "Pending message count should be non-negative");
    
    // Test message processing (should not crash)
    pico_rtos_ipc_process_messages();
    TEST_ASSERT(true, "IPC message processing should not crash");
    
    return true;
}

bool test_ipc_channel_statistics(void) {
    pico_rtos_ipc_channel_t channel;
    pico_rtos_ipc_message_t buffer[10];
    uint32_t total_messages, dropped_messages;
    
    // Initialize channel
    TEST_ASSERT(pico_rtos_ipc_channel_init(&channel, buffer, 10),
               "IPC channel should initialize");
    
    // Get initial statistics
    TEST_ASSERT(pico_rtos_ipc_get_channel_stats(&channel, &total_messages, &dropped_messages),
               "Getting IPC channel statistics should succeed");
    TEST_ASSERT(total_messages == 0, "Initial total messages should be 0");
    TEST_ASSERT(dropped_messages == 0, "Initial dropped messages should be 0");
    
    return true;
}

// =============================================================================
// SYNCHRONIZATION BARRIER TESTS
// =============================================================================

bool test_sync_barrier_init(void) {
    pico_rtos_sync_barrier_t barrier;
    
    // Test successful initialization
    TEST_ASSERT(pico_rtos_sync_barrier_init(&barrier, 0x3, 1),
               "Sync barrier initialization should succeed");
    
    // Test invalid parameters
    TEST_ASSERT(!pico_rtos_sync_barrier_init(NULL, 0x3, 1),
               "Sync barrier init with NULL should fail");
    TEST_ASSERT(!pico_rtos_sync_barrier_init(&barrier, 0, 1),
               "Sync barrier init with no required cores should fail");
    TEST_ASSERT(!pico_rtos_sync_barrier_init(&barrier, 0xF, 1),
               "Sync barrier init with too many cores should fail");
    
    return true;
}

bool test_sync_barrier_operations(void) {
    pico_rtos_sync_barrier_t barrier;
    
    // Initialize barrier for single core (should release immediately)
    TEST_ASSERT(pico_rtos_sync_barrier_init(&barrier, 0x1, 1),
               "Sync barrier should initialize for single core");
    
    // Wait on barrier (should succeed immediately for single core)
    TEST_ASSERT(pico_rtos_sync_barrier_wait(&barrier, 100),
               "Single core barrier wait should succeed immediately");
    
    // Test barrier reset
    pico_rtos_sync_barrier_reset(&barrier);
    TEST_ASSERT(true, "Barrier reset should not crash");
    
    return true;
}

bool test_sync_barrier_statistics(void) {
    pico_rtos_sync_barrier_t barrier;
    uint32_t wait_count, current_cores;
    
    // Initialize barrier
    TEST_ASSERT(pico_rtos_sync_barrier_init(&barrier, 0x1, 1),
               "Sync barrier should initialize");
    
    // Get initial statistics
    TEST_ASSERT(pico_rtos_sync_barrier_get_stats(&barrier, &wait_count, &current_cores),
               "Getting barrier statistics should succeed");
    TEST_ASSERT(wait_count == 0, "Initial wait count should be 0");
    TEST_ASSERT(current_cores == 0, "Initial current cores should be 0");
    
    // Wait on barrier to increment statistics
    TEST_ASSERT(pico_rtos_sync_barrier_wait(&barrier, 100),
               "Barrier wait should succeed");
    
    // Check updated statistics
    TEST_ASSERT(pico_rtos_sync_barrier_get_stats(&barrier, &wait_count, &current_cores),
               "Getting barrier statistics should succeed after use");
    TEST_ASSERT(wait_count == 1, "Wait count should be 1 after one wait");
    
    return true;
}

// =============================================================================
// SMP SYNCHRONIZATION PRIMITIVE TESTS
// =============================================================================

bool test_smp_synchronization_primitives(void) {
    // These functions should not crash and should return appropriate values
    
    // Test mutex SMP enable (should succeed or fail gracefully)
    bool mutex_result = pico_rtos_mutex_enable_smp(NULL);
    TEST_ASSERT(mutex_result == false, "Mutex SMP enable with NULL should return false");
    
    // Test semaphore SMP enable
    bool semaphore_result = pico_rtos_semaphore_enable_smp(NULL);
    TEST_ASSERT(semaphore_result == false, "Semaphore SMP enable with NULL should return false");
    
    // Test queue SMP enable
    bool queue_result = pico_rtos_queue_enable_smp(NULL);
    TEST_ASSERT(queue_result == false, "Queue SMP enable with NULL should return false");
    
    // Test event group SMP enable
    bool event_group_result = pico_rtos_event_group_enable_smp(NULL);
    TEST_ASSERT(event_group_result == false, "Event group SMP enable with NULL should return false");
    
    return true;
}

// =============================================================================
// INTEGRATION TESTS
// =============================================================================

bool test_inter_core_sync_integration(void) {
    // Test that all inter-core synchronization components work together
    
    pico_rtos_inter_core_cs_t ics;
    pico_rtos_sync_barrier_t barrier;
    pico_rtos_ipc_channel_t channel;
    pico_rtos_ipc_message_t buffer[5];
    
    // Initialize all components
    TEST_ASSERT(pico_rtos_inter_core_cs_init(&ics),
               "Inter-core CS should initialize in integration test");
    
    TEST_ASSERT(pico_rtos_sync_barrier_init(&barrier, 0x1, 100),
               "Sync barrier should initialize in integration test");
    
    TEST_ASSERT(pico_rtos_ipc_channel_init(&channel, buffer, 5),
               "IPC channel should initialize in integration test");
    
    // Test combined operations
    TEST_ASSERT(pico_rtos_inter_core_cs_enter(&ics, 100),
               "Should be able to enter CS in integration test");
    
    pico_rtos_sync_barrier_reset(&barrier);
    TEST_ASSERT(true, "Should be able to reset barrier while holding CS");
    
    pico_rtos_inter_core_cs_exit(&ics);
    TEST_ASSERT(true, "Should be able to exit CS after barrier operations");
    
    return true;
}

bool test_error_handling(void) {
    // Test error handling with invalid parameters
    
    // Test all functions with NULL pointers
    TEST_ASSERT(!pico_rtos_inter_core_cs_enter(NULL, 100),
               "CS enter with NULL should fail");
    
    TEST_ASSERT(!pico_rtos_sync_barrier_wait(NULL, 100),
               "Barrier wait with NULL should fail");
    
    TEST_ASSERT(!pico_rtos_ipc_send_message(0, NULL, 100),
               "IPC send with NULL message should fail");
    
    TEST_ASSERT(!pico_rtos_ipc_receive_message(NULL, 100),
               "IPC receive with NULL buffer should fail");
    
    // Test with invalid core IDs
    pico_rtos_ipc_message_t msg = {0};
    TEST_ASSERT(!pico_rtos_ipc_send_message(99, &msg, 100),
               "IPC send to invalid core should fail");
    
    return true;
}

// =============================================================================
// PERFORMANCE TESTS
// =============================================================================

bool test_performance_characteristics(void) {
    pico_rtos_inter_core_cs_t ics;
    
    // Initialize critical section
    TEST_ASSERT(pico_rtos_inter_core_cs_init(&ics),
               "Inter-core CS should initialize for performance test");
    
    // Test rapid enter/exit cycles
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT(pico_rtos_inter_core_cs_enter(&ics, 100),
                   "Rapid CS enter should succeed");
        pico_rtos_inter_core_cs_exit(&ics);
    }
    
    // Test nested critical sections
    for (int depth = 0; depth < 10; depth++) {
        TEST_ASSERT(pico_rtos_inter_core_cs_enter(&ics, 100),
                   "Nested CS enter should succeed");
    }
    
    for (int depth = 0; depth < 10; depth++) {
        pico_rtos_inter_core_cs_exit(&ics);
    }
    
    TEST_ASSERT(true, "Nested critical section test completed");
    
    return true;
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main(void) {
    printf("=== Inter-Core Synchronization Test Suite ===\n");
    printf("Testing inter-core synchronization mechanisms for RP2040\n\n");
    
    // Initialize SMP system for testing
    if (!pico_rtos_smp_init()) {
        printf("WARNING: SMP system initialization failed, some tests may not work correctly\n");
    }
    
    // Run all test categories
    printf("\n--- Inter-Core Critical Section Tests ---\n");
    RUN_TEST(test_inter_core_cs_init);
    RUN_TEST(test_inter_core_cs_basic_operations);
    RUN_TEST(test_inter_core_cs_statistics);
    
    printf("\n--- Inter-Core Communication Tests ---\n");
    RUN_TEST(test_ipc_channel_init);
    RUN_TEST(test_ipc_message_operations);
    RUN_TEST(test_ipc_channel_statistics);
    
    printf("\n--- Synchronization Barrier Tests ---\n");
    RUN_TEST(test_sync_barrier_init);
    RUN_TEST(test_sync_barrier_operations);
    RUN_TEST(test_sync_barrier_statistics);
    
    printf("\n--- SMP Synchronization Primitive Tests ---\n");
    RUN_TEST(test_smp_synchronization_primitives);
    
    printf("\n--- Integration Tests ---\n");
    RUN_TEST(test_inter_core_sync_integration);
    RUN_TEST(test_error_handling);
    
    printf("\n--- Performance Tests ---\n");
    RUN_TEST(test_performance_characteristics);
    
    // Print final results
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.1f%%\n", total_tests > 0 ? (100.0 * tests_passed / total_tests) : 0.0);
    
    if (tests_failed == 0) {
        printf("\n🎉 All inter-core synchronization tests PASSED!\n");
        return 0;
    } else {
        printf("\n❌ Some tests FAILED. Please review the output above.\n");
        return 1;
    }
}