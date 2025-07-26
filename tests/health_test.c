/**
 * @file health_test.c
 * @brief Unit tests for system health monitoring
 */

#include "pico_rtos/health.h"
#include "pico_rtos.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t alert_count;
    pico_rtos_health_alert_t last_alert;
    bool callback_executed;
} test_health_data_t;

static test_health_data_t g_test_data = {0};

// =============================================================================
// TEST CALLBACK FUNCTIONS
// =============================================================================

static void test_alert_callback(const pico_rtos_health_alert_t *alert, void *user_data)
{
    (void)user_data;
    
    if (alert != NULL) {
        g_test_data.alert_count++;
        g_test_data.last_alert = *alert;
        g_test_data.callback_executed = true;
    }
}

static uint32_t test_custom_metric_callback(pico_rtos_health_metric_t *metric, void *user_data)
{
    (void)metric;
    uint32_t *counter = (uint32_t *)user_data;
    
    if (counter != NULL) {
        (*counter)++;
        return *counter * 10; // Return some test value
    }
    
    return 42; // Default test value
}

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static void reset_test_data(void)
{
    memset(&g_test_data, 0, sizeof(g_test_data));
}

static void test_health_init(void)
{
    printf("Testing health monitoring initialization...\n");
    
    // Test initialization
    assert(pico_rtos_health_init() == true);
    
    // Test double initialization
    assert(pico_rtos_health_init() == true);
    
    // Test initial state
    assert(pico_rtos_health_is_enabled() == true);
    
    printf("✓ Health initialization test passed\n");
}

static void test_enable_disable(void)
{
    printf("Testing enable/disable functionality...\n");
    
    // Test disabling
    pico_rtos_health_set_enabled(false);
    assert(pico_rtos_health_is_enabled() == false);
    
    // Test enabling
    pico_rtos_health_set_enabled(true);
    assert(pico_rtos_health_is_enabled() == true);
    
    printf("✓ Enable/disable test passed\n");
}

static void test_configuration(void)
{
    printf("Testing configuration...\n");
    
    reset_test_data();
    
    // Test sample interval setting
    pico_rtos_health_set_sample_interval(200);
    
    // Test alert callback setting
    pico_rtos_health_set_alert_callback(test_alert_callback, NULL);
    
    printf("✓ Configuration test passed\n");
}

static void test_metric_registration(void)
{
    printf("Testing metric registration...\n");
    
    // Test basic metric registration
    uint32_t cpu_metric_id = pico_rtos_health_register_metric(
        PICO_RTOS_HEALTH_METRIC_CPU_USAGE,
        "CPU Usage",
        "CPU usage percentage",
        "%",
        75,  // Warning threshold
        90   // Critical threshold
    );
    assert(cpu_metric_id != 0);
    
    uint32_t memory_metric_id = pico_rtos_health_register_metric(
        PICO_RTOS_HEALTH_METRIC_MEMORY_USAGE,
        "Memory Usage",
        "Memory usage percentage",
        "%",
        80,  // Warning threshold
        95   // Critical threshold
    );
    assert(memory_metric_id != 0);
    assert(memory_metric_id != cpu_metric_id);
    
    // Test custom metric registration
    static uint32_t custom_counter = 0;
    uint32_t custom_metric_id = pico_rtos_health_register_custom_metric(
        "Custom Counter",
        "Test custom metric",
        "count",
        test_custom_metric_callback,
        &custom_counter,
        50,  // Warning threshold
        100  // Critical threshold
    );
    assert(custom_metric_id != 0);
    
    // Test invalid parameters
    assert(pico_rtos_health_register_metric(PICO_RTOS_HEALTH_METRIC_CPU_USAGE, 
                                           NULL, NULL, NULL, 0, 0) == 0);
    
    printf("✓ Metric registration test passed\n");
}

static void test_metric_updates(void)
{
    printf("Testing metric updates...\n");
    
    reset_test_data();
    
    // Register a test metric
    uint32_t test_metric_id = pico_rtos_health_register_metric(
        PICO_RTOS_HEALTH_METRIC_CUSTOM,
        "Test Metric",
        "Test metric for updates",
        "units",
        50,  // Warning threshold
        80   // Critical threshold
    );
    assert(test_metric_id != 0);
    
    // Test normal value update
    assert(pico_rtos_health_update_metric(test_metric_id, 25) == true);
    
    // Test warning threshold trigger
    assert(pico_rtos_health_update_metric(test_metric_id, 60) == true);
    
    // Test critical threshold trigger
    assert(pico_rtos_health_update_metric(test_metric_id, 85) == true);
    
    // Test invalid metric ID
    assert(pico_rtos_health_update_metric(0, 50) == false);
    assert(pico_rtos_health_update_metric(999, 50) == false);
    
    printf("✓ Metric updates test passed\n");
}

static void test_system_statistics(void)
{
    printf("Testing system statistics...\n");
    
    // Test CPU statistics
    pico_rtos_cpu_stats_t cpu_stats;
    assert(pico_rtos_health_get_cpu_stats(0, &cpu_stats) == true);
    assert(cpu_stats.core_id == 0);
    
    // Test invalid core ID
    assert(pico_rtos_health_get_cpu_stats(5, &cpu_stats) == false);
    
    // Test memory statistics
    pico_rtos_memory_stats_t memory_stats;
    assert(pico_rtos_health_get_memory_stats(&memory_stats) == true);
    assert(memory_stats.total_heap_size > 0);
    
    // Test system health
    pico_rtos_system_health_t system_health;
    assert(pico_rtos_health_get_system_health(&system_health) == true);
    assert(system_health.uptime_ms > 0);
    
    // Test with NULL parameters
    assert(pico_rtos_health_get_cpu_stats(0, NULL) == false);
    assert(pico_rtos_health_get_memory_stats(NULL) == false);
    assert(pico_rtos_health_get_system_health(NULL) == false);
    
    printf("✓ System statistics test passed\n");
}

static void test_alert_management(void)
{
    printf("Testing alert management...\n");
    
    reset_test_data();
    
    // Set up alert callback
    pico_rtos_health_set_alert_callback(test_alert_callback, NULL);
    
    // Register a metric that will trigger alerts
    uint32_t alert_metric_id = pico_rtos_health_register_metric(
        PICO_RTOS_HEALTH_METRIC_CUSTOM,
        "Alert Test Metric",
        "Metric for testing alerts",
        "units",
        30,  // Warning threshold
        50   // Critical threshold
    );
    assert(alert_metric_id != 0);
    
    // Trigger warning alert
    assert(pico_rtos_health_update_metric(alert_metric_id, 35) == true);
    
    // Trigger critical alert
    assert(pico_rtos_health_update_metric(alert_metric_id, 55) == true);
    
    // Check that alerts were generated
    assert(g_test_data.callback_executed == true);
    assert(g_test_data.alert_count > 0);
    
    // Test getting active alerts
    pico_rtos_health_alert_t alerts[10];
    uint32_t alert_count;
    assert(pico_rtos_health_get_active_alerts(alerts, 10, &alert_count) == true);
    
    // Test alert acknowledgment
    if (alert_count > 0) {
        assert(pico_rtos_health_acknowledge_alert(alerts[0].alert_id) == true);
    }
    
    // Test clearing acknowledged alerts
    uint32_t cleared_count = pico_rtos_health_clear_acknowledged_alerts();
    (void)cleared_count; // May be 0 if no alerts were acknowledged
    
    printf("✓ Alert management test passed\n");
}

static void test_utility_functions(void)
{
    printf("Testing utility functions...\n");
    
    // Test string functions
    const char *state_str = pico_rtos_health_get_state_string(PICO_RTOS_HEALTH_STATE_HEALTHY);
    assert(state_str != NULL);
    assert(strcmp(state_str, "Healthy") == 0);
    
    const char *metric_str = pico_rtos_health_get_metric_type_string(PICO_RTOS_HEALTH_METRIC_CPU_USAGE);
    assert(metric_str != NULL);
    assert(strcmp(metric_str, "CPU Usage") == 0);
    
    const char *alert_str = pico_rtos_health_get_alert_level_string(PICO_RTOS_HEALTH_ALERT_WARNING);
    assert(alert_str != NULL);
    assert(strcmp(alert_str, "Warning") == 0);
    
    // Test print functions (should not crash)
    pico_rtos_health_print_summary();
    
    printf("✓ Utility functions test passed\n");
}

static void test_periodic_update(void)
{
    printf("Testing periodic update...\n");
    
    // Test periodic update function (should not crash)
    pico_rtos_health_periodic_update();
    
    // Test with monitoring disabled
    pico_rtos_health_set_enabled(false);
    pico_rtos_health_periodic_update();
    
    // Re-enable monitoring
    pico_rtos_health_set_enabled(true);
    
    printf("✓ Periodic update test passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void)
{
    printf("Starting system health monitoring tests...\n\n");
    
    // Initialize RTOS (required for health monitoring)
    assert(pico_rtos_init() == true);
    
    // Run tests
    test_health_init();
    test_enable_disable();
    test_configuration();
    test_metric_registration();
    test_metric_updates();
    test_system_statistics();
    test_alert_management();
    test_utility_functions();
    test_periodic_update();
    
    printf("\n✓ All system health monitoring tests passed!\n");
    return 0;
}

// Task function for testing (if needed)
void health_test_task(void *param)
{
    (void)param;
    main();
    pico_rtos_task_delete(NULL);
}