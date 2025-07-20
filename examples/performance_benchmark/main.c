/**
 * @file main.c
 * @brief Performance Benchmarking Example for Pico-RTOS
 * 
 * This example provides comprehensive performance measurements for the RTOS including:
 * - Context switch timing measurement
 * - Interrupt latency measurement
 * - Synchronization primitive overhead measurement
 * - Memory allocation performance
 * - Task scheduling overhead
 * - System throughput analysis
 * 
 * The benchmark results are output in a standardized format with baseline metrics
 * for comparison and analysis. This tool is essential for:
 * - System optimization and tuning
 * - Performance regression testing
 * - Comparing different RTOS configurations
 * - Validating real-time requirements
 * 
 * Hardware Requirements:
 * - GPIO pin for interrupt latency testing (default: GPIO 2)
 * - LED for visual indication (default: GPIO 25)
 * 
 * @author Pico-RTOS Development Team
 * @version 0.2.1
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include "pico_rtos.h"

// Hardware configuration
#define BENCHMARK_GPIO_PIN      2   // GPIO pin for interrupt latency testing
#define STATUS_LED_PIN         25   // LED for status indication

// Benchmark configuration
#define CONTEXT_SWITCH_ITERATIONS    1000   // Number of context switches to measure
#define INTERRUPT_LATENCY_ITERATIONS 100    // Number of interrupt latency measurements
#define SYNC_PRIMITIVE_ITERATIONS    1000   // Number of synchronization operations
#define MEMORY_ALLOCATION_ITERATIONS 100    // Number of memory allocation tests
#define TASK_CREATION_ITERATIONS     50     // Number of task creation/deletion cycles

// Task priorities for benchmarking
#define BENCHMARK_CONTROLLER_PRIORITY    5  // Highest priority for benchmark control
#define HIGH_PRIORITY_TASK_PRIORITY      4  // High priority test task
#define MEDIUM_PRIORITY_TASK_PRIORITY    3  // Medium priority test task
#define LOW_PRIORITY_TASK_PRIORITY       2  // Low priority test task
#define IDLE_MONITOR_PRIORITY            1  // Lowest priority for idle monitoring

// Timing measurement precision
#define MICROSECONDS_PER_SECOND     1000000
#define NANOSECONDS_PER_MICROSECOND 1000

/**
 * @brief Structure for storing benchmark results
 */
typedef struct {
    const char *test_name;
    uint32_t iterations;
    uint32_t min_time_us;
    uint32_t max_time_us;
    uint32_t avg_time_us;
    uint32_t total_time_us;
    uint32_t std_deviation_us;
    bool test_passed;
    const char *notes;
} benchmark_result_t;

/**
 * @brief Structure for context switch timing data
 */
typedef struct {
    volatile uint32_t switch_count;
    volatile uint32_t start_time;
    volatile uint32_t end_time;
    volatile bool measurement_active;
    uint32_t measurements[CONTEXT_SWITCH_ITERATIONS];
    uint32_t measurement_index;
} context_switch_data_t;

/**
 * @brief Structure for interrupt latency data
 */
typedef struct {
    volatile uint32_t interrupt_count;
    volatile uint32_t trigger_time;
    volatile uint32_t response_time;
    volatile bool measurement_active;
    uint32_t measurements[INTERRUPT_LATENCY_ITERATIONS];
    uint32_t measurement_index;
} interrupt_latency_data_t;

// Global benchmark data structures
static context_switch_data_t context_switch_data = {0};
static interrupt_latency_data_t interrupt_latency_data = {0};
static benchmark_result_t benchmark_results[10]; // Storage for all benchmark results
static uint32_t result_count = 0;

// Synchronization primitives for testing
static pico_rtos_mutex_t benchmark_mutex;
static pico_rtos_semaphore_t benchmark_semaphore;
static pico_rtos_queue_t benchmark_queue;
static uint32_t queue_buffer[10];

// Task handles for dynamic testing
static pico_rtos_task_t high_priority_task;
static pico_rtos_task_t medium_priority_task;
static pico_rtos_task_t low_priority_task;

/**
 * @brief High-precision timer functions using hardware timer
 */
static inline uint32_t get_time_us(void) {
    return time_us_32();
}

static inline uint64_t get_time_us_64(void) {
    return time_us_64();
}

/**
 * @brief Calculate statistics for benchmark measurements
 */
static void calculate_statistics(uint32_t *measurements, uint32_t count, benchmark_result_t *result) {
    if (count == 0) {
        result->min_time_us = 0;
        result->max_time_us = 0;
        result->avg_time_us = 0;
        result->std_deviation_us = 0;
        return;
    }
    
    // Find min and max
    uint32_t min_val = measurements[0];
    uint32_t max_val = measurements[0];
    uint64_t sum = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        if (measurements[i] < min_val) min_val = measurements[i];
        if (measurements[i] > max_val) max_val = measurements[i];
        sum += measurements[i];
    }
    
    result->min_time_us = min_val;
    result->max_time_us = max_val;
    result->avg_time_us = (uint32_t)(sum / count);
    result->total_time_us = (uint32_t)sum;
    
    // Calculate standard deviation
    uint64_t variance_sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        int32_t diff = (int32_t)measurements[i] - (int32_t)result->avg_time_us;
        variance_sum += (uint64_t)(diff * diff);
    }
    
    uint32_t variance = (uint32_t)(variance_sum / count);
    result->std_deviation_us = (uint32_t)sqrt(variance);
}

/**
 * @brief Print benchmark results in standardized format
 */
static void print_benchmark_result(const benchmark_result_t *result) {
    printf("=== %s ===\n", result->test_name);
    printf("Iterations: %lu\n", result->iterations);
    printf("Min Time: %lu μs\n", result->min_time_us);
    printf("Max Time: %lu μs\n", result->max_time_us);
    printf("Avg Time: %lu μs\n", result->avg_time_us);
    printf("Std Dev: %lu μs\n", result->std_deviation_us);
    printf("Total Time: %lu μs\n", result->total_time_us);
    printf("Status: %s\n", result->test_passed ? "PASS" : "FAIL");
    if (result->notes) {
        printf("Notes: %s\n", result->notes);
    }
    printf("Throughput: %.2f ops/sec\n", 
           result->total_time_us > 0 ? 
           (float)result->iterations * MICROSECONDS_PER_SECOND / result->total_time_us : 0.0f);
    printf("=====================================\n\n");
}

/**
 * @brief GPIO interrupt handler for latency measurement
 */
void benchmark_gpio_interrupt_handler(uint gpio, uint32_t events) {
    if (interrupt_latency_data.measurement_active && 
        interrupt_latency_data.measurement_index < INTERRUPT_LATENCY_ITERATIONS) {
        
        uint32_t response_time = get_time_us();
        uint32_t latency = response_time - interrupt_latency_data.trigger_time;
        
        interrupt_latency_data.measurements[interrupt_latency_data.measurement_index] = latency;
        interrupt_latency_data.measurement_index++;
        interrupt_latency_data.interrupt_count++;
        interrupt_latency_data.response_time = response_time;
    }
}

/**
 * @brief High priority task for context switch measurement
 */
void high_priority_benchmark_task(void *param) {
    while (context_switch_data.measurement_active) {
        if (context_switch_data.switch_count < CONTEXT_SWITCH_ITERATIONS) {
            uint32_t start_time = get_time_us();
            
            // Yield to lower priority task and measure time when we resume
            pico_rtos_task_yield();
            
            uint32_t end_time = get_time_us();
            uint32_t switch_time = end_time - start_time;
            
            context_switch_data.measurements[context_switch_data.switch_count] = switch_time;
            context_switch_data.switch_count++;
        } else {
            context_switch_data.measurement_active = false;
            break;
        }
        
        // Small delay to prevent overwhelming the system
        pico_rtos_task_delay(1);
    }
    
    // Task terminates after measurement
}

/**
 * @brief Medium priority task for context switch measurement
 */
void medium_priority_benchmark_task(void *param) {
    while (context_switch_data.measurement_active) {
        // This task just yields back to higher priority task
        pico_rtos_task_yield();
        pico_rtos_task_delay(1);
    }
}

/**
 * @brief Low priority task for system load simulation
 */
void low_priority_benchmark_task(void *param) {
    uint32_t counter = 0;
    while (context_switch_data.measurement_active) {
        counter++;
        if (counter % 1000 == 0) {
            pico_rtos_task_yield();
        }
    }
}

/**
 * @brief Benchmark context switch performance
 */
static bool benchmark_context_switch(void) {
    printf("Starting context switch benchmark...\n");
    
    // Initialize measurement data
    memset(&context_switch_data, 0, sizeof(context_switch_data));
    context_switch_data.measurement_active = true;
    
    // Create benchmark tasks
    if (!pico_rtos_task_create(&high_priority_task, "HighPriBench", 
                               high_priority_benchmark_task, NULL, 512, 
                               HIGH_PRIORITY_TASK_PRIORITY)) {
        printf("Failed to create high priority benchmark task\n");
        return false;
    }
    
    if (!pico_rtos_task_create(&medium_priority_task, "MedPriBench", 
                               medium_priority_benchmark_task, NULL, 512, 
                               MEDIUM_PRIORITY_TASK_PRIORITY)) {
        printf("Failed to create medium priority benchmark task\n");
        return false;
    }
    
    if (!pico_rtos_task_create(&low_priority_task, "LowPriBench", 
                               low_priority_benchmark_task, NULL, 512, 
                               LOW_PRIORITY_TASK_PRIORITY)) {
        printf("Failed to create low priority benchmark task\n");
        return false;
    }
    
    // Wait for measurement to complete
    uint32_t timeout = 0;
    while (context_switch_data.measurement_active && timeout < 10000) {
        pico_rtos_task_delay(10);
        timeout += 10;
    }
    
    // Clean up tasks
    pico_rtos_task_delete(&high_priority_task);
    pico_rtos_task_delete(&medium_priority_task);
    pico_rtos_task_delete(&low_priority_task);
    
    // Calculate and store results
    benchmark_result_t *result = &benchmark_results[result_count++];
    result->test_name = "Context Switch Performance";
    result->iterations = context_switch_data.switch_count;
    calculate_statistics(context_switch_data.measurements, context_switch_data.switch_count, result);
    result->test_passed = (result->avg_time_us < 100); // Pass if average < 100μs
    result->notes = "Time for task yield and resume cycle";
    
    print_benchmark_result(result);
    return result->test_passed;
}

/**
 * @brief Benchmark interrupt latency
 */
static bool benchmark_interrupt_latency(void) {
    printf("Starting interrupt latency benchmark...\n");
    
    // Initialize GPIO for interrupt testing
    gpio_init(BENCHMARK_GPIO_PIN);
    gpio_set_dir(BENCHMARK_GPIO_PIN, GPIO_OUT);
    gpio_put(BENCHMARK_GPIO_PIN, 0);
    
    // Configure interrupt
    gpio_set_irq_enabled_with_callback(BENCHMARK_GPIO_PIN, GPIO_IRQ_EDGE_RISE, 
                                       true, &benchmark_gpio_interrupt_handler);
    
    // Initialize measurement data
    memset(&interrupt_latency_data, 0, sizeof(interrupt_latency_data));
    interrupt_latency_data.measurement_active = true;
    
    // Perform interrupt latency measurements
    for (uint32_t i = 0; i < INTERRUPT_LATENCY_ITERATIONS; i++) {
        // Record trigger time and generate interrupt
        interrupt_latency_data.trigger_time = get_time_us();
        gpio_put(BENCHMARK_GPIO_PIN, 1);
        
        // Wait for interrupt to be processed
        uint32_t timeout = 0;
        while (interrupt_latency_data.measurement_index <= i && timeout < 1000) {
            timeout++;
            busy_wait_us(1);
        }
        
        // Reset GPIO and wait before next measurement
        gpio_put(BENCHMARK_GPIO_PIN, 0);
        pico_rtos_task_delay(1);
    }
    
    interrupt_latency_data.measurement_active = false;
    
    // Disable interrupt
    gpio_set_irq_enabled(BENCHMARK_GPIO_PIN, GPIO_IRQ_EDGE_RISE, false);
    
    // Calculate and store results
    benchmark_result_t *result = &benchmark_results[result_count++];
    result->test_name = "Interrupt Latency";
    result->iterations = interrupt_latency_data.measurement_index;
    calculate_statistics(interrupt_latency_data.measurements, 
                        interrupt_latency_data.measurement_index, result);
    result->test_passed = (result->avg_time_us < 50); // Pass if average < 50μs
    result->notes = "Time from interrupt trigger to ISR execution";
    
    print_benchmark_result(result);
    return result->test_passed;
}

/**
 * @brief Benchmark mutex performance
 */
static bool benchmark_mutex_performance(void) {
    printf("Starting mutex performance benchmark...\n");
    
    uint32_t measurements[SYNC_PRIMITIVE_ITERATIONS];
    uint32_t successful_operations = 0;
    
    for (uint32_t i = 0; i < SYNC_PRIMITIVE_ITERATIONS; i++) {
        uint32_t start_time = get_time_us();
        
        if (pico_rtos_mutex_lock(&benchmark_mutex, 1000)) {
            pico_rtos_mutex_unlock(&benchmark_mutex);
            uint32_t end_time = get_time_us();
            measurements[successful_operations] = end_time - start_time;
            successful_operations++;
        }
    }
    
    // Calculate and store results
    benchmark_result_t *result = &benchmark_results[result_count++];
    result->test_name = "Mutex Lock/Unlock Performance";
    result->iterations = successful_operations;
    calculate_statistics(measurements, successful_operations, result);
    result->test_passed = (result->avg_time_us < 20); // Pass if average < 20μs
    result->notes = "Time for mutex lock and unlock cycle";
    
    print_benchmark_result(result);
    return result->test_passed;
}

/**
 * @brief Benchmark semaphore performance
 */
static bool benchmark_semaphore_performance(void) {
    printf("Starting semaphore performance benchmark...\n");
    
    uint32_t measurements[SYNC_PRIMITIVE_ITERATIONS];
    uint32_t successful_operations = 0;
    
    for (uint32_t i = 0; i < SYNC_PRIMITIVE_ITERATIONS; i++) {
        uint32_t start_time = get_time_us();
        
        if (pico_rtos_semaphore_take(&benchmark_semaphore, 1000)) {
            pico_rtos_semaphore_give(&benchmark_semaphore);
            uint32_t end_time = get_time_us();
            measurements[successful_operations] = end_time - start_time;
            successful_operations++;
        }
    }
    
    // Calculate and store results
    benchmark_result_t *result = &benchmark_results[result_count++];
    result->test_name = "Semaphore Take/Give Performance";
    result->iterations = successful_operations;
    calculate_statistics(measurements, successful_operations, result);
    result->test_passed = (result->avg_time_us < 25); // Pass if average < 25μs
    result->notes = "Time for semaphore take and give cycle";
    
    print_benchmark_result(result);
    return result->test_passed;
}

/**
 * @brief Benchmark queue performance
 */
static bool benchmark_queue_performance(void) {
    printf("Starting queue performance benchmark...\n");
    
    uint32_t measurements[SYNC_PRIMITIVE_ITERATIONS];
    uint32_t successful_operations = 0;
    uint32_t test_data = 0x12345678;
    uint32_t received_data;
    
    for (uint32_t i = 0; i < SYNC_PRIMITIVE_ITERATIONS; i++) {
        uint32_t start_time = get_time_us();
        
        if (pico_rtos_queue_send(&benchmark_queue, &test_data, PICO_RTOS_NO_WAIT)) {
            if (pico_rtos_queue_receive(&benchmark_queue, &received_data, PICO_RTOS_NO_WAIT)) {
                uint32_t end_time = get_time_us();
                measurements[successful_operations] = end_time - start_time;
                successful_operations++;
            }
        }
    }
    
    // Calculate and store results
    benchmark_result_t *result = &benchmark_results[result_count++];
    result->test_name = "Queue Send/Receive Performance";
    result->iterations = successful_operations;
    calculate_statistics(measurements, successful_operations, result);
    result->test_passed = (result->avg_time_us < 30); // Pass if average < 30μs
    result->notes = "Time for queue send and receive cycle";
    
    print_benchmark_result(result);
    return result->test_passed;
}

/**
 * @brief Benchmark memory allocation performance
 */
static bool benchmark_memory_allocation(void) {
    printf("Starting memory allocation benchmark...\n");
    
    uint32_t measurements[MEMORY_ALLOCATION_ITERATIONS];
    uint32_t successful_operations = 0;
    
    for (uint32_t i = 0; i < MEMORY_ALLOCATION_ITERATIONS; i++) {
        uint32_t start_time = get_time_us();
        
        void *ptr = pico_rtos_malloc(256); // Allocate 256 bytes
        if (ptr != NULL) {
            pico_rtos_free(ptr, 256);
            uint32_t end_time = get_time_us();
            measurements[successful_operations] = end_time - start_time;
            successful_operations++;
        }
    }
    
    // Calculate and store results
    benchmark_result_t *result = &benchmark_results[result_count++];
    result->test_name = "Memory Allocation Performance";
    result->iterations = successful_operations;
    calculate_statistics(measurements, successful_operations, result);
    result->test_passed = (result->avg_time_us < 100); // Pass if average < 100μs
    result->notes = "Time for 256-byte malloc and free cycle";
    
    print_benchmark_result(result);
    return result->test_passed;
}

/**
 * @brief Print system baseline metrics
 */
static void print_baseline_metrics(void) {
    printf("\n=== SYSTEM BASELINE METRICS ===\n");
    
    // System information
    printf("RTOS Version: %s\n", pico_rtos_get_version_string());
    printf("System Clock: %lu Hz\n", clock_get_hz(clk_sys));
    printf("Tick Rate: 1000 Hz (1ms)\n"); // Assuming default tick rate
    
    // Memory information
    uint32_t current_memory, peak_memory, allocations;
    pico_rtos_get_memory_stats(&current_memory, &peak_memory, &allocations);
    printf("Memory - Current: %lu bytes, Peak: %lu bytes, Allocations: %lu\n",
           current_memory, peak_memory, allocations);
    
    // System statistics
    pico_rtos_system_stats_t stats;
    pico_rtos_get_system_stats(&stats);
    printf("Tasks - Total: %lu, Ready: %lu, Blocked: %lu\n",
           stats.total_tasks, stats.ready_tasks, stats.blocked_tasks);
    printf("System Uptime: %lu ms\n", stats.system_uptime);
    printf("Idle Counter: %lu\n", stats.idle_counter);
    
    printf("================================\n\n");
}

/**
 * @brief Print comprehensive benchmark summary
 */
static void print_benchmark_summary(void) {
    printf("\n=== BENCHMARK SUMMARY ===\n");
    
    uint32_t passed_tests = 0;
    uint32_t total_tests = result_count;
    
    for (uint32_t i = 0; i < result_count; i++) {
        printf("%-30s: %s (%lu μs avg)\n", 
               benchmark_results[i].test_name,
               benchmark_results[i].test_passed ? "PASS" : "FAIL",
               benchmark_results[i].avg_time_us);
        if (benchmark_results[i].test_passed) {
            passed_tests++;
        }
    }
    
    printf("\nOverall Result: %lu/%lu tests passed (%.1f%%)\n",
           passed_tests, total_tests, 
           total_tests > 0 ? (float)passed_tests * 100.0f / total_tests : 0.0f);
    
    if (passed_tests == total_tests) {
        printf("Status: ALL BENCHMARKS PASSED ✓\n");
    } else {
        printf("Status: SOME BENCHMARKS FAILED ✗\n");
    }
    
    printf("=========================\n\n");
}

/**
 * @brief Main benchmark controller task
 */
void benchmark_controller_task(void *param) {
    printf("\n=== PICO-RTOS PERFORMANCE BENCHMARK v0.2.1 ===\n");
    printf("Starting comprehensive performance analysis...\n\n");
    
    // Print baseline system metrics
    print_baseline_metrics();
    
    // Initialize synchronization primitives for testing
    if (!pico_rtos_mutex_init(&benchmark_mutex)) {
        printf("ERROR: Failed to initialize benchmark mutex\n");
        return;
    }
    
    if (!pico_rtos_semaphore_init(&benchmark_semaphore, 1, 1)) {
        printf("ERROR: Failed to initialize benchmark semaphore\n");
        return;
    }
    
    if (!pico_rtos_queue_init(&benchmark_queue, queue_buffer, 
                              sizeof(uint32_t), 10)) {
        printf("ERROR: Failed to initialize benchmark queue\n");
        return;
    }
    
    // Run all benchmarks
    bool all_passed = true;
    
    printf("Running performance benchmarks...\n\n");
    
    // Context switch benchmark
    if (!benchmark_context_switch()) {
        all_passed = false;
    }
    
    // Interrupt latency benchmark
    if (!benchmark_interrupt_latency()) {
        all_passed = false;
    }
    
    // Synchronization primitive benchmarks
    if (!benchmark_mutex_performance()) {
        all_passed = false;
    }
    
    if (!benchmark_semaphore_performance()) {
        all_passed = false;
    }
    
    if (!benchmark_queue_performance()) {
        all_passed = false;
    }
    
    // Memory allocation benchmark
    if (!benchmark_memory_allocation()) {
        all_passed = false;
    }
    
    // Print final summary
    print_benchmark_summary();
    
    // Final status indication
    gpio_put(STATUS_LED_PIN, all_passed ? 1 : 0);
    
    printf("Benchmark complete. LED %s indicates %s\n",
           all_passed ? "ON" : "OFF",
           all_passed ? "all tests passed" : "some tests failed");
    
    // Keep task alive for continuous monitoring
    while (1) {
        pico_rtos_task_delay(10000); // Report every 10 seconds
        printf("System still running - uptime: %lu ms\n", pico_rtos_get_uptime_ms());
    }
}

/**
 * @brief Initialize hardware for benchmarking
 */
static bool init_benchmark_hardware(void) {
    // Initialize status LED
    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_put(STATUS_LED_PIN, 0);
    
    printf("Benchmark hardware initialized\n");
    printf("Status LED: GPIO %d\n", STATUS_LED_PIN);
    printf("Interrupt test pin: GPIO %d\n", BENCHMARK_GPIO_PIN);
    
    return true;
}

/**
 * @brief Main function - Initialize system and start benchmarks
 */
int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    printf("\n=== Pico-RTOS Performance Benchmark Example ===\n");
    printf("This example measures RTOS performance characteristics\n");
    printf("including context switches, interrupt latency, and\n");
    printf("synchronization primitive overhead.\n\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    printf("RTOS initialized successfully\n");
    
    // Initialize benchmark hardware
    if (!init_benchmark_hardware()) {
        printf("ERROR: Failed to initialize benchmark hardware\n");
        return -1;
    }
    
    // Create benchmark controller task
    pico_rtos_task_t benchmark_task;
    if (!pico_rtos_task_create(&benchmark_task,
                               "BenchmarkController",
                               benchmark_controller_task,
                               NULL,
                               2048, // Larger stack for benchmark processing
                               BENCHMARK_CONTROLLER_PRIORITY)) {
        printf("ERROR: Failed to create benchmark controller task\n");
        return -1;
    }
    printf("Benchmark controller task created\n");
    
    printf("\nStarting RTOS scheduler...\n");
    printf("Benchmark will begin automatically.\n\n");
    
    // Start the RTOS scheduler
    pico_rtos_start();
    
    // We should never reach here
    printf("ERROR: RTOS scheduler returned unexpectedly\n");
    return -1;
}