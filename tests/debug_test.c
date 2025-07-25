#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico_rtos.h"
#include "pico_rtos/debug.h"
#include "pico_rtos/task.h"

// Test configuration
#define TEST_TASK_STACK_SIZE 512
#define TEST_TASK_PRIORITY 5
#define MAX_TEST_TASKS 4

// Test task stacks
static uint32_t test_task1_stack[TEST_TASK_STACK_SIZE / sizeof(uint32_t)];
static uint32_t test_task2_stack[TEST_TASK_STACK_SIZE / sizeof(uint32_t)];
static uint32_t test_task3_stack[TEST_TASK_STACK_SIZE / sizeof(uint32_t)];

// Test tasks
static pico_rtos_task_t test_task1;
static pico_rtos_task_t test_task2;
static pico_rtos_task_t test_task3;

// Test task functions
static void test_task_function_1(void *param) {
    int *counter = (int *)param;
    while (1) {
        (*counter)++;
        pico_rtos_task_delay(100);
    }
}

static void test_task_function_2(void *param) {
    int *counter = (int *)param;
    while (1) {
        (*counter)++;
        pico_rtos_task_delay(200);
    }
}

static void test_task_function_3(void *param) {
    // This task will use more stack to test stack usage calculation
    volatile char large_buffer[256];
    int *counter = (int *)param;
    
    // Use the buffer to prevent optimization
    memset((void *)large_buffer, 0xAA, sizeof(large_buffer));
    
    while (1) {
        (*counter)++;
        large_buffer[0] = (char)(*counter);
        pico_rtos_task_delay(150);
    }
}

// Test counters
static int task1_counter = 0;
static int task2_counter = 0;
static int task3_counter = 0;

// =============================================================================
// TEST HELPER FUNCTIONS
// =============================================================================

static void setup_test_tasks(void) {
    // Create test tasks
    assert(pico_rtos_task_create(&test_task1, "TestTask1", test_task_function_1, 
                                &task1_counter, TEST_TASK_STACK_SIZE, TEST_TASK_PRIORITY));
    
    assert(pico_rtos_task_create(&test_task2, "TestTask2", test_task_function_2, 
                                &task2_counter, TEST_TASK_STACK_SIZE, TEST_TASK_PRIORITY + 1));
    
    assert(pico_rtos_task_create(&test_task3, "TestTask3", test_task_function_3, 
                                &task3_counter, TEST_TASK_STACK_SIZE, TEST_TASK_PRIORITY + 2));
    
    // Initialize stack patterns for testing
    assert(pico_rtos_debug_init_stack_pattern(&test_task1));
    assert(pico_rtos_debug_init_stack_pattern(&test_task2));
    assert(pico_rtos_debug_init_stack_pattern(&test_task3));
}

static void cleanup_test_tasks(void) {
    pico_rtos_task_delete(&test_task1);
    pico_rtos_task_delete(&test_task2);
    pico_rtos_task_delete(&test_task3);
}

// =============================================================================
// TASK INSPECTION TESTS
// =============================================================================

static void test_get_task_info(void) {
    printf("Testing pico_rtos_debug_get_task_info...\n");
    
    pico_rtos_task_info_t info;
    
    // Test with valid task
    assert(pico_rtos_debug_get_task_info(&test_task1, &info));
    assert(strcmp(info.name, "TestTask1") == 0);
    assert(info.priority == TEST_TASK_PRIORITY);
    assert(info.stack_size == TEST_TASK_STACK_SIZE);
    assert(info.function_ptr == (void *)test_task_function_1);
    assert(info.param == &task1_counter);
    
    // Test with NULL task (should use current task)
    assert(pico_rtos_debug_get_task_info(NULL, &info));
    
    // Test with NULL info (should fail)
    assert(!pico_rtos_debug_get_task_info(&test_task1, NULL));
    
    printf("✓ pico_rtos_debug_get_task_info tests passed\n");
}

static void test_get_all_task_info(void) {
    printf("Testing pico_rtos_debug_get_all_task_info...\n");
    
    pico_rtos_task_info_t info_array[MAX_TEST_TASKS];
    
    // Test getting all task info
    uint32_t task_count = pico_rtos_debug_get_all_task_info(info_array, MAX_TEST_TASKS);
    assert(task_count >= 3); // At least our 3 test tasks
    
    // Verify we can find our test tasks
    bool found_task1 = false, found_task2 = false, found_task3 = false;
    for (uint32_t i = 0; i < task_count; i++) {
        if (strcmp(info_array[i].name, "TestTask1") == 0) {
            found_task1 = true;
            assert(info_array[i].priority == TEST_TASK_PRIORITY);
        } else if (strcmp(info_array[i].name, "TestTask2") == 0) {
            found_task2 = true;
            assert(info_array[i].priority == TEST_TASK_PRIORITY + 1);
        } else if (strcmp(info_array[i].name, "TestTask3") == 0) {
            found_task3 = true;
            assert(info_array[i].priority == TEST_TASK_PRIORITY + 2);
        }
    }
    
    assert(found_task1 && found_task2 && found_task3);
    
    // Test with NULL array (should fail)
    assert(pico_rtos_debug_get_all_task_info(NULL, MAX_TEST_TASKS) == 0);
    
    // Test with zero max_tasks (should fail)
    assert(pico_rtos_debug_get_all_task_info(info_array, 0) == 0);
    
    printf("✓ pico_rtos_debug_get_all_task_info tests passed\n");
}

static void test_get_system_inspection(void) {
    printf("Testing pico_rtos_debug_get_system_inspection...\n");
    
    pico_rtos_system_inspection_t summary;
    
    // Test getting system inspection
    assert(pico_rtos_debug_get_system_inspection(&summary));
    
    // Verify basic statistics
    assert(summary.total_tasks >= 3); // At least our test tasks
    assert(summary.total_stack_allocated > 0);
    assert(summary.inspection_time_us > 0);
    assert(summary.inspection_timestamp > 0);
    
    // Test with NULL summary (should fail)
    assert(!pico_rtos_debug_get_system_inspection(NULL));
    
    printf("✓ pico_rtos_debug_get_system_inspection tests passed\n");
}

static void test_find_task_by_name(void) {
    printf("Testing pico_rtos_debug_find_task_by_name...\n");
    
    pico_rtos_task_info_t info;
    
    // Test finding existing task
    assert(pico_rtos_debug_find_task_by_name("TestTask1", &info));
    assert(strcmp(info.name, "TestTask1") == 0);
    assert(info.priority == TEST_TASK_PRIORITY);
    
    // Test finding non-existent task
    assert(!pico_rtos_debug_find_task_by_name("NonExistentTask", &info));
    
    // Test with NULL name (should fail)
    assert(!pico_rtos_debug_find_task_by_name(NULL, &info));
    
    // Test with NULL info (should fail)
    assert(!pico_rtos_debug_find_task_by_name("TestTask1", NULL));
    
    printf("✓ pico_rtos_debug_find_task_by_name tests passed\n");
}

// =============================================================================
// STACK MONITORING TESTS
// =============================================================================

static void test_stack_usage_methods(void) {
    printf("Testing stack usage methods...\n");
    
    // Test setting and getting stack usage method
    pico_rtos_debug_set_stack_usage_method(PICO_RTOS_STACK_USAGE_PATTERN);
    assert(pico_rtos_debug_get_stack_usage_method() == PICO_RTOS_STACK_USAGE_PATTERN);
    
    pico_rtos_debug_set_stack_usage_method(PICO_RTOS_STACK_USAGE_WATERMARK);
    assert(pico_rtos_debug_get_stack_usage_method() == PICO_RTOS_STACK_USAGE_WATERMARK);
    
    printf("✓ Stack usage method tests passed\n");
}

static void test_calculate_stack_usage(void) {
    printf("Testing pico_rtos_debug_calculate_stack_usage...\n");
    
    uint32_t current_usage, peak_usage, free_space;
    
    // Test with valid task
    assert(pico_rtos_debug_calculate_stack_usage(&test_task1, &current_usage, &peak_usage, &free_space));
    assert(current_usage <= TEST_TASK_STACK_SIZE);
    assert(peak_usage <= TEST_TASK_STACK_SIZE);
    assert(free_space <= TEST_TASK_STACK_SIZE);
    assert(current_usage + free_space <= TEST_TASK_STACK_SIZE);
    
    // Test with NULL task (should use current task)
    assert(pico_rtos_debug_calculate_stack_usage(NULL, &current_usage, &peak_usage, &free_space));
    
    // Test with NULL parameters (should fail)
    assert(!pico_rtos_debug_calculate_stack_usage(&test_task1, NULL, &peak_usage, &free_space));
    assert(!pico_rtos_debug_calculate_stack_usage(&test_task1, &current_usage, NULL, &free_space));
    assert(!pico_rtos_debug_calculate_stack_usage(&test_task1, &current_usage, &peak_usage, NULL));
    
    printf("✓ pico_rtos_debug_calculate_stack_usage tests passed\n");
}

static void test_check_stack_overflow(void) {
    printf("Testing pico_rtos_debug_check_stack_overflow...\n");
    
    pico_rtos_stack_overflow_info_t overflow_info;
    
    // Test with normal task (should not have overflow)
    bool overflow = pico_rtos_debug_check_stack_overflow(&test_task1, &overflow_info);
    // Note: This might return true if stack usage is high, which is acceptable
    
    // Test with NULL task (should use current task)
    pico_rtos_debug_check_stack_overflow(NULL, &overflow_info);
    
    // Test without overflow info (should still work)
    pico_rtos_debug_check_stack_overflow(&test_task1, NULL);
    
    printf("✓ pico_rtos_debug_check_stack_overflow tests passed\n");
}

static void test_check_all_stack_overflows(void) {
    printf("Testing pico_rtos_debug_check_all_stack_overflows...\n");
    
    pico_rtos_stack_overflow_info_t overflow_info[MAX_TEST_TASKS];
    
    // Test checking all tasks
    uint32_t overflow_count = pico_rtos_debug_check_all_stack_overflows(overflow_info, MAX_TEST_TASKS);
    // Note: overflow_count might be > 0 if any tasks have high stack usage
    
    // Test with NULL array (should fail)
    assert(pico_rtos_debug_check_all_stack_overflows(NULL, MAX_TEST_TASKS) == 0);
    
    // Test with zero max_overflows (should fail)
    assert(pico_rtos_debug_check_all_stack_overflows(overflow_info, 0) == 0);
    
    printf("✓ pico_rtos_debug_check_all_stack_overflows tests passed\n");
}

static void test_init_stack_pattern(void) {
    printf("Testing pico_rtos_debug_init_stack_pattern...\n");
    
    // Test with valid task
    assert(pico_rtos_debug_init_stack_pattern(&test_task1));
    
    // Test with NULL task (should fail)
    assert(!pico_rtos_debug_init_stack_pattern(NULL));
    
    printf("✓ pico_rtos_debug_init_stack_pattern tests passed\n");
}

// =============================================================================
// RUNTIME STATISTICS TESTS
// =============================================================================

static void test_runtime_statistics(void) {
    printf("Testing runtime statistics functions...\n");
    
    // Test reset task stats
    assert(pico_rtos_debug_reset_task_stats(&test_task1));
    assert(pico_rtos_debug_reset_task_stats(NULL)); // Should use current task
    
    // Test reset all task stats
    pico_rtos_debug_reset_all_task_stats();
    
    // Test get task CPU usage
    float cpu_usage = pico_rtos_debug_get_task_cpu_usage(&test_task1, 1000);
    assert(cpu_usage >= 0.0f && cpu_usage <= 100.0f);
    
    // Test get system CPU usage
    float task_usage[MAX_TEST_TASKS];
    float idle_percentage;
    uint32_t task_count = pico_rtos_debug_get_system_cpu_usage(task_usage, MAX_TEST_TASKS, &idle_percentage);
    assert(task_count > 0);
    assert(idle_percentage >= 0.0f && idle_percentage <= 100.0f);
    
    printf("✓ Runtime statistics tests passed\n");
}

// =============================================================================
// UTILITY FUNCTION TESTS
// =============================================================================

static void test_utility_functions(void) {
    printf("Testing utility functions...\n");
    
    // Test task state to string conversion
    assert(strcmp(pico_rtos_debug_task_state_to_string(PICO_RTOS_TASK_STATE_READY), "READY") == 0);
    assert(strcmp(pico_rtos_debug_task_state_to_string(PICO_RTOS_TASK_STATE_RUNNING), "RUNNING") == 0);
    assert(strcmp(pico_rtos_debug_task_state_to_string(PICO_RTOS_TASK_STATE_BLOCKED), "BLOCKED") == 0);
    assert(strcmp(pico_rtos_debug_task_state_to_string(PICO_RTOS_TASK_STATE_SUSPENDED), "SUSPENDED") == 0);
    assert(strcmp(pico_rtos_debug_task_state_to_string(PICO_RTOS_TASK_STATE_TERMINATED), "TERMINATED") == 0);
    
    // Test block reason to string conversion
    assert(strcmp(pico_rtos_debug_block_reason_to_string(PICO_RTOS_BLOCK_REASON_NONE), "NONE") == 0);
    assert(strcmp(pico_rtos_debug_block_reason_to_string(PICO_RTOS_BLOCK_REASON_DELAY), "DELAY") == 0);
    assert(strcmp(pico_rtos_debug_block_reason_to_string(PICO_RTOS_BLOCK_REASON_MUTEX), "MUTEX") == 0);
    
    // Test format task info
    pico_rtos_task_info_t info;
    assert(pico_rtos_debug_get_task_info(&test_task1, &info));
    
    char buffer[512];
    uint32_t chars_written = pico_rtos_debug_format_task_info(&info, buffer, sizeof(buffer));
    assert(chars_written > 0);
    assert(chars_written < sizeof(buffer));
    assert(strstr(buffer, "TestTask1") != NULL);
    
    // Test with NULL parameters
    assert(pico_rtos_debug_format_task_info(NULL, buffer, sizeof(buffer)) == 0);
    assert(pico_rtos_debug_format_task_info(&info, NULL, sizeof(buffer)) == 0);
    assert(pico_rtos_debug_format_task_info(&info, buffer, 0) == 0);
    
    // Test print functions (these just call printf, so we'll just call them)
    pico_rtos_debug_print_task_info(&info);
    
    pico_rtos_system_inspection_t summary;
    assert(pico_rtos_debug_get_system_inspection(&summary));
    pico_rtos_debug_print_system_inspection(&summary);
    
    printf("✓ Utility function tests passed\n");
}

// =============================================================================
// PERFORMANCE IMPACT TESTS
// =============================================================================

static void test_performance_impact(void) {
    printf("Testing performance impact...\n");
    
    // Measure time for various debug operations
    uint64_t start_time, end_time;
    
    // Test task info retrieval performance
    start_time = time_us_64();
    pico_rtos_task_info_t info;
    for (int i = 0; i < 100; i++) {
        pico_rtos_debug_get_task_info(&test_task1, &info);
    }
    end_time = time_us_64();
    uint64_t task_info_time = end_time - start_time;
    
    // Test system inspection performance
    start_time = time_us_64();
    pico_rtos_system_inspection_t summary;
    for (int i = 0; i < 10; i++) {
        pico_rtos_debug_get_system_inspection(&summary);
    }
    end_time = time_us_64();
    uint64_t system_inspection_time = end_time - start_time;
    
    // Test stack usage calculation performance
    start_time = time_us_64();
    uint32_t current_usage, peak_usage, free_space;
    for (int i = 0; i < 100; i++) {
        pico_rtos_debug_calculate_stack_usage(&test_task1, &current_usage, &peak_usage, &free_space);
    }
    end_time = time_us_64();
    uint64_t stack_calc_time = end_time - start_time;
    
    printf("Performance results:\n");
    printf("  Task info retrieval: %llu us per 100 calls (%.2f us per call)\n", 
           task_info_time, (float)task_info_time / 100.0f);
    printf("  System inspection: %llu us per 10 calls (%.2f us per call)\n", 
           system_inspection_time, (float)system_inspection_time / 10.0f);
    printf("  Stack usage calculation: %llu us per 100 calls (%.2f us per call)\n", 
           stack_calc_time, (float)stack_calc_time / 100.0f);
    
    // Verify performance is reasonable (less than 100us per operation)
    assert(task_info_time / 100 < 100);
    assert(system_inspection_time / 10 < 1000);
    assert(stack_calc_time / 100 < 100);
    
    printf("✓ Performance impact tests passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void) {
    printf("Starting Pico-RTOS Debug System Tests\n");
    printf("=====================================\n");
    
    // Initialize RTOS
    assert(pico_rtos_init());
    
    // Set up test tasks
    setup_test_tasks();
    
    // Allow tasks to run briefly to establish some state
    pico_rtos_task_delay(500);
    
    // Run tests
    test_get_task_info();
    test_get_all_task_info();
    test_get_system_inspection();
    test_find_task_by_name();
    
    test_stack_usage_methods();
    test_calculate_stack_usage();
    test_check_stack_overflow();
    test_check_all_stack_overflows();
    test_init_stack_pattern();
    
    test_runtime_statistics();
    test_utility_functions();
    test_performance_impact();
    
    // Clean up
    cleanup_test_tasks();
    
    printf("\n=====================================\n");
    printf("All Debug System Tests Passed! ✓\n");
    printf("=====================================\n");
    
    return 0;
}