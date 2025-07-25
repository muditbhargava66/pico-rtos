/**
 * @file hires_timer_test.c
 * @brief Unit tests for high-resolution software timers
 */

#include "pico_rtos/hires_timer.h"
#include "pico_rtos.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t callback_count;
    uint64_t last_callback_time;
    void *last_param;
    bool callback_executed;
} test_callback_data_t;

static test_callback_data_t g_test_data = {0};

// =============================================================================
// TEST CALLBACK FUNCTIONS
// =============================================================================

static void test_timer_callback(void *param)
{
    g_test_data.callback_count++;
    g_test_data.last_callback_time = pico_rtos_hires_timer_get_time_us();
    g_test_data.last_param = param;
    g_test_data.callback_executed = true;
}

static void periodic_timer_callback(void *param)
{
    uint32_t *counter = (uint32_t *)param;
    (*counter)++;
}

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static void test_hires_timer_init(void)
{
    printf("Testing high-resolution timer initialization...\n");
    
    // Test initialization
    assert(pico_rtos_hires_timer_init() == true);
    
    // Test double initialization
    assert(pico_rtos_hires_timer_init() == true);
    
    printf("✓ High-resolution timer initialization test passed\n");
}

static void test_timer_creation(void)
{
    printf("Testing timer creation...\n");
    
    pico_rtos_hires_timer_t timer;
    
    // Test valid timer creation
    assert(pico_rtos_hires_timer_create(&timer, "test_timer", test_timer_callback, 
                                       (void *)0x12345678, 1000, 
                                       PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == true);
    
    assert(pico_rtos_hires_timer_is_active(&timer) == true);
    assert(pico_rtos_hires_timer_get_state(&timer) == PICO_RTOS_HIRES_TIMER_STATE_STOPPED);
    assert(pico_rtos_hires_timer_get_period_us(&timer) == 1000);
    assert(pico_rtos_hires_timer_get_mode(&timer) == PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT);
    
    // Test invalid parameters
    pico_rtos_hires_timer_t invalid_timer;
    assert(pico_rtos_hires_timer_create(NULL, "test", test_timer_callback, 
                                       NULL, 1000, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == false);
    assert(pico_rtos_hires_timer_create(&invalid_timer, "test", NULL, 
                                       NULL, 1000, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == false);
    assert(pico_rtos_hires_timer_create(&invalid_timer, "test", test_timer_callback, 
                                       NULL, 5, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == false); // Too short period
    
    printf("✓ Timer creation test passed\n");
}

static void test_timer_start_stop(void)
{
    printf("Testing timer start/stop operations...\n");
    
    pico_rtos_hires_timer_t timer;
    
    assert(pico_rtos_hires_timer_create(&timer, "start_stop_test", test_timer_callback, 
                                       NULL, 1000, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == true);
    
    // Test starting timer
    assert(pico_rtos_hires_timer_start(&timer) == true);
    assert(pico_rtos_hires_timer_is_running(&timer) == true);
    assert(pico_rtos_hires_timer_get_state(&timer) == PICO_RTOS_HIRES_TIMER_STATE_RUNNING);
    
    // Test remaining time
    uint64_t remaining = pico_rtos_hires_timer_get_remaining_time_us(&timer);
    assert(remaining > 0 && remaining <= 1000);
    
    // Test stopping timer
    assert(pico_rtos_hires_timer_stop(&timer) == true);
    assert(pico_rtos_hires_timer_is_running(&timer) == false);
    assert(pico_rtos_hires_timer_get_state(&timer) == PICO_RTOS_HIRES_TIMER_STATE_STOPPED);
    
    // Test double start/stop
    assert(pico_rtos_hires_timer_start(&timer) == true);
    assert(pico_rtos_hires_timer_start(&timer) == true); // Should succeed (already running)
    assert(pico_rtos_hires_timer_stop(&timer) == true);
    assert(pico_rtos_hires_timer_stop(&timer) == true); // Should succeed (already stopped)
    
    printf("✓ Timer start/stop test passed\n");
}

static void test_timer_reset(void)
{
    printf("Testing timer reset...\n");
    
    pico_rtos_hires_timer_t timer;
    
    assert(pico_rtos_hires_timer_create(&timer, "reset_test", test_timer_callback, 
                                       NULL, 2000, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == true);
    
    // Start timer and let it run for a bit
    assert(pico_rtos_hires_timer_start(&timer) == true);
    pico_rtos_hires_timer_delay_us(500); // Wait 500us
    
    uint64_t remaining_before = pico_rtos_hires_timer_get_remaining_time_us(&timer);
    
    // Reset timer
    assert(pico_rtos_hires_timer_reset(&timer) == true);
    assert(pico_rtos_hires_timer_is_running(&timer) == true);
    
    uint64_t remaining_after = pico_rtos_hires_timer_get_remaining_time_us(&timer);
    
    // After reset, remaining time should be close to full period
    assert(remaining_after > remaining_before);
    assert(remaining_after > 1800); // Should be close to 2000us
    
    pico_rtos_hires_timer_stop(&timer);
    
    printf("✓ Timer reset test passed\n");
}

static void test_timer_period_change(void)
{
    printf("Testing timer period change...\n");
    
    pico_rtos_hires_timer_t timer;
    
    assert(pico_rtos_hires_timer_create(&timer, "period_test", test_timer_callback, 
                                       NULL, 1000, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == true);
    
    // Test changing period while stopped
    assert(pico_rtos_hires_timer_change_period(&timer, 2000) == true);
    assert(pico_rtos_hires_timer_get_period_us(&timer) == 2000);
    
    // Test changing period while running
    assert(pico_rtos_hires_timer_start(&timer) == true);
    assert(pico_rtos_hires_timer_change_period(&timer, 3000) == true);
    assert(pico_rtos_hires_timer_get_period_us(&timer) == 3000);
    assert(pico_rtos_hires_timer_is_running(&timer) == true);
    
    // Test invalid period
    assert(pico_rtos_hires_timer_change_period(&timer, 5) == false); // Too short
    
    pico_rtos_hires_timer_stop(&timer);
    
    printf("✓ Timer period change test passed\n");
}

static void test_timer_mode_change(void)
{
    printf("Testing timer mode change...\n");
    
    pico_rtos_hires_timer_t timer;
    
    assert(pico_rtos_hires_timer_create(&timer, "mode_test", test_timer_callback, 
                                       NULL, 1000, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == true);
    
    // Test changing mode
    assert(pico_rtos_hires_timer_change_mode(&timer, PICO_RTOS_HIRES_TIMER_MODE_PERIODIC) == true);
    assert(pico_rtos_hires_timer_get_mode(&timer) == PICO_RTOS_HIRES_TIMER_MODE_PERIODIC);
    
    assert(pico_rtos_hires_timer_change_mode(&timer, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == true);
    assert(pico_rtos_hires_timer_get_mode(&timer) == PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT);
    
    printf("✓ Timer mode change test passed\n");
}

static void test_one_shot_timer_expiration(void)
{
    printf("Testing one-shot timer expiration...\n");
    
    pico_rtos_hires_timer_t timer;
    g_test_data.callback_count = 0;
    g_test_data.callback_executed = false;
    
    assert(pico_rtos_hires_timer_create(&timer, "oneshot_test", test_timer_callback, 
                                       (void *)0xDEADBEEF, 1000, 
                                       PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == true);
    
    uint64_t start_time = pico_rtos_hires_timer_get_time_us();
    assert(pico_rtos_hires_timer_start(&timer) == true);
    
    // Wait for timer to expire (with some margin)
    pico_rtos_hires_timer_delay_us(1500);
    
    // Process any pending expirations
    pico_rtos_hires_timer_process_expirations();
    
    // Check that callback was executed
    assert(g_test_data.callback_executed == true);
    assert(g_test_data.callback_count == 1);
    assert(g_test_data.last_param == (void *)0xDEADBEEF);
    
    // Check timing accuracy (within 100us tolerance)
    uint64_t expected_callback_time = start_time + 1000;
    uint64_t actual_callback_time = g_test_data.last_callback_time;
    int64_t timing_error = (int64_t)(actual_callback_time - expected_callback_time);
    assert(timing_error >= -100 && timing_error <= 100);
    
    // Timer should be in expired state
    assert(pico_rtos_hires_timer_get_state(&timer) == PICO_RTOS_HIRES_TIMER_STATE_EXPIRED);
    assert(pico_rtos_hires_timer_is_running(&timer) == false);
    
    printf("✓ One-shot timer expiration test passed\n");
}

static void test_periodic_timer_expiration(void)
{
    printf("Testing periodic timer expiration...\n");
    
    pico_rtos_hires_timer_t timer;
    uint32_t callback_counter = 0;
    
    assert(pico_rtos_hires_timer_create(&timer, "periodic_test", periodic_timer_callback, 
                                       &callback_counter, 500, 
                                       PICO_RTOS_HIRES_TIMER_MODE_PERIODIC) == true);
    
    assert(pico_rtos_hires_timer_start(&timer) == true);
    
    // Wait for multiple periods
    uint64_t test_duration = 2500; // 5 periods
    uint64_t start_time = pico_rtos_hires_timer_get_time_us();
    
    while ((pico_rtos_hires_timer_get_time_us() - start_time) < test_duration) {
        pico_rtos_hires_timer_process_expirations();
        pico_rtos_hires_timer_delay_us(50); // Small delay to prevent busy loop
    }
    
    // Should have executed approximately 5 times
    assert(callback_counter >= 4 && callback_counter <= 6); // Allow some tolerance
    
    // Timer should still be running
    assert(pico_rtos_hires_timer_is_running(&timer) == true);
    assert(pico_rtos_hires_timer_get_state(&timer) == PICO_RTOS_HIRES_TIMER_STATE_RUNNING);
    
    pico_rtos_hires_timer_stop(&timer);
    
    printf("✓ Periodic timer expiration test passed (executed %u times)\n", callback_counter);
}

static void test_timer_accuracy(void)
{
    printf("Testing timer accuracy...\n");
    
    pico_rtos_hires_timer_t timer;
    uint32_t callback_counter = 0;
    
    assert(pico_rtos_hires_timer_create(&timer, "accuracy_test", periodic_timer_callback, 
                                       &callback_counter, 1000, 
                                       PICO_RTOS_HIRES_TIMER_MODE_PERIODIC) == true);
    
    uint64_t start_time = pico_rtos_hires_timer_get_time_us();
    assert(pico_rtos_hires_timer_start(&timer) == true);
    
    // Run for 10 periods
    uint64_t test_duration = 10000;
    
    while ((pico_rtos_hires_timer_get_time_us() - start_time) < test_duration) {
        pico_rtos_hires_timer_process_expirations();
        pico_rtos_hires_timer_delay_us(50);
    }
    
    uint64_t actual_duration = pico_rtos_hires_timer_get_time_us() - start_time;
    
    // Calculate timing accuracy
    double expected_callbacks = (double)actual_duration / 1000.0;
    double accuracy = ((double)callback_counter / expected_callbacks) * 100.0;
    
    printf("  Expected callbacks: %.1f, Actual: %u, Accuracy: %.1f%%\n", 
           expected_callbacks, callback_counter, accuracy);
    
    // Accuracy should be within 5%
    assert(accuracy >= 95.0 && accuracy <= 105.0);
    
    pico_rtos_hires_timer_stop(&timer);
    
    printf("✓ Timer accuracy test passed\n");
}

static void test_multiple_timers(void)
{
    printf("Testing multiple timers...\n");
    
    pico_rtos_hires_timer_t timer1, timer2, timer3;
    uint32_t counter1 = 0, counter2 = 0, counter3 = 0;
    
    // Create timers with different periods
    assert(pico_rtos_hires_timer_create(&timer1, "multi_1", periodic_timer_callback, 
                                       &counter1, 300, PICO_RTOS_HIRES_TIMER_MODE_PERIODIC) == true);
    assert(pico_rtos_hires_timer_create(&timer2, "multi_2", periodic_timer_callback, 
                                       &counter2, 500, PICO_RTOS_HIRES_TIMER_MODE_PERIODIC) == true);
    assert(pico_rtos_hires_timer_create(&timer3, "multi_3", periodic_timer_callback, 
                                       &counter3, 700, PICO_RTOS_HIRES_TIMER_MODE_PERIODIC) == true);
    
    // Start all timers
    assert(pico_rtos_hires_timer_start(&timer1) == true);
    assert(pico_rtos_hires_timer_start(&timer2) == true);
    assert(pico_rtos_hires_timer_start(&timer3) == true);
    
    // Run for a test period
    uint64_t test_duration = 3000; // 3ms
    uint64_t start_time = pico_rtos_hires_timer_get_time_us();
    
    while ((pico_rtos_hires_timer_get_time_us() - start_time) < test_duration) {
        pico_rtos_hires_timer_process_expirations();
        pico_rtos_hires_timer_delay_us(50);
    }
    
    // Check that all timers executed
    assert(counter1 > 0);
    assert(counter2 > 0);
    assert(counter3 > 0);
    
    // Timer 1 should have executed most frequently
    assert(counter1 >= counter2);
    assert(counter2 >= counter3);
    
    printf("  Timer 1 (300us): %u, Timer 2 (500us): %u, Timer 3 (700us): %u\n", 
           counter1, counter2, counter3);
    
    // Stop all timers
    pico_rtos_hires_timer_stop(&timer1);
    pico_rtos_hires_timer_stop(&timer2);
    pico_rtos_hires_timer_stop(&timer3);
    
    printf("✓ Multiple timers test passed\n");
}

static void test_timer_deletion(void)
{
    printf("Testing timer deletion...\n");
    
    pico_rtos_hires_timer_t timer;
    
    assert(pico_rtos_hires_timer_create(&timer, "delete_test", test_timer_callback, 
                                       NULL, 1000, PICO_RTOS_HIRES_TIMER_MODE_ONE_SHOT) == true);
    
    assert(pico_rtos_hires_timer_is_active(&timer) == true);
    
    // Start and then delete timer
    assert(pico_rtos_hires_timer_start(&timer) == true);
    assert(pico_rtos_hires_timer_delete(&timer) == true);
    
    // Timer should no longer be active
    assert(pico_rtos_hires_timer_is_active(&timer) == false);
    assert(pico_rtos_hires_timer_is_running(&timer) == false);
    
    // Operations on deleted timer should fail
    assert(pico_rtos_hires_timer_start(&timer) == false);
    assert(pico_rtos_hires_timer_stop(&timer) == false);
    
    printf("✓ Timer deletion test passed\n");
}

static void test_time_functions(void)
{
    printf("Testing time functions...\n");
    
    // Test time retrieval
    uint64_t time1_us = pico_rtos_hires_timer_get_time_us();
    uint64_t time1_ns = pico_rtos_hires_timer_get_time_ns();
    
    assert(time1_us > 0);
    assert(time1_ns > 0);
    assert(time1_ns >= time1_us * 1000); // ns should be at least us * 1000
    
    // Test delay function
    uint64_t delay_start = pico_rtos_hires_timer_get_time_us();
    pico_rtos_hires_timer_delay_us(1000);
    uint64_t delay_end = pico_rtos_hires_timer_get_time_us();
    
    uint64_t actual_delay = delay_end - delay_start;
    assert(actual_delay >= 950 && actual_delay <= 1050); // Within 50us tolerance
    
    // Test delay until function
    uint64_t target_time = pico_rtos_hires_timer_get_time_us() + 500;
    uint64_t delay_until_start = pico_rtos_hires_timer_get_time_us();
    assert(pico_rtos_hires_timer_delay_until_us(target_time) == true);
    uint64_t delay_until_end = pico_rtos_hires_timer_get_time_us();
    
    assert(delay_until_end >= target_time);
    assert((delay_until_end - delay_until_start) >= 450); // Should be close to 500us
    
    // Test delay until past time
    uint64_t past_time = pico_rtos_hires_timer_get_time_us() - 1000;
    assert(pico_rtos_hires_timer_delay_until_us(past_time) == false);
    
    printf("✓ Time functions test passed\n");
}

static void test_utility_functions(void)
{
    printf("Testing utility functions...\n");
    
    // Test error string function
    const char *error_str = pico_rtos_hires_timer_get_error_string(PICO_RTOS_HIRES_TIMER_ERROR_INVALID_PERIOD);
    assert(error_str != NULL);
    assert(strlen(error_str) > 0);
    
    // Test state string function
    const char *state_str = pico_rtos_hires_timer_get_state_string(PICO_RTOS_HIRES_TIMER_STATE_RUNNING);
    assert(state_str != NULL);
    assert(strcmp(state_str, "Running") == 0);
    
    // Test mode string function
    const char *mode_str = pico_rtos_hires_timer_get_mode_string(PICO_RTOS_HIRES_TIMER_MODE_PERIODIC);
    assert(mode_str != NULL);
    assert(strcmp(mode_str, "Periodic") == 0);
    
    printf("✓ Utility functions test passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void)
{
    printf("Starting high-resolution timer tests...\n\n");
    
    // Initialize RTOS (required for critical sections)
    assert(pico_rtos_init() == true);
    
    // Run tests
    test_hires_timer_init();
    test_timer_creation();
    test_timer_start_stop();
    test_timer_reset();
    test_timer_period_change();
    test_timer_mode_change();
    test_one_shot_timer_expiration();
    test_periodic_timer_expiration();
    test_timer_accuracy();
    test_multiple_timers();
    test_timer_deletion();
    test_time_functions();
    test_utility_functions();
    
    printf("\n✓ All high-resolution timer tests passed!\n");
    return 0;
}

// Task function for testing (if needed)
void hires_timer_test_task(void *param)
{
    (void)param;
    main();
    pico_rtos_task_delete(NULL);
}