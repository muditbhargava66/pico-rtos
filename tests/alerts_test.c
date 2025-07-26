/**
 * @file alerts_test.c
 * @brief Unit tests for alert and notification system
 */

#include "pico_rtos/alerts.h"
#include "pico_rtos.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t callback_count;
    uint32_t filter_count;
    pico_rtos_alert_t last_alert;
    pico_rtos_alert_action_t last_action;
    bool allow_alert;
    bool callbacks_executed;
} test_alert_data_t;

static test_alert_data_t g_test_data = {0};

// =============================================================================
// TEST CALLBACK FUNCTIONS
// =============================================================================

static pico_rtos_alert_action_t test_alert_callback(const pico_rtos_alert_t *alert, void *user_data)
{
    (void)user_data;
    
    if (alert != NULL) {
        g_test_data.callback_count++;
        g_test_data.last_alert = *alert;
        g_test_data.callbacks_executed = true;
        g_test_data.last_action = PICO_RTOS_ALERT_ACTION_LOG;
    }
    
    return g_test_data.last_action;
}

static bool test_alert_filter(const pico_rtos_alert_t *alert, void *user_data)
{
    (void)alert;
    (void)user_data;
    
    g_test_data.filter_count++;
    return g_test_data.allow_alert;
}

static pico_rtos_alert_action_t test_escalation_callback(const pico_rtos_alert_t *alert, void *user_data)
{
    (void)user_data;
    
    if (alert != NULL) {
        g_test_data.callback_count++;
        g_test_data.last_alert = *alert;
        g_test_data.callbacks_executed = true;
        g_test_data.last_action = PICO_RTOS_ALERT_ACTION_ESCALATE;
    }
    
    return g_test_data.last_action;
}

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static void reset_test_data(void)
{
    memset(&g_test_data, 0, sizeof(g_test_data));
    g_test_data.allow_alert = true; // Default to allowing alerts
}

static void test_alerts_init(void)
{
    printf("Testing alert system initialization...\n");
    
    // Test initialization with default config
    assert(pico_rtos_alerts_init(NULL) == true);
    
    // Test double initialization
    assert(pico_rtos_alerts_init(NULL) == true);
    
    // Test with custom config
    pico_rtos_alert_config_t config = {
        .enable_escalation = true,
        .escalation_interval_ms = 5000,
        .max_escalation_levels = 2,
        .enable_auto_acknowledge = true,
        .auto_acknowledge_timeout_ms = 10000,
        .enable_alert_history = true,
        .history_retention_ms = 60000,
        .enable_persistent_alerts = false
    };
    
    assert(pico_rtos_alerts_init(&config) == true);
    
    // Verify initial state
    assert(pico_rtos_alerts_is_enabled() == true);
    
    printf("✓ Alert initialization test passed\n");
}

static void test_enable_disable(void)
{
    printf("Testing enable/disable functionality...\n");
    
    // Test disabling
    pico_rtos_alerts_set_enabled(false);
    assert(pico_rtos_alerts_is_enabled() == false);
    
    // Test enabling
    pico_rtos_alerts_set_enabled(true);
    assert(pico_rtos_alerts_is_enabled() == true);
    
    printf("✓ Enable/disable test passed\n");
}

static void test_alert_creation(void)
{
    printf("Testing alert creation...\n");
    
    // Test valid alert creation
    uint32_t alert1_id = pico_rtos_alerts_create(
        PICO_RTOS_ALERT_SEVERITY_WARNING,
        PICO_RTOS_ALERT_SOURCE_SYSTEM,
        "Test Alert 1",
        "This is a test alert for unit testing",
        "Test Category"
    );
    assert(alert1_id != 0);
    
    uint32_t alert2_id = pico_rtos_alerts_create(
        PICO_RTOS_ALERT_SEVERITY_ERROR,
        PICO_RTOS_ALERT_SOURCE_MEMORY,
        "Test Alert 2",
        "Another test alert",
        NULL  // No category
    );
    assert(alert2_id != 0);
    assert(alert2_id != alert1_id);
    
    // Test invalid parameters
    assert(pico_rtos_alerts_create(PICO_RTOS_ALERT_SEVERITY_INFO, 
                                  PICO_RTOS_ALERT_SOURCE_USER, 
                                  NULL, "Description", NULL) == 0);
    
    // Test getting alert information
    pico_rtos_alert_t *alert;
    assert(pico_rtos_alerts_get(alert1_id, &alert) == true);
    assert(alert != NULL);
    assert(alert->alert_id == alert1_id);
    assert(alert->severity == PICO_RTOS_ALERT_SEVERITY_WARNING);
    assert(alert->source == PICO_RTOS_ALERT_SOURCE_SYSTEM);
    assert(strcmp(alert->name, "Test Alert 1") == 0);
    
    printf("✓ Alert creation test passed\n");
}

static void test_handler_registration(void)
{
    printf("Testing handler registration...\n");
    
    reset_test_data();
    
    // Test handler registration
    uint32_t handler1_id = pico_rtos_alerts_register_handler(
        "Test Handler 1",
        test_alert_callback,
        test_alert_filter,
        NULL,
        PICO_RTOS_ALERT_SEVERITY_INFO,
        0,  // All sources
        10  // Priority
    );
    assert(handler1_id != 0);
    
    uint32_t handler2_id = pico_rtos_alerts_register_handler(
        "Test Handler 2",
        test_escalation_callback,
        NULL,  // No filter
        NULL,
        PICO_RTOS_ALERT_SEVERITY_WARNING,
        (1 << PICO_RTOS_ALERT_SOURCE_SYSTEM),  // Only system alerts
        5   // Lower priority
    );
    assert(handler2_id != 0);
    assert(handler2_id != handler1_id);
    
    // Test handler enable/disable
    assert(pico_rtos_alerts_set_handler_enabled(handler1_id, false) == true);
    assert(pico_rtos_alerts_set_handler_enabled(handler1_id, true) == true);
    
    // Test handler unregistration
    assert(pico_rtos_alerts_unregister_handler(handler2_id) == true);
    
    // Test invalid handler operations
    assert(pico_rtos_alerts_unregister_handler(0) == false);
    assert(pico_rtos_alerts_set_handler_enabled(999, true) == false);
    
    // Test invalid parameters
    assert(pico_rtos_alerts_register_handler("Test", NULL, NULL, NULL, 
                                            PICO_RTOS_ALERT_SEVERITY_INFO, 0, 1) == 0);
    
    printf("✓ Handler registration test passed\n");
}

static void test_alert_triggering(void)
{
    printf("Testing alert triggering...\n");
    
    reset_test_data();
    
    // Register a handler to capture alerts
    uint32_t handler_id = pico_rtos_alerts_register_handler(
        "Trigger Test Handler",
        test_alert_callback,
        NULL,
        NULL,
        PICO_RTOS_ALERT_SEVERITY_INFO,
        0,  // All sources
        1
    );
    assert(handler_id != 0);
    
    // Create an alert
    uint32_t alert_id = pico_rtos_alerts_create(
        PICO_RTOS_ALERT_SEVERITY_WARNING,
        PICO_RTOS_ALERT_SOURCE_CPU,
        "CPU Usage Alert",
        "CPU usage exceeded threshold",
        "Performance"
    );
    assert(alert_id != 0);
    
    // Trigger the alert
    assert(pico_rtos_alerts_trigger(alert_id, 85, 80, "%") == true);
    
    // Verify handler was called
    assert(g_test_data.callbacks_executed == true);
    assert(g_test_data.callback_count > 0);
    assert(g_test_data.last_alert.alert_id == alert_id);
    assert(g_test_data.last_alert.value == 85);
    assert(g_test_data.last_alert.threshold == 80);
    
    // Test invalid alert ID
    assert(pico_rtos_alerts_trigger(0, 100, 90, "units") == false);
    assert(pico_rtos_alerts_trigger(999, 100, 90, "units") == false);
    
    printf("✓ Alert triggering test passed\n");
}

static void test_alert_acknowledgment(void)
{
    printf("Testing alert acknowledgment...\n");
    
    // Create and trigger an alert
    uint32_t alert_id = pico_rtos_alerts_create(
        PICO_RTOS_ALERT_SEVERITY_ERROR,
        PICO_RTOS_ALERT_SOURCE_MEMORY,
        "Memory Alert",
        "Memory usage critical",
        "System"
    );
    assert(alert_id != 0);
    
    assert(pico_rtos_alerts_trigger(alert_id, 95, 90, "%") == true);
    
    // Acknowledge the alert
    assert(pico_rtos_alerts_acknowledge(alert_id) == true);
    
    // Verify alert state
    pico_rtos_alert_t *alert;
    assert(pico_rtos_alerts_get(alert_id, &alert) == true);
    assert(alert->state == PICO_RTOS_ALERT_STATE_ACKNOWLEDGED);
    assert(alert->acknowledged_time > 0);
    
    // Test invalid alert ID
    assert(pico_rtos_alerts_acknowledge(0) == false);
    assert(pico_rtos_alerts_acknowledge(999) == false);
    
    printf("✓ Alert acknowledgment test passed\n");
}

static void test_alert_resolution(void)
{
    printf("Testing alert resolution...\n");
    
    // Create and trigger an alert
    uint32_t alert_id = pico_rtos_alerts_create(
        PICO_RTOS_ALERT_SEVERITY_CRITICAL,
        PICO_RTOS_ALERT_SOURCE_WATCHDOG,
        "Watchdog Alert",
        "Watchdog timeout imminent",
        "Safety"
    );
    assert(alert_id != 0);
    
    assert(pico_rtos_alerts_trigger(alert_id, 1000, 5000, "ms") == true);
    
    // Resolve the alert
    assert(pico_rtos_alerts_resolve(alert_id) == true);
    
    // Verify alert state
    pico_rtos_alert_t *alert;
    assert(pico_rtos_alerts_get(alert_id, &alert) == true);
    assert(alert->state == PICO_RTOS_ALERT_STATE_RESOLVED);
    assert(alert->resolved_time > 0);
    
    // Test invalid alert ID
    assert(pico_rtos_alerts_resolve(0) == false);
    assert(pico_rtos_alerts_resolve(999) == false);
    
    printf("✓ Alert resolution test passed\n");
}

static void test_alert_filtering(void)
{
    printf("Testing alert filtering...\n");
    
    reset_test_data();
    
    // Register a handler with filter
    uint32_t handler_id = pico_rtos_alerts_register_handler(
        "Filter Test Handler",
        test_alert_callback,
        test_alert_filter,
        NULL,
        PICO_RTOS_ALERT_SEVERITY_INFO,
        0,  // All sources
        1
    );
    assert(handler_id != 0);
    
    // Create an alert
    uint32_t alert_id = pico_rtos_alerts_create(
        PICO_RTOS_ALERT_SEVERITY_INFO,
        PICO_RTOS_ALERT_SOURCE_USER,
        "Filtered Alert",
        "This alert should be filtered",
        "Test"
    );
    assert(alert_id != 0);
    
    // Test with filter allowing alert
    g_test_data.allow_alert = true;
    g_test_data.callback_count = 0;
    g_test_data.filter_count = 0;
    
    assert(pico_rtos_alerts_trigger(alert_id, 50, 40, "units") == true);
    assert(g_test_data.filter_count > 0);
    assert(g_test_data.callback_count > 0);
    
    // Test with filter blocking alert
    g_test_data.allow_alert = false;
    g_test_data.callback_count = 0;
    g_test_data.filter_count = 0;
    
    assert(pico_rtos_alerts_trigger(alert_id, 60, 40, "units") == true);
    assert(g_test_data.filter_count > 0);
    assert(g_test_data.callback_count == 0); // Should be filtered out
    
    printf("✓ Alert filtering test passed\n");
}

static void test_alert_statistics(void)
{
    printf("Testing alert statistics...\n");
    
    // Create and trigger several alerts
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Stats Test Alert %d", i + 1);
        
        uint32_t alert_id = pico_rtos_alerts_create(
            (pico_rtos_alert_severity_t)(i % 3), // Vary severity
            PICO_RTOS_ALERT_SOURCE_SYSTEM,
            name,
            "Statistics test alert",
            "Test"
        );
        assert(alert_id != 0);
        
        assert(pico_rtos_alerts_trigger(alert_id, 100 + i, 90, "units") == true);
        
        if (i % 2 == 0) {
            assert(pico_rtos_alerts_acknowledge(alert_id) == true);
        }
        
        if (i == 2) {
            assert(pico_rtos_alerts_resolve(alert_id) == true);
        }
    }
    
    // Get statistics
    pico_rtos_alert_statistics_t stats;
    assert(pico_rtos_alerts_get_statistics(&stats) == true);
    
    // Verify statistics
    assert(stats.total_alerts > 0);
    assert(stats.active_alerts > 0);
    
    // Test statistics reset
    assert(pico_rtos_alerts_reset_statistics() == true);
    
    printf("✓ Alert statistics test passed\n");
}

static void test_configuration(void)
{
    printf("Testing configuration management...\n");
    
    // Get current configuration
    pico_rtos_alert_config_t config;
    assert(pico_rtos_alerts_get_config(&config) == true);
    
    // Modify configuration
    config.enable_escalation = false;
    config.escalation_interval_ms = 15000;
    config.max_escalation_levels = 5;
    
    // Set new configuration
    assert(pico_rtos_alerts_set_config(&config) == true);
    
    // Verify configuration was applied
    pico_rtos_alert_config_t new_config;
    assert(pico_rtos_alerts_get_config(&new_config) == true);
    assert(new_config.enable_escalation == false);
    assert(new_config.escalation_interval_ms == 15000);
    assert(new_config.max_escalation_levels == 5);
    
    printf("✓ Configuration test passed\n");
}

static void test_utility_functions(void)
{
    printf("Testing utility functions...\n");
    
    // Test string functions
    const char *severity_str = pico_rtos_alerts_get_severity_string(PICO_RTOS_ALERT_SEVERITY_WARNING);
    assert(severity_str != NULL);
    assert(strcmp(severity_str, "Warning") == 0);
    
    const char *state_str = pico_rtos_alerts_get_state_string(PICO_RTOS_ALERT_STATE_ACTIVE);
    assert(state_str != NULL);
    assert(strcmp(state_str, "Active") == 0);
    
    const char *source_str = pico_rtos_alerts_get_source_string(PICO_RTOS_ALERT_SOURCE_SYSTEM);
    assert(source_str != NULL);
    assert(strcmp(source_str, "System") == 0);
    
    // Test print functions (should not crash)
    pico_rtos_alerts_print_summary();
    pico_rtos_alerts_print_detailed_report();
    
    printf("✓ Utility functions test passed\n");
}

static void test_periodic_update(void)
{
    printf("Testing periodic update...\n");
    
    // Test periodic update function (should not crash)
    pico_rtos_alerts_periodic_update();
    
    // Test with alerts disabled
    pico_rtos_alerts_set_enabled(false);
    pico_rtos_alerts_periodic_update();
    
    // Re-enable alerts
    pico_rtos_alerts_set_enabled(true);
    
    printf("✓ Periodic update test passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void)
{
    printf("Starting alert and notification system tests...\n\n");
    
    // Initialize RTOS (required for alert system)
    assert(pico_rtos_init() == true);
    
    // Run tests
    test_alerts_init();
    test_enable_disable();
    test_alert_creation();
    test_handler_registration();
    test_alert_triggering();
    test_alert_acknowledgment();
    test_alert_resolution();
    test_alert_filtering();
    test_alert_statistics();
    test_configuration();
    test_utility_functions();
    test_periodic_update();
    
    printf("\n✓ All alert and notification system tests passed!\n");
    return 0;
}

// Task function for testing (if needed)
void alerts_test_task(void *param)
{
    (void)param;
    main();
    pico_rtos_task_delete(NULL);
}