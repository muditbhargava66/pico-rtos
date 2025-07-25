/**
 * @file performance_regression_test.c
 * @brief Performance regression test suite for Pico-RTOS v0.3.0
 * 
 * Automated performance benchmarks for:
 * - Context switch timing validation with new features enabled
 * - Memory allocation performance tests
 * - Real-time compliance validation tests
 * - Critical path performance monitoring
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico_rtos.h"

// Include available headers based on configuration
#ifdef PICO_RTOS_ENABLE_EVENT_GROUPS
#include "pico_rtos/event_group.h"
#endif

#ifdef PICO_RTOS_ENABLE_STREAM_BUFFERS
#include "pico_rtos/stream_buffer.h"
#endif

#ifdef PICO_RTOS_ENABLE_MEMORY_POOLS
#include "pico_rtos/memory_pool.h"
#endif

#ifdef PICO_RTOS_ENABLE_MULTI_CORE
#include "pico_rtos/smp.h"
#endif

#ifdef PICO_RTOS_ENABLE_EXECUTION_PROFILING
#include "pico_rtos/profiler.h"
#endif

// Mock definitions for disabled features
#ifndef PICO_RTOS_ENABLE_EVENT_GROUPS
typedef struct { int dummy; } pico_rtos_event_group_t;
typedef struct { int dummy; } pico_rtos_event_group_stats_t;
static inline bool pico_rtos_event_group_init(pico_rtos_event_group_t *group) { (void)group; return false; }
static inline bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *group, uint32_t bits) { (void)group; (void)bits; return false; }
static inline bool pico_rtos_event_group_clear_bits(pico_rtos_event_group_t *group, uint32_t bits) { (void)group; (void)bits; return false; }
static inline bool pico_rtos_event_group_get_stats(pico_rtos_event_group_t *group, pico_rtos_event_group_stats_t *stats) { (void)group; (void)stats; return false; }
#endif

#ifndef PICO_RTOS_ENABLE_STREAM_BUFFERS
typedef struct { int dummy; } pico_rtos_stream_buffer_t;
typedef struct { int dummy; } pico_rtos_stream_buffer_stats_t;
static inline bool pico_rtos_stream_buffer_init(pico_rtos_stream_buffer_t *buffer, uint8_t *mem, size_t size) { (void)buffer; (void)mem; (void)size; return false; }
static inline uint32_t pico_rtos_stream_buffer_send(pico_rtos_stream_buffer_t *buffer, const void *data, size_t size, uint32_t timeout) { (void)buffer; (void)data; (void)size; (void)timeout; return 0; }
static inline uint32_t pico_rtos_stream_buffer_receive(pico_rtos_stream_buffer_t *buffer, void *data, size_t size, uint32_t timeout) { (void)buffer; (void)data; (void)size; (void)timeout; return 0; }
static inline bool pico_rtos_stream_buffer_get_stats(pico_rtos_stream_buffer_t *buffer, pico_rtos_stream_buffer_stats_t *stats) { (void)buffer; (void)stats; return false; }
#endif

#ifndef PICO_RTOS_ENABLE_MEMORY_POOLS
typedef struct { int dummy; } pico_rtos_memory_pool_t;
static inline bool pico_rtos_memory_pool_init(pico_rtos_memory_pool_t *pool, uint8_t *mem, size_t block_size, size_t count) { (void)pool; (void)mem; (void)block_size; (void)count; return false; }
static inline void* pico_rtos_memory_pool_alloc(pico_rtos_memory_pool_t *pool, uint32_t timeout) { (void)pool; (void)timeout; return NULL; }
static inline bool pico_rtos_memory_pool_free(pico_rtos_memory_pool_t *pool, void *ptr) { (void)pool; (void)ptr; return false; }
#endif

#ifndef PICO_RTOS_ENABLE_EXECUTION_PROFILING
typedef struct { int dummy; } pico_rtos_profiler_stats_t;
static inline bool pico_rtos_profiler_init(void) { return false; }
static inline bool pico_rtos_profiler_get_stats(pico_rtos_profiler_stats_t *stats) { (void)stats; return false; }
#endif

// Performance test configuration
#define PERF_TEST_ITERATIONS 1000
#define CONTEXT_SWITCH_TEST_TASKS 4
#define MEMORY_ALLOC_TEST_BLOCKS 100
#define REAL_TIME_TEST_DURATION_MS 5000
#define PERFORMANCE_TASK_STACK_SIZE 512

// Performance thresholds (in microseconds)
#define MAX_CONTEXT_SWITCH_TIME_US 50
#define MAX_MEMORY_ALLOC_TIME_US 20
#define MAX_EVENT_SET_TIME_US 10
#define MAX_STREAM_SEND_TIME_US 30
#define MAX_INTERRUPT_LATENCY_US 25

// Test objects
static pico_rtos_event_group_t perf_event_group;
static pico_rtos_stream_buffer_t perf_stream_buffer;
static uint8_t perf_stream_memory[1024];
static pico_rtos_memory_pool_t perf_memory_pool;
static uint8_t perf_pool_memory[MEMORY_ALLOC_TEST_BLOCKS * 64 + 128];

// Performance measurement data
typedef struct {
    uint32_t min_time_us;
    uint32_t max_time_us;
    uint32_t avg_time_us;
    uint32_t total_time_us;
    uint32_t sample_count;
    uint32_t violations;
    uint32_t threshold_us;
} performance_metrics_t;

// Test state
static volatile bool test_passed = true;
static volatile int test_count = 0;
static volatile int test_failures = 0;

// Performance data
static performance_metrics_t context_switch_metrics;
static performance_metrics_t memory_alloc_metrics;
static performance_metrics_t event_operation_metrics;
static performance_metrics_t stream_operation_metrics;
static performance_metrics_t interrupt_latency_metrics;

// Test helper macro
#define TEST_ASSERT(condition, message) \
    do { \
        test_count++; \
        if (!(condition)) { \
            printf("FAIL: %s (line %d): %s\n", __func__, __LINE__, message); \
            test_failures++; \
            test_passed = false; \
        } else { \
            printf("PASS: %s\n", message); \
        } \
    } while(0)

// =============================================================================
// PERFORMANCE MEASUREMENT UTILITIES
// =============================================================================

static void init_performance_metrics(performance_metrics_t *metrics, uint32_t threshold_us) {
    memset(metrics, 0, sizeof(performance_metrics_t));
    metrics->min_time_us = UINT32_MAX;
    metrics->threshold_us = threshold_us;
}

static void update_performance_metrics(performance_metrics_t *metrics, uint32_t time_us) {
    if (time_us < metrics->min_time_us) {
        metrics->min_time_us = time_us;
    }
    if (time_us > metrics->max_time_us) {
        metrics->max_time_us = time_us;
    }
    
    metrics->total_time_us += time_us;
    metrics->sample_count++;
    metrics->avg_time_us = metrics->total_time_us / metrics->sample_count;
    
    if (time_us > metrics->threshold_us) {
        metrics->violations++;
    }
}

static void print_performance_metrics(const char *test_name, const performance_metrics_t *metrics) {
    printf("\n%s Performance Metrics:\n", test_name);
    printf("  Samples: %lu\n", metrics->sample_count);
    printf("  Min: %lu μs\n", metrics->min_time_us);
    printf("  Max: %lu μs\n", metrics->max_time_us);
    printf("  Avg: %lu μs\n", metrics->avg_time_us);
    printf("  Threshold: %lu μs\n", metrics->threshold_us);
    printf("  Violations: %lu (%.1f%%)\n", metrics->violations,
           metrics->sample_count > 0 ? (100.0 * metrics->violations / metrics->sample_count) : 0.0);
}

static uint32_t get_time_us(void) {
    return time_us_32();
}

// =============================================================================
// CONTEXT SWITCH PERFORMANCE TESTS
// =============================================================================

typedef struct {
    uint32_t task_id;
    volatile bool ready_to_switch;
    volatile uint32_t switch_count;
    uint32_t start_time_us;
    uint32_t end_time_us;
} context_switch_task_data_t;

static context_switch_task_data_t cs_task_data[CONTEXT_SWITCH_TEST_TASKS];

static void context_switch_test_task(void *param) {
    context_switch_task_data_t *data = (context_switch_task_data_t *)param;
    
    for (uint32_t i = 0; i < PERF_TEST_ITERATIONS / CONTEXT_SWITCH_TEST_TASKS; i++) {
        // Signal ready for context switch measurement
        data->ready_to_switch = true;
        data->start_time_us = get_time_us();
        
        // Yield to trigger context switch
        pico_rtos_task_delay(1);
        
        data->end_time_us = get_time_us();
        data->switch_count++;
        
        // Small delay to prevent overwhelming the scheduler
        pico_rtos_task_delay(2);
    }
    
    pico_rtos_task_delete(NULL);
}

static void test_context_switch_performance(void) {
    printf("\n=== Testing Context Switch Performance ===\n");
    
    init_performance_metrics(&context_switch_metrics, MAX_CONTEXT_SWITCH_TIME_US);
    
    // Create context switch test tasks
    pico_rtos_task_t cs_tasks[CONTEXT_SWITCH_TEST_TASKS];
    uint32_t cs_stacks[CONTEXT_SWITCH_TEST_TASKS][PERFORMANCE_TASK_STACK_SIZE / sizeof(uint32_t)];
    
    // Initialize task data
    for (int i = 0; i < CONTEXT_SWITCH_TEST_TASKS; i++) {
        memset(&cs_task_data[i], 0, sizeof(context_switch_task_data_t));
        cs_task_data[i].task_id = i;
    }
    
    // Create tasks with different priorities
    for (int i = 0; i < CONTEXT_SWITCH_TEST_TASKS; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "cs_test_%d", i);
        
        bool result = pico_rtos_task_create(&cs_tasks[i], task_name,
                                           context_switch_test_task,
                                           &cs_task_data[i],
                                           sizeof(cs_stacks[i]),
                                           5 + i);
        TEST_ASSERT(result, "Context switch test task creation should succeed");
    }
    
    // Monitor context switch performance
    uint32_t monitor_start = get_time_us();
    uint32_t last_check = monitor_start;
    
    while (get_time_us() - monitor_start < 10000000) { // 10 seconds max
        bool all_done = true;
        uint32_t current_time = get_time_us();
        
        // Check if tasks are still running
        for (int i = 0; i < CONTEXT_SWITCH_TEST_TASKS; i++) {
            if (cs_task_data[i].switch_count < PERF_TEST_ITERATIONS / CONTEXT_SWITCH_TEST_TASKS) {
                all_done = false;
            }
            
            // Measure context switch time if task just switched
            if (cs_task_data[i].ready_to_switch && cs_task_data[i].end_time_us > cs_task_data[i].start_time_us) {
                uint32_t switch_time = cs_task_data[i].end_time_us - cs_task_data[i].start_time_us;
                update_performance_metrics(&context_switch_metrics, switch_time);
                cs_task_data[i].ready_to_switch = false;
            }
        }
        
        if (all_done) break;
        
        // Progress report every second
        if (current_time - last_check > 1000000) {
            uint32_t total_switches = 0;
            for (int i = 0; i < CONTEXT_SWITCH_TEST_TASKS; i++) {
                total_switches += cs_task_data[i].switch_count;
            }
            printf("Context switches completed: %lu/%d\n", total_switches, PERF_TEST_ITERATIONS);
            last_check = current_time;
        }
        
        pico_rtos_task_delay(10);
    }
    
    print_performance_metrics("Context Switch", &context_switch_metrics);
    
    // Validate performance
    TEST_ASSERT(context_switch_metrics.sample_count > 0, "Should have context switch measurements");
    TEST_ASSERT(context_switch_metrics.avg_time_us <= MAX_CONTEXT_SWITCH_TIME_US,
               "Average context switch time should be within threshold");
    TEST_ASSERT(context_switch_metrics.violations < (context_switch_metrics.sample_count / 10),
               "Context switch violations should be less than 10%");
}

// =============================================================================
// MEMORY ALLOCATION PERFORMANCE TESTS
// =============================================================================

static void test_memory_allocation_performance(void) {
    printf("\n=== Testing Memory Allocation Performance ===\n");
    
    init_performance_metrics(&memory_alloc_metrics, MAX_MEMORY_ALLOC_TIME_US);
    
    // Initialize memory pool for testing
    bool pool_result = pico_rtos_memory_pool_init(&perf_memory_pool,
                                                 perf_pool_memory,
                                                 64,
                                                 MEMORY_ALLOC_TEST_BLOCKS);
    TEST_ASSERT(pool_result, "Performance memory pool initialization should succeed");
    
    void *allocated_blocks[MEMORY_ALLOC_TEST_BLOCKS];
    
    // Test allocation performance
    for (int iteration = 0; iteration < 10; iteration++) {
        // Allocation phase
        for (int i = 0; i < MEMORY_ALLOC_TEST_BLOCKS; i++) {
            uint32_t start_time = get_time_us();
            allocated_blocks[i] = pico_rtos_memory_pool_alloc(&perf_memory_pool, 10);
            uint32_t end_time = get_time_us();
            
            if (allocated_blocks[i]) {
                uint32_t alloc_time = end_time - start_time;
                update_performance_metrics(&memory_alloc_metrics, alloc_time);
            }
        }
        
        // Deallocation phase
        for (int i = 0; i < MEMORY_ALLOC_TEST_BLOCKS; i++) {
            if (allocated_blocks[i]) {
                uint32_t start_time = get_time_us();
                pico_rtos_memory_pool_free(&perf_memory_pool, allocated_blocks[i]);
                uint32_t end_time = get_time_us();
                
                uint32_t free_time = end_time - start_time;
                update_performance_metrics(&memory_alloc_metrics, free_time);
                allocated_blocks[i] = NULL;
            }
        }
        
        // Small delay between iterations
        pico_rtos_task_delay(10);
    }
    
    print_performance_metrics("Memory Allocation", &memory_alloc_metrics);
    
    // Validate performance
    TEST_ASSERT(memory_alloc_metrics.sample_count > 0, "Should have memory allocation measurements");
    TEST_ASSERT(memory_alloc_metrics.avg_time_us <= MAX_MEMORY_ALLOC_TIME_US,
               "Average memory allocation time should be within threshold");
    TEST_ASSERT(memory_alloc_metrics.violations < (memory_alloc_metrics.sample_count / 20),
               "Memory allocation violations should be less than 5%");
}

// =============================================================================
// EVENT GROUP PERFORMANCE TESTS
// =============================================================================

static void test_event_group_performance(void) {
    printf("\n=== Testing Event Group Performance ===\n");
    
    init_performance_metrics(&event_operation_metrics, MAX_EVENT_SET_TIME_US);
    
    // Initialize event group
    bool event_result = pico_rtos_event_group_init(&perf_event_group);
    TEST_ASSERT(event_result, "Performance event group initialization should succeed");
    
    // Test event set/clear performance
    for (int i = 0; i < PERF_TEST_ITERATIONS; i++) {
        uint32_t bit_pattern = (1UL << (i % 32));
        
        // Measure set operation
        uint32_t start_time = get_time_us();
        bool set_result = pico_rtos_event_group_set_bits(&perf_event_group, bit_pattern);
        uint32_t end_time = get_time_us();
        
        if (set_result) {
            uint32_t set_time = end_time - start_time;
            update_performance_metrics(&event_operation_metrics, set_time);
        }
        
        // Measure clear operation
        start_time = get_time_us();
        bool clear_result = pico_rtos_event_group_clear_bits(&perf_event_group, bit_pattern);
        end_time = get_time_us();
        
        if (clear_result) {
            uint32_t clear_time = end_time - start_time;
            update_performance_metrics(&event_operation_metrics, clear_time);
        }
        
        // Occasional delay to prevent overwhelming
        if (i % 100 == 0) {
            pico_rtos_task_delay(1);
        }
    }
    
    print_performance_metrics("Event Group Operations", &event_operation_metrics);
    
    // Validate performance
    TEST_ASSERT(event_operation_metrics.sample_count > 0, "Should have event operation measurements");
    TEST_ASSERT(event_operation_metrics.avg_time_us <= MAX_EVENT_SET_TIME_US,
               "Average event operation time should be within threshold");
    TEST_ASSERT(event_operation_metrics.violations < (event_operation_metrics.sample_count / 20),
               "Event operation violations should be less than 5%");
}

// =============================================================================
// STREAM BUFFER PERFORMANCE TESTS
// =============================================================================

static void test_stream_buffer_performance(void) {
    printf("\n=== Testing Stream Buffer Performance ===\n");
    
    init_performance_metrics(&stream_operation_metrics, MAX_STREAM_SEND_TIME_US);
    
    // Initialize stream buffer
    bool stream_result = pico_rtos_stream_buffer_init(&perf_stream_buffer,
                                                     perf_stream_memory,
                                                     sizeof(perf_stream_memory));
    TEST_ASSERT(stream_result, "Performance stream buffer initialization should succeed");
    
    char test_message[] = "Performance test message";
    char receive_buffer[64];
    
    // Test stream send/receive performance
    for (int i = 0; i < PERF_TEST_ITERATIONS / 2; i++) {
        // Measure send operation
        uint32_t start_time = get_time_us();
        uint32_t sent = pico_rtos_stream_buffer_send(&perf_stream_buffer,
                                                    test_message,
                                                    strlen(test_message),
                                                    0);
        uint32_t end_time = get_time_us();
        
        if (sent > 0) {
            uint32_t send_time = end_time - start_time;
            update_performance_metrics(&stream_operation_metrics, send_time);
        }
        
        // Measure receive operation
        start_time = get_time_us();
        uint32_t received = pico_rtos_stream_buffer_receive(&perf_stream_buffer,
                                                           receive_buffer,
                                                           sizeof(receive_buffer),
                                                           0);
        end_time = get_time_us();
        
        if (received > 0) {
            uint32_t receive_time = end_time - start_time;
            update_performance_metrics(&stream_operation_metrics, receive_time);
        }
        
        // Occasional delay
        if (i % 50 == 0) {
            pico_rtos_task_delay(1);
        }
    }
    
    print_performance_metrics("Stream Buffer Operations", &stream_operation_metrics);
    
    // Validate performance
    TEST_ASSERT(stream_operation_metrics.sample_count > 0, "Should have stream operation measurements");
    TEST_ASSERT(stream_operation_metrics.avg_time_us <= MAX_STREAM_SEND_TIME_US,
               "Average stream operation time should be within threshold");
    TEST_ASSERT(stream_operation_metrics.violations < (stream_operation_metrics.sample_count / 20),
               "Stream operation violations should be less than 5%");
}

// =============================================================================
// REAL-TIME COMPLIANCE TESTS
// =============================================================================

typedef struct {
    uint32_t deadline_us;
    uint32_t actual_time_us;
    bool deadline_met;
} real_time_test_result_t;

static real_time_test_result_t rt_results[100];
static volatile int rt_result_count = 0;

static void real_time_test_task(void *param) {
    uint32_t task_id = (uint32_t)(uintptr_t)param;
    uint32_t deadline_us = 1000 + (task_id * 500); // Different deadlines
    
    for (int i = 0; i < 25; i++) {
        uint32_t start_time = get_time_us();
        
        // Simulate real-time work
        volatile uint32_t work_counter = 0;
        for (uint32_t j = 0; j < 1000; j++) {
            work_counter += j;
        }
        
        uint32_t end_time = get_time_us();
        uint32_t execution_time = end_time - start_time;
        
        // Record result
        if (rt_result_count < 100) {
            rt_results[rt_result_count].deadline_us = deadline_us;
            rt_results[rt_result_count].actual_time_us = execution_time;
            rt_results[rt_result_count].deadline_met = (execution_time <= deadline_us);
            rt_result_count++;
        }
        
        pico_rtos_task_delay(10);
    }
    
    pico_rtos_task_delete(NULL);
}

static void test_real_time_compliance(void) {
    printf("\n=== Testing Real-Time Compliance ===\n");
    
    rt_result_count = 0;
    
    // Create real-time test tasks
    const int num_rt_tasks = 4;
    pico_rtos_task_t rt_tasks[num_rt_tasks];
    uint32_t rt_stacks[num_rt_tasks][PERFORMANCE_TASK_STACK_SIZE / sizeof(uint32_t)];
    
    for (int i = 0; i < num_rt_tasks; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "rt_test_%d", i);
        
        bool result = pico_rtos_task_create(&rt_tasks[i], task_name,
                                           real_time_test_task,
                                           (void *)(uintptr_t)i,
                                           sizeof(rt_stacks[i]),
                                           8 - i); // Higher priority for shorter deadlines
        TEST_ASSERT(result, "Real-time test task creation should succeed");
    }
    
    // Wait for tasks to complete
    uint32_t start_time = get_time_us();
    while (rt_result_count < 100 && (get_time_us() - start_time) < 30000000) { // 30 seconds max
        pico_rtos_task_delay(100);
    }
    
    // Analyze real-time compliance
    uint32_t deadlines_met = 0;
    uint32_t total_tests = rt_result_count;
    
    printf("\nReal-Time Compliance Results:\n");
    printf("Test | Deadline (μs) | Actual (μs) | Met\n");
    printf("-----|---------------|-------------|----\n");
    
    for (int i = 0; i < rt_result_count && i < 20; i++) { // Show first 20 results
        printf("%4d | %12lu | %10lu | %s\n",
               i, rt_results[i].deadline_us, rt_results[i].actual_time_us,
               rt_results[i].deadline_met ? "Yes" : "No");
        
        if (rt_results[i].deadline_met) {
            deadlines_met++;
        }
    }
    
    if (rt_result_count > 20) {
        printf("... (%d more results)\n", rt_result_count - 20);
        
        // Count remaining results
        for (int i = 20; i < rt_result_count; i++) {
            if (rt_results[i].deadline_met) {
                deadlines_met++;
            }
        }
    }
    
    float compliance_rate = total_tests > 0 ? (100.0 * deadlines_met / total_tests) : 0.0;
    printf("\nReal-Time Compliance: %lu/%lu (%.1f%%)\n", 
           deadlines_met, total_tests, compliance_rate);
    
    // Validate real-time compliance
    TEST_ASSERT(total_tests > 0, "Should have real-time test results");
    TEST_ASSERT(compliance_rate >= 95.0, "Real-time compliance should be at least 95%");
}

// =============================================================================
// PERFORMANCE REGRESSION ANALYSIS
// =============================================================================

static void analyze_performance_regression(void) {
    printf("\n=== Performance Regression Analysis ===\n");
    
    // Define baseline performance expectations (from v0.2.1)
    const uint32_t baseline_context_switch_us = 30;
    const uint32_t baseline_memory_alloc_us = 15;
    const uint32_t baseline_event_op_us = 8;
    const uint32_t baseline_stream_op_us = 25;
    
    // Calculate regression percentages
    float cs_regression = context_switch_metrics.avg_time_us > baseline_context_switch_us ?
        (100.0 * (context_switch_metrics.avg_time_us - baseline_context_switch_us) / baseline_context_switch_us) : 0.0;
    
    float mem_regression = memory_alloc_metrics.avg_time_us > baseline_memory_alloc_us ?
        (100.0 * (memory_alloc_metrics.avg_time_us - baseline_memory_alloc_us) / baseline_memory_alloc_us) : 0.0;
    
    float event_regression = event_operation_metrics.avg_time_us > baseline_event_op_us ?
        (100.0 * (event_operation_metrics.avg_time_us - baseline_event_op_us) / baseline_event_op_us) : 0.0;
    
    float stream_regression = stream_operation_metrics.avg_time_us > baseline_stream_op_us ?
        (100.0 * (stream_operation_metrics.avg_time_us - baseline_stream_op_us) / baseline_stream_op_us) : 0.0;
    
    printf("Performance Regression Analysis:\n");
    printf("Operation          | Baseline | Current | Regression\n");
    printf("-------------------|----------|---------|------------\n");
    printf("Context Switch     | %7lu  | %6lu  | %+6.1f%%\n", 
           baseline_context_switch_us, context_switch_metrics.avg_time_us, cs_regression);
    printf("Memory Allocation  | %7lu  | %6lu  | %+6.1f%%\n", 
           baseline_memory_alloc_us, memory_alloc_metrics.avg_time_us, mem_regression);
    printf("Event Operations   | %7lu  | %6lu  | %+6.1f%%\n", 
           baseline_event_op_us, event_operation_metrics.avg_time_us, event_regression);
    printf("Stream Operations  | %7lu  | %6lu  | %+6.1f%%\n", 
           baseline_stream_op_us, stream_operation_metrics.avg_time_us, stream_regression);
    
    // Validate acceptable regression levels (< 20% increase)
    TEST_ASSERT(cs_regression < 20.0, "Context switch regression should be less than 20%");
    TEST_ASSERT(mem_regression < 20.0, "Memory allocation regression should be less than 20%");
    TEST_ASSERT(event_regression < 20.0, "Event operation regression should be less than 20%");
    TEST_ASSERT(stream_regression < 20.0, "Stream operation regression should be less than 20%");
    
    // Overall performance score
    float avg_regression = (cs_regression + mem_regression + event_regression + stream_regression) / 4.0;
    printf("\nOverall Performance Regression: %.1f%%\n", avg_regression);
    
    TEST_ASSERT(avg_regression < 15.0, "Overall performance regression should be less than 15%");
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

static void run_performance_regression_tests(void) {
    printf("\n");
    printf("========================================================\n");
    printf("  Performance Regression Test Suite (Pico-RTOS v0.3.0) \n");
    printf("========================================================\n");
    
    // Initialize profiler for accurate measurements
    pico_rtos_profiler_init();
    
    test_context_switch_performance();
    test_memory_allocation_performance();
    test_event_group_performance();
    test_stream_buffer_performance();
    test_real_time_compliance();
    analyze_performance_regression();
    
    printf("\n");
    printf("========================================================\n");
    printf("         Performance Regression Test Results           \n");
    printf("========================================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_count - test_failures);
    printf("Failed: %d\n", test_failures);
    printf("Success rate: %.1f%%\n", 
           test_count > 0 ? (100.0 * (test_count - test_failures)) / test_count : 0.0);
    printf("========================================================\n");
    
    if (test_failures == 0) {
        printf("🎉 All performance regression tests PASSED!\n");
        printf("✅ v0.3.0 performance is within acceptable limits\n");
    } else {
        printf("❌ Some performance regression tests FAILED!\n");
        printf("⚠️  Performance regression detected - review required\n");
    }
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("Performance Regression Test Suite Starting...\n");
    
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    run_performance_regression_tests();
    
    return (test_failures == 0) ? 0 : 1;
}