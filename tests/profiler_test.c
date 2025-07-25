#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico_rtos.h"
#include "pico_rtos/profiler.h"
#include "pico/time.h"

// Test configuration
#define TEST_MAX_ENTRIES 16
#define TEST_FUNCTION_ID_1 0x12345678
#define TEST_FUNCTION_ID_2 0x87654321
#define TEST_FUNCTION_ID_3 0xABCDEF00

// Test functions to profile
static void test_function_fast(void) {
    // Simulate fast function (minimal work)
    volatile int x = 42;
    x = x + 1;
    (void)x;
}

static void test_function_slow(void) {
    // Simulate slow function (more work)
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }
    (void)sum;
}

static void test_function_variable(int iterations) {
    // Function with variable execution time
    volatile int sum = 0;
    for (int i = 0; i < iterations; i++) {
        sum += i * i;
    }
    (void)sum;
}

// =============================================================================
// PROFILER INITIALIZATION TESTS
// =============================================================================

static void test_profiler_initialization(void) {
    printf("Testing profiler initialization...\n");
    
    // Test initialization
    assert(pico_rtos_profiler_init(TEST_MAX_ENTRIES));
    assert(pico_rtos_profiler_is_enabled());
    
    // Test double initialization (should fail)
    assert(!pico_rtos_profiler_init(TEST_MAX_ENTRIES));
    
    // Test deinitialization
    pico_rtos_profiler_deinit();
    assert(!pico_rtos_profiler_is_enabled());
    
    // Test reinitialization after deinit
    assert(pico_rtos_profiler_init(TEST_MAX_ENTRIES));
    assert(pico_rtos_profiler_is_enabled());
    
    printf("✓ Profiler initialization tests passed\n");
}

static void test_profiler_enable_disable(void) {
    printf("Testing profiler enable/disable...\n");
    
    // Test enabling/disabling
    assert(pico_rtos_profiler_is_enabled());
    
    pico_rtos_profiler_enable(false);
    assert(!pico_rtos_profiler_is_enabled());
    
    pico_rtos_profiler_enable(true);
    assert(pico_rtos_profiler_is_enabled());
    
    printf("✓ Profiler enable/disable tests passed\n");
}

// =============================================================================
// BASIC PROFILING TESTS
// =============================================================================

static void test_basic_profiling(void) {
    printf("Testing basic profiling functionality...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    // Test function entry/exit profiling
    pico_rtos_profile_context_t ctx;
    
    // Profile a function call
    assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "test_function_1", &ctx));
    assert(ctx.valid);
    assert(ctx.function_id == TEST_FUNCTION_ID_1);
    
    // Simulate some work
    test_function_fast();
    
    assert(pico_rtos_profiler_function_exit(&ctx));
    assert(!ctx.valid); // Context should be invalidated after exit
    
    // Verify profiling entry was created
    pico_rtos_profile_entry_t entry;
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    assert(entry.function_id == TEST_FUNCTION_ID_1);
    assert(entry.call_count == 1);
    assert(entry.total_time_us > 0);
    assert(entry.avg_time_us > 0);
    assert(entry.min_time_us > 0);
    assert(entry.max_time_us > 0);
    assert(entry.active);
    
    printf("✓ Basic profiling tests passed\n");
}

static void test_multiple_calls(void) {
    printf("Testing multiple function calls...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    const int num_calls = 10;
    
    // Profile multiple calls to the same function
    for (int i = 0; i < num_calls; i++) {
        pico_rtos_profile_context_t ctx;
        
        assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "test_function_1", &ctx));
        test_function_fast();
        assert(pico_rtos_profiler_function_exit(&ctx));
    }
    
    // Verify statistics
    pico_rtos_profile_entry_t entry;
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    assert(entry.call_count == num_calls);
    assert(entry.total_time_us > 0);
    assert(entry.avg_time_us == entry.total_time_us / num_calls);
    assert(entry.min_time_us <= entry.max_time_us);
    
    printf("✓ Multiple calls tests passed\n");
}

static void test_multiple_functions(void) {
    printf("Testing multiple functions...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    // Profile different functions
    pico_rtos_profile_context_t ctx1, ctx2, ctx3;
    
    // Function 1 - fast
    assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "fast_function", &ctx1));
    test_function_fast();
    assert(pico_rtos_profiler_function_exit(&ctx1));
    
    // Function 2 - slow
    assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_2, "slow_function", &ctx2));
    test_function_slow();
    assert(pico_rtos_profiler_function_exit(&ctx2));
    
    // Function 3 - variable
    assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_3, "variable_function", &ctx3));
    test_function_variable(500);
    assert(pico_rtos_profiler_function_exit(&ctx3));
    
    // Verify all entries exist
    pico_rtos_profile_entry_t entry1, entry2, entry3;
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry1));
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_2, &entry2));
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_3, &entry3));
    
    // Verify function names
    assert(strcmp(entry1.function_name, "fast_function") == 0);
    assert(strcmp(entry2.function_name, "slow_function") == 0);
    assert(strcmp(entry3.function_name, "variable_function") == 0);
    
    // Verify slow function takes more time than fast function
    assert(entry2.avg_time_us > entry1.avg_time_us);
    
    printf("✓ Multiple functions tests passed\n");
}

// =============================================================================
// MANUAL TIMING TESTS
// =============================================================================

static void test_manual_timing(void) {
    printf("Testing manual timing recording...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    const uint32_t test_time_us = 1000;
    
    // Record manual timing
    assert(pico_rtos_profiler_record_time(TEST_FUNCTION_ID_1, "manual_function", test_time_us));
    
    // Verify entry
    pico_rtos_profile_entry_t entry;
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    assert(entry.call_count == 1);
    assert(entry.total_time_us == test_time_us);
    assert(entry.avg_time_us == test_time_us);
    assert(entry.min_time_us == test_time_us);
    assert(entry.max_time_us == test_time_us);
    
    // Record another timing for the same function
    const uint32_t test_time_us_2 = 2000;
    assert(pico_rtos_profiler_record_time(TEST_FUNCTION_ID_1, "manual_function", test_time_us_2));
    
    // Verify updated statistics
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    assert(entry.call_count == 2);
    assert(entry.total_time_us == test_time_us + test_time_us_2);
    assert(entry.avg_time_us == (test_time_us + test_time_us_2) / 2);
    assert(entry.min_time_us == test_time_us);
    assert(entry.max_time_us == test_time_us_2);
    
    printf("✓ Manual timing tests passed\n");
}

// =============================================================================
// DATA RETRIEVAL TESTS
// =============================================================================

static void test_get_all_entries(void) {
    printf("Testing get all entries...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    // Create several profiling entries
    pico_rtos_profile_context_t ctx;
    
    for (int i = 0; i < 5; i++) {
        uint32_t function_id = 0x1000 + i;
        char function_name[32];
        snprintf(function_name, sizeof(function_name), "function_%d", i);
        
        assert(pico_rtos_profiler_function_enter(function_id, function_name, &ctx));
        test_function_fast();
        assert(pico_rtos_profiler_function_exit(&ctx));
    }
    
    // Get all entries
    pico_rtos_profile_entry_t entries[TEST_MAX_ENTRIES];
    uint32_t count = pico_rtos_profiler_get_all_entries(entries, TEST_MAX_ENTRIES);
    
    assert(count == 5);
    
    // Verify entries
    for (uint32_t i = 0; i < count; i++) {
        assert(entries[i].active);
        assert(entries[i].call_count == 1);
        assert(entries[i].total_time_us > 0);
    }
    
    printf("✓ Get all entries tests passed\n");
}

static void test_profiling_statistics(void) {
    printf("Testing profiling statistics...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    // Create some profiling data
    pico_rtos_profile_context_t ctx;
    
    // Fast function - multiple calls
    for (int i = 0; i < 3; i++) {
        assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "fast_function", &ctx));
        test_function_fast();
        assert(pico_rtos_profiler_function_exit(&ctx));
    }
    
    // Slow function - single call
    assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_2, "slow_function", &ctx));
    test_function_slow();
    assert(pico_rtos_profiler_function_exit(&ctx));
    
    // Get statistics
    pico_rtos_profiling_stats_t stats;
    assert(pico_rtos_profiler_get_stats(&stats));
    
    // Verify statistics
    assert(stats.total_functions == 2);
    assert(stats.total_calls == 4); // 3 + 1
    assert(stats.total_execution_time_us > 0);
    assert(stats.total_profiling_time_us > 0);
    assert(stats.profiling_overhead_percent >= 0.0f);
    assert(stats.slowest_function_id == TEST_FUNCTION_ID_2); // Slow function should be slowest
    assert(stats.most_called_function_id == TEST_FUNCTION_ID_1); // Fast function called most
    
    printf("✓ Profiling statistics tests passed\n");
}

static void test_sorting_functions(void) {
    printf("Testing function sorting...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    // Create functions with different characteristics
    pico_rtos_profile_context_t ctx;
    
    // Fast function - many calls
    for (int i = 0; i < 10; i++) {
        assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "fast_function", &ctx));
        test_function_fast();
        assert(pico_rtos_profiler_function_exit(&ctx));
    }
    
    // Slow function - few calls
    for (int i = 0; i < 2; i++) {
        assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_2, "slow_function", &ctx));
        test_function_slow();
        assert(pico_rtos_profiler_function_exit(&ctx));
    }
    
    // Medium function - medium calls
    for (int i = 0; i < 5; i++) {
        assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_3, "medium_function", &ctx));
        test_function_variable(100);
        assert(pico_rtos_profiler_function_exit(&ctx));
    }
    
    // Test slowest functions
    pico_rtos_profile_entry_t slowest[TEST_MAX_ENTRIES];
    uint32_t slowest_count = pico_rtos_profiler_get_slowest_functions(slowest, TEST_MAX_ENTRIES);
    
    assert(slowest_count == 3);
    // Slowest should be first (slow_function should have highest average time)
    assert(slowest[0].function_id == TEST_FUNCTION_ID_2);
    
    // Test most called functions
    pico_rtos_profile_entry_t most_called[TEST_MAX_ENTRIES];
    uint32_t most_called_count = pico_rtos_profiler_get_most_called_functions(most_called, TEST_MAX_ENTRIES);
    
    assert(most_called_count == 3);
    // Most called should be first (fast_function has 10 calls)
    assert(most_called[0].function_id == TEST_FUNCTION_ID_1);
    assert(most_called[0].call_count == 10);
    
    printf("✓ Function sorting tests passed\n");
}

// =============================================================================
// RESET AND CLEANUP TESTS
// =============================================================================

static void test_reset_functionality(void) {
    printf("Testing reset functionality...\n");
    
    // Create some profiling data
    pico_rtos_profile_context_t ctx;
    assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "test_function", &ctx));
    test_function_fast();
    assert(pico_rtos_profiler_function_exit(&ctx));
    
    // Verify data exists
    pico_rtos_profile_entry_t entry;
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    assert(entry.call_count == 1);
    
    // Reset specific function
    assert(pico_rtos_profiler_reset_function(TEST_FUNCTION_ID_1));
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    assert(entry.call_count == 0);
    assert(entry.total_time_us == 0);
    
    // Add data again
    assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "test_function", &ctx));
    test_function_fast();
    assert(pico_rtos_profiler_function_exit(&ctx));
    
    // Reset all
    pico_rtos_profiler_reset();
    
    // Verify all data is cleared
    assert(!pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    
    pico_rtos_profiling_stats_t stats;
    assert(pico_rtos_profiler_get_stats(&stats));
    assert(stats.total_functions == 0);
    assert(stats.total_calls == 0);
    
    printf("✓ Reset functionality tests passed\n");
}

// =============================================================================
// UTILITY FUNCTION TESTS
// =============================================================================

static void test_utility_functions(void) {
    printf("Testing utility functions...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    // Create a profiling entry
    pico_rtos_profile_context_t ctx;
    assert(pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "test_function", &ctx));
    test_function_fast();
    assert(pico_rtos_profiler_function_exit(&ctx));
    
    // Test format entry
    pico_rtos_profile_entry_t entry;
    assert(pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    
    char buffer[512];
    uint32_t chars_written = pico_rtos_profiler_format_entry(&entry, buffer, sizeof(buffer));
    assert(chars_written > 0);
    assert(chars_written < sizeof(buffer));
    assert(strstr(buffer, "test_function") != NULL);
    assert(strstr(buffer, "Calls: 1") != NULL);
    
    // Test with NULL parameters
    assert(pico_rtos_profiler_format_entry(NULL, buffer, sizeof(buffer)) == 0);
    assert(pico_rtos_profiler_format_entry(&entry, NULL, sizeof(buffer)) == 0);
    assert(pico_rtos_profiler_format_entry(&entry, buffer, 0) == 0);
    
    // Test print functions (these just call printf, so we'll just call them)
    pico_rtos_profiler_print_entry(&entry);
    
    pico_rtos_profiling_stats_t stats;
    assert(pico_rtos_profiler_get_stats(&stats));
    pico_rtos_profiler_print_stats(&stats);
    
    pico_rtos_profiler_print_all_entries();
    
    printf("✓ Utility function tests passed\n");
}

// =============================================================================
// PERFORMANCE IMPACT TESTS
// =============================================================================

static void test_profiling_overhead(void) {
    printf("Testing profiling overhead...\n");
    
    // Reset profiler
    pico_rtos_profiler_reset();
    
    const int num_iterations = 1000;
    
    // Measure time without profiling
    uint64_t start_time = time_us_64();
    for (int i = 0; i < num_iterations; i++) {
        test_function_fast();
    }
    uint64_t time_without_profiling = time_us_64() - start_time;
    
    // Measure time with profiling
    start_time = time_us_64();
    for (int i = 0; i < num_iterations; i++) {
        pico_rtos_profile_context_t ctx;
        pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "test_function", &ctx);
        test_function_fast();
        pico_rtos_profiler_function_exit(&ctx);
    }
    uint64_t time_with_profiling = time_us_64() - start_time;
    
    // Calculate overhead
    uint64_t overhead = time_with_profiling - time_without_profiling;
    float overhead_per_call = (float)overhead / num_iterations;
    float overhead_percentage = (float)overhead / time_without_profiling * 100.0f;
    
    printf("Performance results:\n");
    printf("  Without profiling: %llu us total\n", time_without_profiling);
    printf("  With profiling: %llu us total\n", time_with_profiling);
    printf("  Overhead: %llu us total (%.2f us per call)\n", overhead, overhead_per_call);
    printf("  Overhead percentage: %.2f%%\n", overhead_percentage);
    
    // Verify overhead is reasonable (less than 50% for fast functions)
    assert(overhead_percentage < 50.0f);
    assert(overhead_per_call < 50.0f); // Less than 50us per call
    
    // Get profiling statistics
    pico_rtos_profiling_stats_t stats;
    assert(pico_rtos_profiler_get_stats(&stats));
    printf("  Profiler reported overhead: %.2f%%\n", stats.profiling_overhead_percent);
    
    printf("✓ Profiling overhead tests passed\n");
}

// =============================================================================
// DISABLED PROFILING TESTS
// =============================================================================

static void test_disabled_profiling(void) {
    printf("Testing disabled profiling...\n");
    
    // Disable profiling
    pico_rtos_profiler_enable(false);
    assert(!pico_rtos_profiler_is_enabled());
    
    // Try to profile (should fail gracefully)
    pico_rtos_profile_context_t ctx;
    assert(!pico_rtos_profiler_function_enter(TEST_FUNCTION_ID_1, "test_function", &ctx));
    assert(!ctx.valid);
    
    test_function_fast();
    
    assert(!pico_rtos_profiler_function_exit(&ctx));
    
    // Verify no data was recorded
    pico_rtos_profile_entry_t entry;
    assert(!pico_rtos_profiler_get_entry(TEST_FUNCTION_ID_1, &entry));
    
    // Re-enable profiling
    pico_rtos_profiler_enable(true);
    assert(pico_rtos_profiler_is_enabled());
    
    printf("✓ Disabled profiling tests passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void) {
    printf("Starting Pico-RTOS Profiler Tests\n");
    printf("==================================\n");
    
    // Initialize RTOS
    assert(pico_rtos_init());
    
    // Run tests
    test_profiler_initialization();
    test_profiler_enable_disable();
    test_basic_profiling();
    test_multiple_calls();
    test_multiple_functions();
    test_manual_timing();
    test_get_all_entries();
    test_profiling_statistics();
    test_sorting_functions();
    test_reset_functionality();
    test_utility_functions();
    test_profiling_overhead();
    test_disabled_profiling();
    
    // Clean up
    pico_rtos_profiler_deinit();
    
    printf("\n==================================\n");
    printf("All Profiler Tests Passed! ✓\n");
    printf("==================================\n");
    
    return 0;
}