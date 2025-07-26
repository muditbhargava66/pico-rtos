/**
 * @file timeout_test.c
 * @brief Unit tests for universal timeout support
 */

#include "pico_rtos/timeout.h"
#include "pico_rtos.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t callback_count;
    uint32_t cleanup_count;
    pico_rtos_timeout_t *last_timeout;
    void *last_user_data;
    bool callback_executed;
    bool cleanup_executed;
} test_timeout_data_t;

static test_timeout_data_t g_test_data = {0};

// =============================================================================
// TEST CALLBACK FUNCTIONS
// =============================================================================

static void test_timeout_callback(pico_rtos_timeout_t *timeout, void *user_data)
{
    g_test_data.callback_count++;
    g_test_data.last_timeout = timeout;
    g_test_data.last_user_data = user_data;
    g_test_data.callback_executed = true;
}

static void test_timeout_cleanup(pico_rtos_timeout_t *timeout, void *user_data)
{
    g_test_data.cleanup_count++;
    g_test_data.last_timeout = timeout;
    g_test_data.last_user_data = user_data;
    g_test_data.cleanup_executed = true;
}

static bool test_condition_false(void *data)
{
    (void)data;
    return false; // Always false for timeout testing
}

static bool test_condition_true(void *data)
{
    (void)data;
    return true; // Always true for immediate success
}

static bool test_condition_counter(void *data)
{
    uint32_t *counter = (uint32_t *)data;
    (*counter)++;
    return (*counter >= 3); // True after 3 calls
}

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static void reset_test_data(void)
{
    memset(&g_test_data, 0, sizeof(g_test_data));
}

static void test_timeout_init(void)
{
    printf("Testing timeout system initialization...\n");
    
    // Test initialization
    assert(pico_rtos_timeout_init() == true);
    
    // Test double initialization
    assert(pico_rtos_timeout_init() == true);
    
    printf("✓ Timeout initialization test passed\n");
}

static void test_timeout_creation(void)
{
    printf("Testing timeout creation...\n");
    
    reset_test_data();
    
    pico_rtos_timeout_t timeout;
    
    // Test valid timeout creation
    assert(pico_rtos_timeout_create(&timeout, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   1000, test_timeout_callback, test_timeout_cleanup, 
                                   (void *)0x12345678) == true);
    
    assert(timeout.timeout_ms == 1000);
    assert(timeout.type == PICO_RTOS_TIMEOUT_TYPE_BLOCKING);
    assert(timeout.callback == test_timeout_callback);
    assert(timeout.cleanup == test_timeout_cleanup);
    assert(timeout.user_data == (void *)0x12345678);
    assert(timeout.active == false);
    assert(timeout.cancelled == false);
    assert(timeout.expired == false);
    
    // Test invalid parameters
    assert(pico_rtos_timeout_create(NULL, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   1000, NULL, NULL, NULL) == false);
    assert(pico_rtos_timeout_create(&timeout, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   PICO_RTOS_TIMEOUT_MAX_MS + 1, NULL, NULL, NULL) == false);
    
    printf("✓ Timeout creation test passed\n");
}

static void test_timeout_start_stop(void)
{
    printf("Testing timeout start/stop operations...\n");
    
    reset_test_data();
    
    pico_rtos_timeout_t timeout;
    assert(pico_rtos_timeout_create(&timeout, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   1000, test_timeout_callback, test_timeout_cleanup, 
                                   NULL) == true);
    
    // Test starting timeout
    assert(pico_rtos_timeout_start(&timeout) == true);
    assert(pico_rtos_timeout_is_active(&timeout) == true);
    assert(timeout.start_time_us > 0);
    assert(timeout.expiry_time_us > timeout.start_time_us);
    
    // Test remaining time
    uint32_t remaining_ms = pico_rtos_timeout_get_remaining_ms(&timeout);
    assert(remaining_ms > 0 && remaining_ms <= 1000);
    
    uint64_t remaining_us = pico_rtos_timeout_get_remaining_us(&timeout);
    assert(remaining_us > 0 && remaining_us <= 1000000);
    
    // Test elapsed time
    pico_rtos_timeout_sleep(10); // Sleep 10ms
    uint32_t elapsed_ms = pico_rtos_timeout_get_elapsed_ms(&timeout);
    assert(elapsed_ms >= 10);
    
    // Test cancelling timeout
    assert(pico_rtos_timeout_cancel(&timeout) == true);
    assert(pico_rtos_timeout_is_active(&timeout) == false);
    assert(timeout.cancelled == true);
    assert(g_test_data.cleanup_executed == true);
    
    // Test double start/cancel
    assert(pico_rtos_timeout_start(&timeout) == true);
    assert(pico_rtos_timeout_start(&timeout) == true); // Should succeed (already running)
    assert(pico_rtos_timeout_cancel(&timeout) == true);
    assert(pico_rtos_timeout_cancel(&timeout) == true); // Should succeed (already cancelled)
    
    printf("✓ Timeout start/stop test passed\n");
}

static void test_timeout_expiration(void)
{
    printf("Testing timeout expiration...\n");
    
    reset_test_data();
    
    pico_rtos_timeout_t timeout;
    assert(pico_rtos_timeout_create(&timeout, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   50, test_timeout_callback, test_timeout_cleanup, 
                                   (void *)0xDEADBEEF) == true);
    
    // Start timeout
    assert(pico_rtos_timeout_start(&timeout) == true);
    
    // Wait for timeout to expire
    pico_rtos_timeout_sleep(100); // Sleep longer than timeout
    
    // Process expirations
    pico_rtos_timeout_process_expirations();
    
    // Check that timeout expired and callback was called
    assert(pico_rtos_timeout_is_expired(&timeout) == true);
    assert(pico_rtos_timeout_is_active(&timeout) == false);
    assert(g_test_data.callback_executed == true);
    assert(g_test_data.callback_count == 1);
    assert(g_test_data.last_user_data == (void *)0xDEADBEEF);
    
    // Remaining time should be 0
    assert(pico_rtos_timeout_get_remaining_ms(&timeout) == 0);
    assert(pico_rtos_timeout_get_remaining_us(&timeout) == 0);
    
    printf("✓ Timeout expiration test passed\n");
}

static void test_timeout_reset(void)
{
    printf("Testing timeout reset...\n");
    
    reset_test_data();
    
    pico_rtos_timeout_t timeout;
    assert(pico_rtos_timeout_create(&timeout, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   100, test_timeout_callback, test_timeout_cleanup, 
                                   NULL) == true);
    
    // Start timeout and let it run for a bit
    assert(pico_rtos_timeout_start(&timeout) == true);
    pico_rtos_timeout_sleep(30); // Wait 30ms
    
    uint32_t remaining_before = pico_rtos_timeout_get_remaining_ms(&timeout);
    
    // Reset timeout
    assert(pico_rtos_timeout_reset(&timeout) == true);
    assert(pico_rtos_timeout_is_active(&timeout) == true);
    
    uint32_t remaining_after = pico_rtos_timeout_get_remaining_ms(&timeout);
    
    // After reset, remaining time should be close to full timeout
    assert(remaining_after > remaining_before);
    assert(remaining_after > 80); // Should be close to 100ms
    
    pico_rtos_timeout_cancel(&timeout);
    
    printf("✓ Timeout reset test passed\n");
}

static void test_timeout_types(void)
{
    printf("Testing different timeout types...\n");
    
    reset_test_data();
    
    // Test blocking timeout
    pico_rtos_timeout_t blocking_timeout;
    assert(pico_rtos_timeout_create(&blocking_timeout, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   100, NULL, NULL, NULL) == true);
    
    // Test periodic timeout
    pico_rtos_timeout_t periodic_timeout;
    assert(pico_rtos_timeout_create(&periodic_timeout, PICO_RTOS_TIMEOUT_TYPE_PERIODIC, 
                                   50, NULL, NULL, NULL) == true);
    
    // Test absolute timeout
    pico_rtos_timeout_t absolute_timeout;
    assert(pico_rtos_timeout_create(&absolute_timeout, PICO_RTOS_TIMEOUT_TYPE_ABSOLUTE, 
                                   200, NULL, NULL, NULL) == true);
    absolute_timeout.deadline_us = pico_rtos_timeout_get_time_us() + 200000; // 200ms from now
    
    // Start all timeouts
    assert(pico_rtos_timeout_start(&blocking_timeout) == true);
    assert(pico_rtos_timeout_start(&periodic_timeout) == true);
    assert(pico_rtos_timeout_start(&absolute_timeout) == true);
    
    // Verify they're all active
    assert(pico_rtos_timeout_is_active(&blocking_timeout) == true);
    assert(pico_rtos_timeout_is_active(&periodic_timeout) == true);
    assert(pico_rtos_timeout_is_active(&absolute_timeout) == true);
    
    // Clean up
    pico_rtos_timeout_cancel(&blocking_timeout);
    pico_rtos_timeout_cancel(&periodic_timeout);
    pico_rtos_timeout_cancel(&absolute_timeout);
    
    printf("✓ Timeout types test passed\n");
}

static void test_wait_for_condition(void)
{
    printf("Testing wait for condition...\n");
    
    // Test immediate success
    pico_rtos_timeout_result_t result = pico_rtos_timeout_wait_for_condition(
        test_condition_true, NULL, 1000);
    assert(result == PICO_RTOS_TIMEOUT_SUCCESS);
    
    // Test immediate timeout
    result = pico_rtos_timeout_wait_for_condition(
        test_condition_false, NULL, PICO_RTOS_TIMEOUT_IMMEDIATE);
    assert(result == PICO_RTOS_TIMEOUT_EXPIRED);
    
    // Test timeout expiration
    result = pico_rtos_timeout_wait_for_condition(
        test_condition_false, NULL, 50);
    assert(result == PICO_RTOS_TIMEOUT_EXPIRED);
    
    // Test condition becoming true
    uint32_t counter = 0;
    result = pico_rtos_timeout_wait_for_condition(
        test_condition_counter, &counter, 1000);
    assert(result == PICO_RTOS_TIMEOUT_SUCCESS);
    assert(counter >= 3);
    
    // Test invalid parameters
    result = pico_rtos_timeout_wait_for_condition(NULL, NULL, 1000);
    assert(result == PICO_RTOS_TIMEOUT_INVALID);
    
    printf("✓ Wait for condition test passed\n");
}

static void test_sleep_functions(void)
{
    printf("Testing sleep functions...\n");
    
    uint64_t start_time, end_time, duration;
    
    // Test sleep in milliseconds
    start_time = pico_rtos_timeout_get_time_us();
    pico_rtos_timeout_result_t result = pico_rtos_timeout_sleep(50);
    end_time = pico_rtos_timeout_get_time_us();
    duration = end_time - start_time;
    
    assert(result == PICO_RTOS_TIMEOUT_SUCCESS);
    assert(duration >= 45000 && duration <= 55000); // 45-55ms tolerance
    
    // Test sleep in microseconds
    start_time = pico_rtos_timeout_get_time_us();
    result = pico_rtos_timeout_sleep_us(30000); // 30ms
    end_time = pico_rtos_timeout_get_time_us();
    duration = end_time - start_time;
    
    assert(result == PICO_RTOS_TIMEOUT_SUCCESS);
    assert(duration >= 25000 && duration <= 35000); // 25-35ms tolerance
    
    // Test zero sleep
    result = pico_rtos_timeout_sleep(0);
    assert(result == PICO_RTOS_TIMEOUT_SUCCESS);
    
    result = pico_rtos_timeout_sleep_us(0);
    assert(result == PICO_RTOS_TIMEOUT_SUCCESS);
    
    printf("✓ Sleep functions test passed\n");
}

static void test_wait_until(void)
{
    printf("Testing wait until function...\n");
    
    uint64_t current_time = pico_rtos_timeout_get_time_us();
    uint64_t target_time = current_time + 50000; // 50ms from now
    
    // Test waiting until future time
    uint64_t start_time = pico_rtos_timeout_get_time_us();
    pico_rtos_timeout_result_t result = pico_rtos_timeout_wait_until(target_time);
    uint64_t end_time = pico_rtos_timeout_get_time_us();
    
    assert(result == PICO_RTOS_TIMEOUT_SUCCESS);
    assert(end_time >= target_time);
    assert((end_time - start_time) >= 45000); // At least 45ms
    
    // Test waiting until past time
    uint64_t past_time = pico_rtos_timeout_get_time_us() - 10000; // 10ms ago
    result = pico_rtos_timeout_wait_until(past_time);
    assert(result == PICO_RTOS_TIMEOUT_EXPIRED);
    
    printf("✓ Wait until test passed\n");
}

static void test_configuration(void)
{
    printf("Testing timeout configuration...\n");
    
    // Get current configuration
    pico_rtos_timeout_config_t config;
    pico_rtos_timeout_get_config(&config);
    
    // Verify default values
    assert(config.resolution_us == PICO_RTOS_TIMEOUT_RESOLUTION_US);
    assert(config.max_active_timeouts == PICO_RTOS_TIMEOUT_MAX_ACTIVE);
    
    // Modify configuration
    config.high_precision_mode = false;
    config.cleanup_interval_ms = 2000;
    config.accuracy_threshold_us = 500;
    
    // Set new configuration
    assert(pico_rtos_timeout_set_config(&config) == true);
    
    // Verify configuration was applied
    pico_rtos_timeout_config_t new_config;
    pico_rtos_timeout_get_config(&new_config);
    
    assert(new_config.high_precision_mode == false);
    assert(new_config.cleanup_interval_ms == 2000);
    assert(new_config.accuracy_threshold_us == 500);
    
    printf("✓ Configuration test passed\n");
}

static void test_statistics(void)
{
    printf("Testing timeout statistics...\n");
    
    // Reset statistics
    pico_rtos_timeout_reset_statistics();
    
    // Create and use some timeouts
    pico_rtos_timeout_t timeout1, timeout2;
    
    assert(pico_rtos_timeout_create(&timeout1, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   30, test_timeout_callback, NULL, NULL) == true);
    assert(pico_rtos_timeout_create(&timeout2, PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                   50, test_timeout_callback, NULL, NULL) == true);
    
    // Start timeouts
    assert(pico_rtos_timeout_start(&timeout1) == true);
    assert(pico_rtos_timeout_start(&timeout2) == true);
    
    // Cancel one, let other expire
    pico_rtos_timeout_cancel(&timeout1);
    pico_rtos_timeout_sleep(60);
    pico_rtos_timeout_process_expirations();
    
    // Get statistics
    pico_rtos_timeout_statistics_t stats;
    pico_rtos_timeout_get_statistics(&stats);
    
    // Verify statistics
    assert(stats.total_timeouts_created >= 2);
    assert(stats.total_timeouts_cancelled >= 1);
    assert(stats.total_timeouts_expired >= 1);
    assert(stats.total_wait_time_us > 0);
    
    printf("✓ Statistics test passed\n");
}

static void test_utility_functions(void)
{
    printf("Testing utility functions...\n");
    
    // Test time conversion functions
    assert(pico_rtos_timeout_ms_to_us(1000) == 1000000);
    assert(pico_rtos_timeout_us_to_ms(1000000) == 1000);
    assert(pico_rtos_timeout_us_to_ms(1500000) == 1500);
    
    // Test time retrieval functions
    uint64_t time_us = pico_rtos_timeout_get_time_us();
    uint32_t time_ms = pico_rtos_timeout_get_time_ms();
    
    assert(time_us > 0);
    assert(time_ms > 0);
    assert(time_ms == pico_rtos_timeout_us_to_ms(time_us));
    
    // Test string functions
    const char *result_str = pico_rtos_timeout_get_result_string(PICO_RTOS_TIMEOUT_SUCCESS);
    assert(result_str != NULL);
    assert(strcmp(result_str, "Success") == 0);
    
    const char *type_str = pico_rtos_timeout_get_type_string(PICO_RTOS_TIMEOUT_TYPE_BLOCKING);
    assert(type_str != NULL);
    assert(strcmp(type_str, "Blocking") == 0);
    
    printf("✓ Utility functions test passed\n");
}

static void test_multiple_timeouts(void)
{
    printf("Testing multiple concurrent timeouts...\n");
    
    reset_test_data();
    
    pico_rtos_timeout_t timeouts[5];
    uint32_t durations[] = {20, 40, 60, 80, 100};
    
    // Create multiple timeouts with different durations
    for (int i = 0; i < 5; i++) {
        assert(pico_rtos_timeout_create(&timeouts[i], PICO_RTOS_TIMEOUT_TYPE_BLOCKING, 
                                       durations[i], test_timeout_callback, NULL, 
                                       (void *)(uintptr_t)i) == true);
        assert(pico_rtos_timeout_start(&timeouts[i]) == true);
    }
    
    // Wait for all timeouts to expire
    pico_rtos_timeout_sleep(120);
    
    // Process expirations multiple times to catch all timeouts
    for (int i = 0; i < 10; i++) {
        pico_rtos_timeout_process_expirations();
        pico_rtos_timeout_sleep(5);
    }
    
    // Check that all timeouts expired
    for (int i = 0; i < 5; i++) {
        assert(pico_rtos_timeout_is_expired(&timeouts[i]) == true);
    }
    
    // Should have received 5 callbacks
    assert(g_test_data.callback_count == 5);
    
    printf("✓ Multiple timeouts test passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void)
{
    printf("Starting universal timeout system tests...\n\n");
    
    // Initialize RTOS (required for timeout system)
    assert(pico_rtos_init() == true);
    
    // Run tests
    test_timeout_init();
    test_timeout_creation();
    test_timeout_start_stop();
    test_timeout_expiration();
    test_timeout_reset();
    test_timeout_types();
    test_wait_for_condition();
    test_sleep_functions();
    test_wait_until();
    test_configuration();
    test_statistics();
    test_utility_functions();
    test_multiple_timeouts();
    
    printf("\n✓ All universal timeout system tests passed!\n");
    return 0;
}

// Task function for testing (if needed)
void timeout_test_task(void *param)
{
    (void)param;
    main();
    pico_rtos_task_delete(NULL);
}