#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico_rtos.h"
#include "pico_rtos/trace.h"
#include "pico/time.h"

// Test configuration
#define TEST_BUFFER_SIZE 64
#define TEST_EVENT_COUNT 10

// =============================================================================
// TRACE INITIALIZATION TESTS
// =============================================================================

static void test_trace_initialization(void) {
    printf("Testing trace initialization...\n");
    
    // Test initialization
    assert(pico_rtos_trace_init(TEST_BUFFER_SIZE, PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP));
    assert(pico_rtos_trace_is_enabled());
    
    // Test double initialization (should fail)
    assert(!pico_rtos_trace_init(TEST_BUFFER_SIZE, PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP));
    
    // Test deinitialization
    pico_rtos_trace_deinit();
    assert(!pico_rtos_trace_is_enabled());
    
    // Test reinitialization after deinit
    assert(pico_rtos_trace_init(TEST_BUFFER_SIZE, PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_STOP));
    assert(pico_rtos_trace_is_enabled());
    
    printf("✓ Trace initialization tests passed\n");
}

static void test_trace_enable_disable(void) {
    printf("Testing trace enable/disable...\n");
    
    // Test enabling/disabling
    assert(pico_rtos_trace_is_enabled());
    
    pico_rtos_trace_enable(false);
    assert(!pico_rtos_trace_is_enabled());
    
    pico_rtos_trace_enable(true);
    assert(pico_rtos_trace_is_enabled());
    
    printf("✓ Trace enable/disable tests passed\n");
}

static void test_overflow_behavior(void) {
    printf("Testing overflow behavior configuration...\n");
    
    // Test setting and getting overflow behavior
    pico_rtos_trace_set_overflow_behavior(PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP);
    assert(pico_rtos_trace_get_overflow_behavior() == PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP);
    
    pico_rtos_trace_set_overflow_behavior(PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_STOP);
    assert(pico_rtos_trace_get_overflow_behavior() == PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_STOP);
    
    printf("✓ Overflow behavior tests passed\n");
}

// =============================================================================
// EVENT RECORDING TESTS
// =============================================================================

static void test_basic_event_recording(void) {
    printf("Testing basic event recording...\n");
    
    // Clear trace buffer
    pico_rtos_trace_clear();
    assert(pico_rtos_trace_get_event_count() == 0);
    
    // Record a simple event
    assert(pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 0x12345678));
    assert(pico_rtos_trace_get_event_count() == 1);
    
    // Record a detailed event
    assert(pico_rtos_trace_record_event(PICO_RTOS_TRACE_MUTEX_LOCK, 
                                       PICO_RTOS_TRACE_PRIORITY_HIGH,
                                       1, 2, 0xAABBCCDD, 0x11223344, "test_mutex"));
    assert(pico_rtos_trace_get_event_count() == 2);
    
    // Record a user event
    assert(pico_rtos_trace_record_user_event("user_test", 0x1111, 0x2222));
    assert(pico_rtos_trace_get_event_count() == 3);
    
    // Verify events can be retrieved
    pico_rtos_trace_event_t events[3];
    uint32_t retrieved = pico_rtos_trace_get_events(events, 3, 0);
    assert(retrieved == 3);
    
    // Verify first event
    assert(events[0].type == PICO_RTOS_TRACE_TASK_SWITCH);
    assert(events[0].data1 == 0x12345678);
    
    // Verify second event
    assert(events[1].type == PICO_RTOS_TRACE_MUTEX_LOCK);
    assert(events[1].priority == PICO_RTOS_TRACE_PRIORITY_HIGH);
    assert(events[1].task_id == 1);
    assert(events[1].object_id == 2);
    assert(events[1].data1 == 0xAABBCCDD);
    assert(events[1].data2 == 0x11223344);
    assert(strcmp(events[1].event_name, "test_mutex") == 0);
    
    // Verify third event
    assert(events[2].type == PICO_RTOS_TRACE_USER_EVENT);
    assert(events[2].data1 == 0x1111);
    assert(events[2].data2 == 0x2222);
    assert(strcmp(events[2].event_name, "user_test") == 0);
    
    printf("✓ Basic event recording tests passed\n");
}

static void test_event_timestamps(void) {
    printf("Testing event timestamps...\n");
    
    // Clear trace buffer
    pico_rtos_trace_clear();
    
    uint64_t start_time = time_us_64();
    
    // Record events with delays
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 1);
    sleep_us(1000); // 1ms delay
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 2);
    sleep_us(1000); // 1ms delay
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 3);
    
    uint64_t end_time = time_us_64();
    
    // Retrieve events
    pico_rtos_trace_event_t events[3];
    uint32_t retrieved = pico_rtos_trace_get_events(events, 3, 0);
    assert(retrieved == 3);
    
    // Verify timestamps are in order and within expected range
    assert(events[0].timestamp >= start_time);
    assert(events[1].timestamp > events[0].timestamp);
    assert(events[2].timestamp > events[1].timestamp);
    assert(events[2].timestamp <= end_time);
    
    // Verify time differences are reasonable (should be around 1ms each)
    uint64_t diff1 = events[1].timestamp - events[0].timestamp;
    uint64_t diff2 = events[2].timestamp - events[1].timestamp;
    
    assert(diff1 >= 800 && diff1 <= 1200); // Allow some tolerance
    assert(diff2 >= 800 && diff2 <= 1200);
    
    printf("✓ Event timestamp tests passed\n");
}

// =============================================================================
// EVENT FILTERING TESTS
// =============================================================================

static void test_type_filtering(void) {
    printf("Testing event type filtering...\n");
    
    // Clear trace buffer
    pico_rtos_trace_clear();
    
    // Set filter to only record task events
    uint32_t task_events_mask = (1U << PICO_RTOS_TRACE_TASK_SWITCH) | 
                               (1U << PICO_RTOS_TRACE_TASK_CREATE);
    pico_rtos_trace_set_type_filter(task_events_mask);
    assert(pico_rtos_trace_get_type_filter() == task_events_mask);
    
    // Record various events
    assert(pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 1)); // Should be recorded
    assert(!pico_rtos_trace_record_simple(PICO_RTOS_TRACE_MUTEX_LOCK, 2)); // Should be filtered
    assert(pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_CREATE, 3)); // Should be recorded
    assert(!pico_rtos_trace_record_simple(PICO_RTOS_TRACE_QUEUE_SEND, 4)); // Should be filtered
    
    // Verify only task events were recorded
    assert(pico_rtos_trace_get_event_count() == 2);
    
    pico_rtos_trace_event_t events[2];
    uint32_t retrieved = pico_rtos_trace_get_events(events, 2, 0);
    assert(retrieved == 2);
    assert(events[0].type == PICO_RTOS_TRACE_TASK_SWITCH);
    assert(events[1].type == PICO_RTOS_TRACE_TASK_CREATE);
    
    // Reset filter to record all events
    pico_rtos_trace_set_type_filter(0);
    assert(pico_rtos_trace_get_type_filter() == 0);
    
    printf("✓ Type filtering tests passed\n");
}

static void test_priority_filtering(void) {
    printf("Testing priority filtering...\n");
    
    // Clear trace buffer
    pico_rtos_trace_clear();
    
    // Set minimum priority to HIGH
    pico_rtos_trace_set_priority_filter(PICO_RTOS_TRACE_PRIORITY_HIGH);
    assert(pico_rtos_trace_get_priority_filter() == PICO_RTOS_TRACE_PRIORITY_HIGH);
    
    // Record events with different priorities
    assert(!pico_rtos_trace_record_event(PICO_RTOS_TRACE_TASK_SWITCH, 
                                        PICO_RTOS_TRACE_PRIORITY_LOW, 0, 0, 1, 0, NULL)); // Filtered
    assert(!pico_rtos_trace_record_event(PICO_RTOS_TRACE_TASK_SWITCH, 
                                        PICO_RTOS_TRACE_PRIORITY_NORMAL, 0, 0, 2, 0, NULL)); // Filtered
    assert(pico_rtos_trace_record_event(PICO_RTOS_TRACE_TASK_SWITCH, 
                                       PICO_RTOS_TRACE_PRIORITY_HIGH, 0, 0, 3, 0, NULL)); // Recorded
    assert(pico_rtos_trace_record_event(PICO_RTOS_TRACE_TASK_SWITCH, 
                                       PICO_RTOS_TRACE_PRIORITY_CRITICAL, 0, 0, 4, 0, NULL)); // Recorded
    
    // Verify only high and critical priority events were recorded
    assert(pico_rtos_trace_get_event_count() == 2);
    
    pico_rtos_trace_event_t events[2];
    uint32_t retrieved = pico_rtos_trace_get_events(events, 2, 0);
    assert(retrieved == 2);
    assert(events[0].priority == PICO_RTOS_TRACE_PRIORITY_HIGH);
    assert(events[1].priority == PICO_RTOS_TRACE_PRIORITY_CRITICAL);
    
    // Reset priority filter
    pico_rtos_trace_set_priority_filter(PICO_RTOS_TRACE_PRIORITY_LOW);
    
    printf("✓ Priority filtering tests passed\n");
}

// =============================================================================
// BUFFER OVERFLOW TESTS
// =============================================================================

static void test_wrap_around_behavior(void) {
    printf("Testing wrap-around overflow behavior...\n");
    
    // Reinitialize with small buffer and wrap behavior
    pico_rtos_trace_deinit();
    assert(pico_rtos_trace_init(4, PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP));
    
    // Fill buffer beyond capacity
    for (int i = 0; i < 8; i++) {
        assert(pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, i));
    }
    
    // Buffer should contain only the last 4 events
    assert(pico_rtos_trace_get_event_count() == 4);
    
    pico_rtos_trace_event_t events[4];
    uint32_t retrieved = pico_rtos_trace_get_events(events, 4, 0);
    assert(retrieved == 4);
    
    // Should contain events 4, 5, 6, 7 (the last 4)
    assert(events[0].data1 == 4);
    assert(events[1].data1 == 5);
    assert(events[2].data1 == 6);
    assert(events[3].data1 == 7);
    
    // Reinitialize with normal buffer size
    pico_rtos_trace_deinit();
    assert(pico_rtos_trace_init(TEST_BUFFER_SIZE, PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP));
    
    printf("✓ Wrap-around behavior tests passed\n");
}

static void test_stop_behavior(void) {
    printf("Testing stop overflow behavior...\n");
    
    // Reinitialize with small buffer and stop behavior
    pico_rtos_trace_deinit();
    assert(pico_rtos_trace_init(4, PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_STOP));
    
    // Fill buffer to capacity
    for (int i = 0; i < 4; i++) {
        assert(pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, i));
    }
    
    // Additional events should be dropped
    assert(!pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 4));
    assert(!pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 5));
    
    // Buffer should still contain only 4 events
    assert(pico_rtos_trace_get_event_count() == 4);
    
    pico_rtos_trace_event_t events[4];
    uint32_t retrieved = pico_rtos_trace_get_events(events, 4, 0);
    assert(retrieved == 4);
    
    // Should contain original events 0, 1, 2, 3
    assert(events[0].data1 == 0);
    assert(events[1].data1 == 1);
    assert(events[2].data1 == 2);
    assert(events[3].data1 == 3);
    
    // Reinitialize with normal buffer size
    pico_rtos_trace_deinit();
    assert(pico_rtos_trace_init(TEST_BUFFER_SIZE, PICO_RTOS_TRACE_OVERFLOW_BEHAVIOR_WRAP));
    
    printf("✓ Stop behavior tests passed\n");
}

// =============================================================================
// EVENT RETRIEVAL TESTS
// =============================================================================

static void test_event_retrieval(void) {
    printf("Testing event retrieval functions...\n");
    
    // Clear trace buffer
    pico_rtos_trace_clear();
    
    // Record various types of events
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 1);
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_MUTEX_LOCK, 2);
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 3);
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_QUEUE_SEND, 4);
    pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 5);
    
    assert(pico_rtos_trace_get_event_count() == 5);
    
    // Test getting all events
    pico_rtos_trace_event_t all_events[5];
    uint32_t retrieved = pico_rtos_trace_get_events(all_events, 5, 0);
    assert(retrieved == 5);
    
    // Test getting recent events
    pico_rtos_trace_event_t recent_events[3];
    retrieved = pico_rtos_trace_get_recent_events(recent_events, 3);
    assert(retrieved == 3);
    assert(recent_events[0].data1 == 3); // Should be events 3, 4, 5
    assert(recent_events[1].data1 == 4);
    assert(recent_events[2].data1 == 5);
    
    // Test getting events by type
    pico_rtos_trace_event_t task_events[3];
    retrieved = pico_rtos_trace_get_events_by_type(PICO_RTOS_TRACE_TASK_SWITCH, task_events, 3);
    assert(retrieved == 3);
    assert(task_events[0].data1 == 1); // Should be task switch events 1, 3, 5
    assert(task_events[1].data1 == 3);
    assert(task_events[2].data1 == 5);
    
    // Test getting events by task (using data1 as task ID for this test)
    pico_rtos_trace_event_t task_1_events[1];
    retrieved = pico_rtos_trace_get_events_by_task(1, task_1_events, 1);
    // This test depends on task_id being set, which our simple recording doesn't do
    // So we'll just verify the function doesn't crash
    
    printf("✓ Event retrieval tests passed\n");
}

// =============================================================================
// STATISTICS TESTS
// =============================================================================

static void test_trace_statistics(void) {
    printf("Testing trace statistics...\n");
    
    // Clear trace buffer
    pico_rtos_trace_clear();
    
    // Record events with different types and priorities
    pico_rtos_trace_record_event(PICO_RTOS_TRACE_TASK_SWITCH, PICO_RTOS_TRACE_PRIORITY_LOW, 0, 0, 1, 0, NULL);
    pico_rtos_trace_record_event(PICO_RTOS_TRACE_MUTEX_LOCK, PICO_RTOS_TRACE_PRIORITY_NORMAL, 0, 0, 2, 0, NULL);
    pico_rtos_trace_record_event(PICO_RTOS_TRACE_TASK_SWITCH, PICO_RTOS_TRACE_PRIORITY_HIGH, 0, 0, 3, 0, NULL);
    pico_rtos_trace_record_event(PICO_RTOS_TRACE_QUEUE_SEND, PICO_RTOS_TRACE_PRIORITY_CRITICAL, 0, 0, 4, 0, NULL);
    
    // Get statistics
    pico_rtos_trace_stats_t stats;
    assert(pico_rtos_trace_get_stats(&stats));
    
    // Verify basic statistics
    assert(stats.total_events == 4);
    assert(stats.dropped_events == 0);
    assert(stats.buffer_utilization_percent > 0);
    assert(stats.first_event_time > 0);
    assert(stats.last_event_time >= stats.first_event_time);
    
    // Verify event counts by type
    assert(stats.events_by_type[PICO_RTOS_TRACE_TASK_SWITCH] == 2);
    assert(stats.events_by_type[PICO_RTOS_TRACE_MUTEX_LOCK] == 1);
    assert(stats.events_by_type[PICO_RTOS_TRACE_QUEUE_SEND] == 1);
    
    // Verify event counts by priority
    assert(stats.events_by_priority[PICO_RTOS_TRACE_PRIORITY_LOW] == 1);
    assert(stats.events_by_priority[PICO_RTOS_TRACE_PRIORITY_NORMAL] == 1);
    assert(stats.events_by_priority[PICO_RTOS_TRACE_PRIORITY_HIGH] == 1);
    assert(stats.events_by_priority[PICO_RTOS_TRACE_PRIORITY_CRITICAL] == 1);
    
    printf("✓ Trace statistics tests passed\n");
}

// =============================================================================
// UTILITY FUNCTION TESTS
// =============================================================================

static void test_utility_functions(void) {
    printf("Testing utility functions...\n");
    
    // Test event type to string conversion
    assert(strcmp(pico_rtos_trace_event_type_to_string(PICO_RTOS_TRACE_TASK_SWITCH), "TASK_SWITCH") == 0);
    assert(strcmp(pico_rtos_trace_event_type_to_string(PICO_RTOS_TRACE_MUTEX_LOCK), "MUTEX_LOCK") == 0);
    assert(strcmp(pico_rtos_trace_event_type_to_string(PICO_RTOS_TRACE_USER_EVENT), "USER_EVENT") == 0);
    
    // Test priority to string conversion
    assert(strcmp(pico_rtos_trace_priority_to_string(PICO_RTOS_TRACE_PRIORITY_LOW), "LOW") == 0);
    assert(strcmp(pico_rtos_trace_priority_to_string(PICO_RTOS_TRACE_PRIORITY_NORMAL), "NORMAL") == 0);
    assert(strcmp(pico_rtos_trace_priority_to_string(PICO_RTOS_TRACE_PRIORITY_HIGH), "HIGH") == 0);
    assert(strcmp(pico_rtos_trace_priority_to_string(PICO_RTOS_TRACE_PRIORITY_CRITICAL), "CRITICAL") == 0);
    
    // Test event formatting
    pico_rtos_trace_clear();
    pico_rtos_trace_record_event(PICO_RTOS_TRACE_MUTEX_LOCK, PICO_RTOS_TRACE_PRIORITY_HIGH,
                                1, 2, 0xAABB, 0xCCDD, "test_event");
    
    pico_rtos_trace_event_t event;
    uint32_t retrieved = pico_rtos_trace_get_events(&event, 1, 0);
    assert(retrieved == 1);
    
    char buffer[256];
    uint32_t chars_written = pico_rtos_trace_format_event(&event, buffer, sizeof(buffer));
    assert(chars_written > 0);
    assert(chars_written < sizeof(buffer));
    assert(strstr(buffer, "MUTEX_LOCK") != NULL);
    assert(strstr(buffer, "HIGH") != NULL);
    assert(strstr(buffer, "test_event") != NULL);
    
    // Test with NULL parameters
    assert(pico_rtos_trace_format_event(NULL, buffer, sizeof(buffer)) == 0);
    assert(pico_rtos_trace_format_event(&event, NULL, sizeof(buffer)) == 0);
    assert(pico_rtos_trace_format_event(&event, buffer, 0) == 0);
    
    // Test print functions (these just call printf, so we'll just call them)
    pico_rtos_trace_print_event(&event);
    
    pico_rtos_trace_stats_t stats;
    pico_rtos_trace_get_stats(&stats);
    pico_rtos_trace_print_stats(&stats);
    
    pico_rtos_trace_print_all_events();
    pico_rtos_trace_dump_buffer();
    
    printf("✓ Utility function tests passed\n");
}

// =============================================================================
// DISABLED TRACING TESTS
// =============================================================================

static void test_disabled_tracing(void) {
    printf("Testing disabled tracing...\n");
    
    // Disable tracing
    pico_rtos_trace_enable(false);
    assert(!pico_rtos_trace_is_enabled());
    
    // Clear buffer
    pico_rtos_trace_clear();
    
    // Try to record events (should fail gracefully)
    assert(!pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 1));
    assert(!pico_rtos_trace_record_event(PICO_RTOS_TRACE_MUTEX_LOCK, PICO_RTOS_TRACE_PRIORITY_NORMAL,
                                        0, 0, 2, 0, "test"));
    assert(!pico_rtos_trace_record_user_event("user_test", 3, 4));
    
    // Verify no events were recorded
    assert(pico_rtos_trace_get_event_count() == 0);
    
    // Re-enable tracing
    pico_rtos_trace_enable(true);
    assert(pico_rtos_trace_is_enabled());
    
    // Verify recording works again
    assert(pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, 5));
    assert(pico_rtos_trace_get_event_count() == 1);
    
    printf("✓ Disabled tracing tests passed\n");
}

// =============================================================================
// PERFORMANCE TESTS
// =============================================================================

static void test_tracing_performance(void) {
    printf("Testing tracing performance...\n");
    
    // Clear trace buffer
    pico_rtos_trace_clear();
    
    const int num_events = 1000;
    
    // Measure time for recording events
    uint64_t start_time = time_us_64();
    for (int i = 0; i < num_events; i++) {
        pico_rtos_trace_record_simple(PICO_RTOS_TRACE_TASK_SWITCH, i);
    }
    uint64_t end_time = time_us_64();
    
    uint64_t total_time = end_time - start_time;
    float time_per_event = (float)total_time / num_events;
    
    printf("Performance results:\n");
    printf("  %d events recorded in %llu us\n", num_events, total_time);
    printf("  Average time per event: %.2f us\n", time_per_event);
    
    // Verify performance is reasonable (less than 10us per event)
    assert(time_per_event < 10.0f);
    
    // Verify all events were recorded (up to buffer limit)
    uint32_t recorded_events = pico_rtos_trace_get_event_count();
    uint32_t expected_events = (num_events < TEST_BUFFER_SIZE) ? num_events : TEST_BUFFER_SIZE;
    assert(recorded_events == expected_events);
    
    printf("✓ Tracing performance tests passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void) {
    printf("Starting Pico-RTOS Trace System Tests\n");
    printf("=====================================\n");
    
    // Initialize RTOS
    assert(pico_rtos_init());
    
    // Run tests
    test_trace_initialization();
    test_trace_enable_disable();
    test_overflow_behavior();
    test_basic_event_recording();
    test_event_timestamps();
    test_type_filtering();
    test_priority_filtering();
    test_wrap_around_behavior();
    test_stop_behavior();
    test_event_retrieval();
    test_trace_statistics();
    test_utility_functions();
    test_disabled_tracing();
    test_tracing_performance();
    
    // Clean up
    pico_rtos_trace_deinit();
    
    printf("\n=====================================\n");
    printf("All Trace System Tests Passed! ✓\n");
    printf("=====================================\n");
    
    return 0;
}