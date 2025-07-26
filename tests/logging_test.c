/**
 * @file logging_test.c
 * @brief Unit tests for enhanced logging system
 */

#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t message_count;
    pico_rtos_log_entry_t last_entry;
    bool callback_called;
    char captured_messages[10][128];
    uint32_t captured_count;
} test_log_data_t;

static test_log_data_t g_test_data = {0};

// =============================================================================
// TEST OUTPUT FUNCTIONS
// =============================================================================

static void test_output_handler(const pico_rtos_log_entry_t *entry)
{
    if (entry == NULL) return;
    
    g_test_data.message_count++;
    g_test_data.last_entry = *entry;
    g_test_data.callback_called = true;
    
    if (g_test_data.captured_count < 10) {
        strncpy(g_test_data.captured_messages[g_test_data.captured_count], 
                entry->message, 127);
        g_test_data.captured_messages[g_test_data.captured_count][127] = '\0';
        g_test_data.captured_count++;
    }
}

static void secondary_output_handler(const pico_rtos_log_entry_t *entry)
{
    if (entry == NULL) return;
    // Just increment a counter to verify this handler was called
    g_test_data.message_count += 100; // Use different increment to distinguish
}

static bool test_filter_function(const pico_rtos_log_entry_t *entry)
{
    if (entry == NULL) return false;
    
    // Filter out messages containing "FILTERED"
    return (strstr(entry->message, "FILTERED") == NULL);
}

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static void reset_test_data(void)
{
    memset(&g_test_data, 0, sizeof(g_test_data));
}

static void test_logging_init(void)
{
    printf("Testing logging initialization...\n");
    
    reset_test_data();
    
    // Test initialization
    pico_rtos_log_init(test_output_handler);
    
    // Test basic logging
    PICO_RTOS_LOG_INFO("Test message");
    assert(g_test_data.callback_called == true);
    assert(g_test_data.message_count == 1);
    assert(strcmp(g_test_data.last_entry.message, "Test message") == 0);
    
    printf("✓ Logging initialization test passed\n");
}

static void test_log_levels(void)
{
    printf("Testing log levels...\n");
    
    reset_test_data();
    pico_rtos_log_init(test_output_handler);
    
    // Set log level to WARN
    pico_rtos_log_set_level(PICO_RTOS_LOG_LEVEL_WARN);
    assert(pico_rtos_log_get_level() == PICO_RTOS_LOG_LEVEL_WARN);
    
    // ERROR should pass
    PICO_RTOS_LOG_ERROR("Error message");
    assert(g_test_data.message_count == 1);
    
    // WARN should pass
    PICO_RTOS_LOG_WARN("Warning message");
    assert(g_test_data.message_count == 2);
    
    // INFO should be filtered out
    PICO_RTOS_LOG_INFO("Info message");
    assert(g_test_data.message_count == 2); // Should still be 2
    
    // DEBUG should be filtered out
    PICO_RTOS_LOG_DEBUG("Debug message");
    assert(g_test_data.message_count == 2); // Should still be 2
    
    printf("✓ Log levels test passed\n");
}

static void test_subsystem_filtering(void)
{
    printf("Testing subsystem filtering...\n");
    
    reset_test_data();
    pico_rtos_log_init(test_output_handler);
    
    // Enable only CORE and TASK subsystems
    pico_rtos_log_disable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_ALL);
    pico_rtos_log_enable_subsystem(PICO_RTOS_LOG_SUBSYSTEM_CORE | PICO_RTOS_LOG_SUBSYSTEM_TASK);
    
    // CORE message should pass
    PICO_RTOS_LOG_CORE_INFO("Core message");
    assert(g_test_data.message_count == 1);
    
    // TASK message should pass
    PICO_RTOS_LOG_TASK_INFO("Task message");
    assert(g_test_data.message_count == 2);
    
    // MUTEX message should be filtered out
    PICO_RTOS_LOG_MUTEX_INFO("Mutex message");
    assert(g_test_data.message_count == 2); // Should still be 2
    
    // Check subsystem enabled status
    assert(pico_rtos_log_is_subsystem_enabled(PICO_RTOS_LOG_SUBSYSTEM_CORE) == true);
    assert(pico_rtos_log_is_subsystem_enabled(PICO_RTOS_LOG_SUBSYSTEM_MUTEX) == false);
    
    printf("✓ Subsystem filtering test passed\n");
}

static void test_multiple_output_handlers(void)
{
    printf("Testing multiple output handlers...\n");
    
    reset_test_data();
    pico_rtos_log_init(test_output_handler);
    
    // Add a second output handler
    pico_rtos_log_output_handler_t handler2 = {
        .output_func = secondary_output_handler,
        .min_level = PICO_RTOS_LOG_LEVEL_INFO,
        .subsystem_mask = PICO_RTOS_LOG_SUBSYSTEM_ALL,
        .enabled = true,
        .name = "Secondary Handler"
    };
    
    assert(pico_rtos_log_add_output_handler(&handler2) == true);
    
    // Log a message - should trigger both handlers
    PICO_RTOS_LOG_INFO("Multi-handler test");
    
    // First handler increments by 1, second by 100
    assert(g_test_data.message_count == 101);
    
    // Remove second handler
    assert(pico_rtos_log_remove_output_handler(secondary_output_handler) == true);
    
    // Reset counter and test again
    g_test_data.message_count = 0;
    PICO_RTOS_LOG_INFO("Single handler test");
    assert(g_test_data.message_count == 1); // Only first handler should be called
    
    printf("✓ Multiple output handlers test passed\n");
}

static void test_message_filtering(void)
{
    printf("Testing message filtering...\n");
    
    reset_test_data();
    pico_rtos_log_init(test_output_handler);
    
    // Set filter function
    pico_rtos_log_set_filter_func(test_filter_function);
    
    // Normal message should pass
    PICO_RTOS_LOG_INFO("Normal message");
    assert(g_test_data.message_count == 1);
    
    // Filtered message should be blocked
    PICO_RTOS_LOG_INFO("This message is FILTERED");
    assert(g_test_data.message_count == 1); // Should still be 1
    
    // Remove filter
    pico_rtos_log_set_filter_func(NULL);
    
    // Previously filtered message should now pass
    PICO_RTOS_LOG_INFO("This message was FILTERED");
    assert(g_test_data.message_count == 2);
    
    printf("✓ Message filtering test passed\n");
}

static void test_configuration_management(void)
{
    printf("Testing configuration management...\n");
    
    reset_test_data();
    pico_rtos_log_init(test_output_handler);
    
    // Get current configuration
    pico_rtos_log_config_t config;
    pico_rtos_log_get_config(&config);
    
    // Modify configuration
    config.timestamp_enabled = false;
    config.task_id_enabled = false;
    config.color_output_enabled = true;
    config.rate_limit_messages_per_second = 10;
    
    // Set new configuration
    assert(pico_rtos_log_set_config(&config) == true);
    
    // Verify configuration was applied
    pico_rtos_log_config_t new_config;
    pico_rtos_log_get_config(&new_config);
    
    assert(new_config.timestamp_enabled == false);
    assert(new_config.task_id_enabled == false);
    assert(new_config.color_output_enabled == true);
    assert(new_config.rate_limit_messages_per_second == 10);
    
    // Test individual configuration functions
    pico_rtos_log_enable_timestamps(true);
    pico_rtos_log_enable_task_ids(true);
    pico_rtos_log_enable_colors(false);
    pico_rtos_log_set_rate_limit(0);
    
    printf("✓ Configuration management test passed\n");
}

static void test_per_subsystem_levels(void)
{
    printf("Testing per-subsystem log levels...\n");
    
    reset_test_data();
    pico_rtos_log_init(test_output_handler);
    
    // Set different levels for different subsystems
    pico_rtos_log_set_subsystem_level(PICO_RTOS_LOG_SUBSYSTEM_CORE, PICO_RTOS_LOG_LEVEL_ERROR);
    pico_rtos_log_set_subsystem_level(PICO_RTOS_LOG_SUBSYSTEM_TASK, PICO_RTOS_LOG_LEVEL_DEBUG);
    
    // Verify levels were set
    assert(pico_rtos_log_get_subsystem_level(PICO_RTOS_LOG_SUBSYSTEM_CORE) == PICO_RTOS_LOG_LEVEL_ERROR);
    assert(pico_rtos_log_get_subsystem_level(PICO_RTOS_LOG_SUBSYSTEM_TASK) == PICO_RTOS_LOG_LEVEL_DEBUG);
    
    printf("✓ Per-subsystem levels test passed\n");
}

static void test_hex_dump(void)
{
    printf("Testing hex dump functionality...\n");
    
    reset_test_data();
    pico_rtos_log_init(test_output_handler);
    
    uint8_t test_data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    
    // Test hex dump
    pico_rtos_log_hex_dump(PICO_RTOS_LOG_LEVEL_INFO, PICO_RTOS_LOG_SUBSYSTEM_USER,
                          test_data, sizeof(test_data), "Test Data");
    
    // Should generate multiple log messages (header + data lines)
    assert(g_test_data.message_count > 1);
    
    printf("✓ Hex dump test passed\n");
}

static void test_statistics(void)
{
    printf("Testing logging statistics...\n");
    
    reset_test_data();
    pico_rtos_log_init(test_output_handler);
    
    // Reset statistics
    pico_rtos_log_reset_statistics();
    
    // Generate some log messages
    PICO_RTOS_LOG_ERROR("Error 1");
    PICO_RTOS_LOG_ERROR("Error 2");
    PICO_RTOS_LOG_WARN("Warning 1");
    PICO_RTOS_LOG_INFO("Info 1");
    PICO_RTOS_LOG_DEBUG("Debug 1");
    
    // Get statistics
    pico_rtos_log_statistics_t stats;
    pico_rtos_log_get_statistics(&stats);
    
    // Verify statistics (exact values depend on implementation)
    assert(stats.total_messages > 0);
    
    printf("✓ Statistics test passed\n");
}

static void test_output_formats(void)
{
    printf("Testing different output formats...\n");
    
    reset_test_data();
    
    // Test default output
    pico_rtos_log_init(pico_rtos_log_default_output);
    PICO_RTOS_LOG_INFO("Default format test");
    
    // Test compact output
    pico_rtos_log_set_output_func(pico_rtos_log_compact_output);
    PICO_RTOS_LOG_INFO("Compact format test");
    
    // Test colored output
    pico_rtos_log_set_output_func(pico_rtos_log_colored_output);
    PICO_RTOS_LOG_ERROR("Colored format test");
    
    // Test JSON output
    pico_rtos_log_set_output_func(pico_rtos_log_json_output);
    PICO_RTOS_LOG_INFO("JSON format test");
    
    // Test CSV output
    pico_rtos_log_set_output_func(pico_rtos_log_csv_output);
    PICO_RTOS_LOG_INFO("CSV format test");
    
    printf("✓ Output formats test passed\n");
}

static void test_utility_functions(void)
{
    printf("Testing utility functions...\n");
    
    // Test level to string conversion
    assert(strcmp(pico_rtos_log_level_to_string(PICO_RTOS_LOG_LEVEL_ERROR), "ERROR") == 0);
    assert(strcmp(pico_rtos_log_level_to_string(PICO_RTOS_LOG_LEVEL_WARN), "WARN") == 0);
    assert(strcmp(pico_rtos_log_level_to_string(PICO_RTOS_LOG_LEVEL_INFO), "INFO") == 0);
    assert(strcmp(pico_rtos_log_level_to_string(PICO_RTOS_LOG_LEVEL_DEBUG), "DEBUG") == 0);
    
    // Test subsystem to string conversion
    const char *subsys_str = pico_rtos_log_subsystem_to_string(PICO_RTOS_LOG_SUBSYSTEM_CORE);
    assert(subsys_str != NULL);
    assert(strlen(subsys_str) > 0);
    
    printf("✓ Utility functions test passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void)
{
    printf("Starting enhanced logging system tests...\n\n");
    
    // Initialize RTOS (required for logging system)
    assert(pico_rtos_init() == true);
    
    // Run tests
    test_logging_init();
    test_log_levels();
    test_subsystem_filtering();
    test_multiple_output_handlers();
    test_message_filtering();
    test_configuration_management();
    test_per_subsystem_levels();
    test_hex_dump();
    test_statistics();
    test_output_formats();
    test_utility_functions();
    
    printf("\n✓ All enhanced logging system tests passed!\n");
    return 0;
}

// Task function for testing (if needed)
void logging_test_task(void *param)
{
    (void)param;
    main();
    pico_rtos_task_delete(NULL);
}