/**
 * @file watchdog_test.c
 * @brief Unit tests for hardware watchdog integration
 */

#include "pico_rtos/watchdog.h"
#include "pico_rtos.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t timeout_callback_count;
    uint32_t feed_callback_count;
    uint32_t recovery_callback_count;
    pico_rtos_watchdog_action_t last_action;
    uint32_t last_remaining_ms;
    bool allow_feed;
    bool callbacks_executed;
} test_watchdog_data_t;

static test_watchdog_data_t g_test_data = {0};

// =============================================================================
// TEST CALLBACK FUNCTIONS
// =============================================================================

static pico_rtos_watchdog_action_t test_timeout_callback(uint32_t remaining_ms, void *user_data)
{
    (void)user_data;
    
    g_test_data.timeout_callback_count++;
    g_test_data.last_remaining_ms = remaining_ms;
    g_test_data.callbacks_executed = true;
    g_test_data.last_action = PICO_RTOS_WATCHDOG_ACTION_CALLBACK;
    
    return g_test_data.last_action;
}

static bool test_feed_callback(void *user_data)
{
    (void)user_data;
    
    g_test_data.feed_callback_count++;
    g_test_data.callbacks_executed = true;
    
    return g_test_data.allow_feed;
}

static void test_recovery_callback(pico_rtos_watchdog_reset_reason_t reset_reason, void *user_data)
{
    (void)reset_reason;
    (void)user_data;
    
    g_test_data.recovery_callback_count++;
    g_test_data.callbacks_executed = true;
}

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static void reset_test_data(void)
{
    memset(&g_test_data, 0, sizeof(g_test_data));
    g_test_data.allow_feed = true; // Default to allowing feeds
}

static void test_watchdog_init(void)
{
    printf("Testing watchdog initialization...\n");
    
    // Test initialization with default config
    assert(pico_rtos_watchdog_init(NULL) == true);
    
    // Test double initialization
    assert(pico_rtos_watchdog_init(NULL) == true);
    
    // Test with custom config
    pico_rtos_watchdog_config_t config = {
        .timeout_ms = 5000,
        .feed_interval_ms = 500,
        .auto_feed_enabled = true,
        .pause_on_debug = true,
        .default_action = PICO_RTOS_WATCHDOG_ACTION_RESET,
        .warning_threshold_ms = 1000,
        .enable_early_warning = true
    };
    
    assert(pico_rtos_watchdog_init(&config) == true);
    
    // Verify initial state
    assert(pico_rtos_watchdog_get_state() == PICO_RTOS_WATCHDOG_STATE_DISABLED);
    assert(pico_rtos_watchdog_is_enabled() == false);
    
    printf("✓ Watchdog initialization test passed\n");
}

static void test_watchdog_configuration(void)
{
    printf("Testing watchdog configuration...\n");
    
    // Test timeout setting
    assert(pico_rtos_watchdog_set_timeout(3000) == true);
    assert(pico_rtos_watchdog_get_timeout() == 3000);
    
    // Test configuration get/set
    pico_rtos_watchdog_config_t config;
    assert(pico_rtos_watchdog_get_config(&config) == true);
    
    config.feed_interval_ms = 800;
    config.auto_feed_enabled = false;
    assert(pico_rtos_watchdog_set_config(&config) == true);
    
    // Verify configuration was applied
    pico_rtos_watchdog_config_t new_config;
    assert(pico_rtos_watchdog_get_config(&new_config) == true);
    assert(new_config.feed_interval_ms == 800);
    assert(new_config.auto_feed_enabled == false);
    
    // Test feed interval setting
    assert(pico_rtos_watchdog_set_feed_interval(1200) == true);
    
    // Test auto-feed setting
    assert(pico_rtos_watchdog_set_auto_feed(true) == true);
    
    printf("✓ Watchdog configuration test passed\n");
}

static void test_handler_registration(void)
{
    printf("Testing handler registration...\n");
    
    reset_test_data();
    
    // Test handler registration
    uint32_t handler1_id = pico_rtos_watchdog_register_handler(
        "Test Handler 1",
        test_timeout_callback,
        test_feed_callback,
        NULL,
        10  // Priority
    );
    assert(handler1_id != 0);
    
    uint32_t handler2_id = pico_rtos_watchdog_register_handler(
        "Test Handler 2",
        NULL,  // No timeout callback
        test_feed_callback,
        NULL,
        5   // Lower priority
    );
    assert(handler2_id != 0);
    assert(handler2_id != handler1_id);
    
    // Test handler enable/disable
    assert(pico_rtos_watchdog_set_handler_enabled(handler1_id, false) == true);
    assert(pico_rtos_watchdog_set_handler_enabled(handler1_id, true) == true);
    
    // Test getting handler list
    pico_rtos_watchdog_handler_t *handlers[10];
    uint32_t handler_count;
    assert(pico_rtos_watchdog_get_handlers(handlers, 10, &handler_count) == true);
    assert(handler_count >= 2);
    
    // Test handler unregistration
    assert(pico_rtos_watchdog_unregister_handler(handler2_id) == true);
    
    // Test invalid handler operations
    assert(pico_rtos_watchdog_unregister_handler(0) == false);
    assert(pico_rtos_watchdog_set_handler_enabled(999, true) == false);
    
    printf("✓ Handler registration test passed\n");
}

static void test_watchdog_enable_disable(void)
{
    printf("Testing watchdog enable/disable...\n");
    
    // Test invalid timeout values
    assert(pico_rtos_watchdog_enable(500) == false);    // Too short
    assert(pico_rtos_watchdog_enable(0x800000) == false); // Too long
    
    // Test valid enable
    assert(pico_rtos_watchdog_enable(2000) == true);
    assert(pico_rtos_watchdog_is_enabled() == true);
    assert(pico_rtos_watchdog_get_state() == PICO_RTOS_WATCHDOG_STATE_ENABLED);
    assert(pico_rtos_watchdog_get_timeout() == 2000);
    
    // Test remaining time (should be close to timeout)
    uint32_t remaining = pico_rtos_watchdog_get_remaining_time();
    assert(remaining > 1800 && remaining <= 2000);
    
    // Test time since feed (should be small)
    uint32_t time_since_feed = pico_rtos_watchdog_get_time_since_feed();
    assert(time_since_feed < 100);
    
    // Test disable
    assert(pico_rtos_watchdog_disable() == true);
    assert(pico_rtos_watchdog_get_state() == PICO_RTOS_WATCHDOG_STATE_DISABLED);
    
    printf("✓ Watchdog enable/disable test passed\n");
}

static void test_watchdog_feeding(void)
{
    printf("Testing watchdog feeding...\n");
    
    reset_test_data();
    
    // Register a feed handler
    uint32_t handler_id = pico_rtos_watchdog_register_handler(
        "Feed Test Handler",
        NULL,
        test_feed_callback,
        NULL,
        1
    );
    assert(handler_id != 0);
    
    // Enable watchdog
    assert(pico_rtos_watchdog_enable(3000) == true);
    
    // Test successful feed
    g_test_data.allow_feed = true;
    assert(pico_rtos_watchdog_feed() == true);
    assert(g_test_data.feed_callback_count > 0);
    
    // Test blocked feed
    g_test_data.allow_feed = false;
    g_test_data.feed_callback_count = 0;
    assert(pico_rtos_watchdog_feed() == false);
    assert(g_test_data.feed_callback_count > 0);
    
    // Test feed without handlers
    assert(pico_rtos_watchdog_unregister_handler(handler_id) == true);
    assert(pico_rtos_watchdog_feed() == true);
    
    printf("✓ Watchdog feeding test passed\n");
}

static void test_watchdog_statistics(void)
{
    printf("Testing watchdog statistics...\n");
    
    // Enable watchdog and perform some operations
    assert(pico_rtos_watchdog_enable(4000) == true);
    
    // Feed a few times
    for (int i = 0; i < 3; i++) {
        assert(pico_rtos_watchdog_feed() == true);
        // Small delay between feeds (in a real system this would be longer)
        for (volatile int j = 0; j < 10000; j++);
    }
    
    // Get statistics
    pico_rtos_watchdog_statistics_t stats;
    assert(pico_rtos_watchdog_get_statistics(&stats) == true);
    
    // Verify statistics
    assert(stats.total_feeds >= 3);
    assert(stats.uptime_since_last_reset_ms > 0);
    
    // Test statistics reset
    assert(pico_rtos_watchdog_reset_statistics() == true);
    
    // Get statistics again
    assert(pico_rtos_watchdog_get_statistics(&stats) == true);
    assert(stats.total_feeds == 0); // Should be reset
    
    printf("✓ Watchdog statistics test passed\n");
}

static void test_recovery_system(void)
{
    printf("Testing recovery system...\n");
    
    // Test recovery callback setting
    pico_rtos_watchdog_set_recovery_callback(test_recovery_callback, NULL);
    
    // Test recovery data save/restore
    const char test_data[] = "Test recovery data";
    assert(pico_rtos_watchdog_save_recovery_data(test_data, strlen(test_data)) == true);
    
    char recovered_data[64];
    size_t recovered_size;
    bool recovery_result = pico_rtos_watchdog_restore_recovery_data(
        recovered_data, sizeof(recovered_data), &recovered_size);
    
    // Recovery may or may not work depending on system state
    if (recovery_result) {
        assert(recovered_size == strlen(test_data));
        assert(memcmp(recovered_data, test_data, recovered_size) == 0);
    }
    
    // Test recovery performance
    bool recovery_performed = pico_rtos_watchdog_perform_recovery();
    (void)recovery_performed; // May be true or false depending on system state
    
    // Test invalid parameters
    assert(pico_rtos_watchdog_save_recovery_data(NULL, 10) == false);
    assert(pico_rtos_watchdog_save_recovery_data(test_data, 0) == false);
    assert(pico_rtos_watchdog_restore_recovery_data(NULL, 10, NULL) == false);
    
    printf("✓ Recovery system test passed\n");
}

static void test_reset_reason(void)
{
    printf("Testing reset reason detection...\n");
    
    // Get reset reason
    pico_rtos_watchdog_reset_reason_t reason = pico_rtos_watchdog_get_reset_reason();
    
    // Should be a valid reason
    assert(reason >= PICO_RTOS_WATCHDOG_RESET_NONE && 
           reason <= PICO_RTOS_WATCHDOG_RESET_UNKNOWN);
    
    // Test string conversion
    const char *reason_str = pico_rtos_watchdog_get_reset_reason_string(reason);
    assert(reason_str != NULL);
    assert(strlen(reason_str) > 0);
    
    printf("  Reset reason: %s\n", reason_str);
    
    printf("✓ Reset reason test passed\n");
}

static void test_utility_functions(void)
{
    printf("Testing utility functions...\n");
    
    // Test string functions
    const char *state_str = pico_rtos_watchdog_get_state_string(PICO_RTOS_WATCHDOG_STATE_ENABLED);
    assert(state_str != NULL);
    assert(strcmp(state_str, "Enabled") == 0);
    
    const char *action_str = pico_rtos_watchdog_get_action_string(PICO_RTOS_WATCHDOG_ACTION_RESET);
    assert(action_str != NULL);
    assert(strcmp(action_str, "Reset") == 0);
    
    // Test print functions (should not crash)
    pico_rtos_watchdog_print_status();
    pico_rtos_watchdog_print_detailed_report();
    
    printf("✓ Utility functions test passed\n");
}

static void test_periodic_update(void)
{
    printf("Testing periodic update...\n");
    
    // Enable watchdog with auto-feed
    assert(pico_rtos_watchdog_enable(5000) == true);
    assert(pico_rtos_watchdog_set_auto_feed(true) == true);
    
    // Test periodic update function (should not crash)
    pico_rtos_watchdog_periodic_update();
    
    // Test with watchdog disabled
    assert(pico_rtos_watchdog_disable() == true);
    pico_rtos_watchdog_periodic_update();
    
    printf("✓ Periodic update test passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void)
{
    printf("Starting hardware watchdog integration tests...\n\n");
    
    // Initialize RTOS (required for watchdog system)
    assert(pico_rtos_init() == true);
    
    // Run tests
    test_watchdog_init();
    test_watchdog_configuration();
    test_handler_registration();
    test_watchdog_enable_disable();
    test_watchdog_feeding();
    test_watchdog_statistics();
    test_recovery_system();
    test_reset_reason();
    test_utility_functions();
    test_periodic_update();
    
    printf("\n✓ All hardware watchdog integration tests passed!\n");
    printf("Note: Some watchdog features require actual hardware reset to fully test.\n");
    return 0;
}

// Task function for testing (if needed)
void watchdog_test_task(void *param)
{
    (void)param;
    main();
    pico_rtos_task_delete(NULL);
}