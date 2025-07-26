/**
 * @file main.c
 * @brief Debugging and Profiling Real-Time Analysis Example for Pico-RTOS v0.3.0
 * 
 * This example demonstrates comprehensive debugging and profiling capabilities:
 * 
 * 1. Runtime task inspection and monitoring
 * 2. Execution time profiling for performance analysis
 * 3. System event tracing for behavior analysis
 * 4. Memory usage tracking and leak detection
 * 5. Real-time performance metrics collection
 * 6. Configurable assertion handling and error reporting
 * 
 * The example simulates a complex real-time system with various tasks that
 * demonstrate different debugging and profiling scenarios.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"

// Disable migration warnings for examples (they already use v0.3.0 APIs)
#define PICO_RTOS_DISABLE_MIGRATION_WARNINGS
#include "pico_rtos.h"

// =============================================================================
// CONFIGURATION AND PROFILING DEFINITIONS
// =============================================================================

#define LED_PIN 25

// Function IDs for profiling
#define FUNC_ID_SENSOR_READ         1
#define FUNC_ID_DATA_PROCESS        2
#define FUNC_ID_CONTROL_ALGORITHM   3
#define FUNC_ID_COMMUNICATION       4
#define FUNC_ID_MEMORY_OPERATION    5
#define FUNC_ID_CRITICAL_SECTION    6

// Trace buffer configuration
#define TRACE_BUFFER_SIZE           1000
#define PROFILER_MAX_ENTRIES        50

// Performance thresholds (in microseconds)
#define PERFORMANCE_THRESHOLD_WARN  1000
#define PERFORMANCE_THRESHOLD_ERROR 5000

// =============================================================================
// DATA STRUCTURES
// =============================================================================

typedef struct {
    uint32_t sensor_id;
    float value;
    uint32_t timestamp;
    uint32_t quality;
} sensor_reading_t;

typedef struct {
    float input_values[4];
    float output_values[4];
    uint32_t processing_time_us;
    uint32_t algorithm_version;
} processing_result_t;

typedef struct {
    uint32_t task_count;
    uint32_t memory_usage;
    uint32_t cpu_utilization;
    uint32_t context_switches;
    uint32_t interrupt_count;
} system_metrics_t;

// Performance tracking structure
typedef struct {
    uint32_t function_calls;
    uint32_t call_count;
    uint32_t total_time_us;
    uint32_t total_execution_time;
    uint32_t max_time_us;
    uint32_t max_execution_time;
    uint32_t min_time_us;
    uint32_t min_execution_time;
    uint32_t threshold_violations;
    uint32_t entry_time;
} function_performance_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

// Profiling and debugging state
static bool profiling_enabled = true;
static bool tracing_enabled = true;
static bool debug_mode = true;

// Performance tracking
static function_performance_t function_performance[10] = {0};
static pico_rtos_mutex_t performance_mutex;

// System metrics
static system_metrics_t current_metrics = {0};
static pico_rtos_mutex_t metrics_mutex;

// Memory pools for testing
static pico_rtos_memory_pool_t test_memory_pool;
static uint8_t pool_memory[64 * 32];  // 32 blocks of 64 bytes

// System control
static volatile bool system_running = true;

// =============================================================================
// PROFILING AND DEBUGGING UTILITIES
// =============================================================================

/**
 * @brief Enhanced function profiling with threshold checking
 */
void profile_function_enter(uint32_t function_id) {
    if (!profiling_enabled || function_id >= 10) return;
    
    // Store entry time for manual profiling
    if (function_id < 10) {
        function_performance[function_id].entry_time = pico_rtos_get_tick_count();
    }
}

void profile_function_exit(uint32_t function_id) {
    if (!profiling_enabled || function_id >= 10) return;
    
    // Calculate execution time for manual profiling
    if (function_id < 10) {
        uint32_t exit_time = pico_rtos_get_tick_count();
        uint32_t execution_time = (exit_time - function_performance[function_id].entry_time) * 1000; // Convert to microseconds
        function_performance[function_id].total_execution_time += execution_time;
        function_performance[function_id].call_count++;
        
        if (execution_time > function_performance[function_id].max_execution_time) {
            function_performance[function_id].max_execution_time = execution_time;
        }
        if (execution_time < function_performance[function_id].min_execution_time || 
            function_performance[function_id].min_execution_time == 0) {
            function_performance[function_id].min_execution_time = execution_time;
        }
        
        if (execution_time > PERFORMANCE_THRESHOLD_ERROR) {
            function_performance[function_id].threshold_violations++;
        }
    }
}

/**
 * @brief Update function performance statistics
 */
void update_function_performance(uint32_t function_id, uint32_t execution_time_us) {
    if (function_id >= 10) return;
    
    if (pico_rtos_mutex_lock(&performance_mutex, 100)) {
        function_performance_t *perf = &function_performance[function_id];
        
        perf->function_calls++;
        perf->total_time_us += execution_time_us;
        
        if (execution_time_us > perf->max_time_us) {
            perf->max_time_us = execution_time_us;
        }
        if (perf->min_time_us == 0 || execution_time_us < perf->min_time_us) {
            perf->min_time_us = execution_time_us;
        }
        
        // Check performance thresholds
        if (execution_time_us > PERFORMANCE_THRESHOLD_ERROR) {
            perf->threshold_violations++;
            printf("[PERF ERROR] Function %lu exceeded error threshold: %lu μs\n", 
                   function_id, execution_time_us);
        } else if (execution_time_us > PERFORMANCE_THRESHOLD_WARN) {
            perf->threshold_violations++;
            printf("[PERF WARN] Function %lu exceeded warning threshold: %lu μs\n", 
                   function_id, execution_time_us);
        }
        
        pico_rtos_mutex_unlock(&performance_mutex);
    }
}

/**
 * @brief Simulate sensor reading with profiling
 */
sensor_reading_t read_sensor_with_profiling(uint32_t sensor_id) {
    uint32_t start_time = pico_rtos_get_tick_count();
    profile_function_enter(FUNC_ID_SENSOR_READ);
    
    sensor_reading_t reading;
    reading.sensor_id = sensor_id;
    reading.timestamp = start_time;
    
    // Simulate sensor reading with variable delay
    uint32_t read_delay = 5 + (sensor_id % 20);
    pico_rtos_task_delay(read_delay);
    
    // Generate realistic sensor data
    float base_value = 20.0f + (float)(sensor_id * 5);
    reading.value = base_value + sinf((float)start_time / 1000.0f) * 10.0f;
    reading.quality = (start_time % 100 < 95) ? 100 : 50;  // 95% good quality
    
    // Occasionally simulate sensor errors
    if ((start_time % 200) == 0) {
        reading.quality = 0;  // Sensor error
        printf("[SENSOR ERROR] Sensor %lu read error at timestamp %lu\n", sensor_id, start_time);
    }
    
    profile_function_exit(FUNC_ID_SENSOR_READ);
    uint32_t execution_time = (pico_rtos_get_tick_count() - start_time) * 1000;
    update_function_performance(FUNC_ID_SENSOR_READ, execution_time);
    
    return reading;
}

/**
 * @brief Process data with profiling and error injection
 */
processing_result_t process_data_with_profiling(const sensor_reading_t *readings, uint32_t count) {
    uint32_t start_time = pico_rtos_get_tick_count();
    profile_function_enter(FUNC_ID_DATA_PROCESS);
    
    processing_result_t result = {0};
    result.algorithm_version = 1;
    
    // Copy input values
    for (uint32_t i = 0; i < count && i < 4; i++) {
        result.input_values[i] = readings[i].value;
    }
    
    // Simulate complex data processing
    for (uint32_t i = 0; i < count && i < 4; i++) {
        // Apply filtering algorithm
        result.output_values[i] = result.input_values[i] * 0.8f + 
                                 (i > 0 ? result.output_values[i-1] * 0.2f : 0.0f);
        
        // Simulate computational load
        for (uint32_t j = 0; j < 100; j++) {
            result.output_values[i] += sinf((float)j / 10.0f) * 0.001f;
        }
    }
    
    // Occasionally simulate processing errors
    if ((start_time % 150) == 0) {
        printf("[PROCESSING ERROR] Data processing error at timestamp %lu\n", start_time);
        // Set error flag in result
        result.algorithm_version = 0;  // Error indicator
    }
    
    // Simulate variable processing time
    uint32_t extra_delay = (count * 10) + (start_time % 30);
    pico_rtos_task_delay(extra_delay);
    
    profile_function_exit(FUNC_ID_DATA_PROCESS);
    uint32_t execution_time = (pico_rtos_get_tick_count() - start_time) * 1000;
    result.processing_time_us = execution_time;
    update_function_performance(FUNC_ID_DATA_PROCESS, execution_time);
    
    return result;
}

/**
 * @brief Control algorithm with critical section profiling
 */
void execute_control_algorithm_with_profiling(const processing_result_t *data) {
    uint32_t start_time = pico_rtos_get_tick_count();
    profile_function_enter(FUNC_ID_CONTROL_ALGORITHM);
    
    // Simulate critical section
    profile_function_enter(FUNC_ID_CRITICAL_SECTION);
    pico_rtos_enter_critical();
    
    // Critical section work (should be minimal)
    volatile uint32_t critical_work = 0;
    for (uint32_t i = 0; i < 1000; i++) {
        critical_work += i;
    }
    
    pico_rtos_exit_critical();
    profile_function_exit(FUNC_ID_CRITICAL_SECTION);
    
    // Non-critical control algorithm work
    float control_output = 0.0f;
    for (uint32_t i = 0; i < 4; i++) {
        control_output += data->output_values[i] * 0.25f;
    }
    
    // Simulate control output application
    pico_rtos_task_delay(5 + (start_time % 15));
    
    profile_function_exit(FUNC_ID_CONTROL_ALGORITHM);
    uint32_t execution_time = (pico_rtos_get_tick_count() - start_time) * 1000;
    update_function_performance(FUNC_ID_CONTROL_ALGORITHM, execution_time);
    
    if (debug_mode && (start_time % 500 == 0)) {
        printf("[CONTROL DEBUG] Control output: %.2f, processing time: %lu μs\n", 
               control_output, execution_time);
    }
}

// =============================================================================
// TASK IMPLEMENTATIONS WITH DEBUGGING
// =============================================================================

/**
 * @brief High-priority real-time task with strict timing requirements
 */
void realtime_control_task(void *param) {
    uint32_t task_id = (uint32_t)param;
    uint32_t cycle_count = 0;
    sensor_reading_t readings[2];
    
    printf("[RT Control %lu] Real-time control task started\n", task_id);
    
    while (system_running) {
        uint32_t cycle_start = pico_rtos_get_tick_count();
        
        // Read multiple sensors
        readings[0] = read_sensor_with_profiling(task_id * 2);
        readings[1] = read_sensor_with_profiling(task_id * 2 + 1);
        
        // Process sensor data
        processing_result_t processed = process_data_with_profiling(readings, 2);
        
        // Execute control algorithm
        if (processed.algorithm_version > 0) {  // Only if processing was successful
            execute_control_algorithm_with_profiling(&processed);
        } else {
            printf("[RT Control %lu] Skipping control due to processing error\n", task_id);
        }
        
        cycle_count++;
        
        // Check cycle timing
        uint32_t cycle_time = (pico_rtos_get_tick_count() - cycle_start) * 1000;
        if (cycle_time > 50000) {  // 50ms cycle time limit
            printf("[RT Control %lu] TIMING VIOLATION: Cycle %lu took %lu μs (limit: 50000 μs)\n",
                   task_id, cycle_count, cycle_time);
        }
        
        // Log periodic status
        if (cycle_count % 100 == 0) {
            printf("[RT Control %lu] Completed %lu cycles, last cycle: %lu μs\n",
                   task_id, cycle_count, cycle_time);
        }
        
        // Maintain 20Hz control rate (50ms period)
        pico_rtos_task_delay(50);
    }
    
    printf("[RT Control %lu] Real-time control task stopped after %lu cycles\n", task_id, cycle_count);
}

/**
 * @brief Data logging task with memory pool usage
 */
void data_logging_task(void *param) {
    uint32_t logger_id = (uint32_t)param;
    uint32_t log_entries = 0;
    
    printf("[Logger %lu] Data logging task started\n", logger_id);
    
    while (system_running) {
        uint32_t start_time = pico_rtos_get_tick_count();
        profile_function_enter(FUNC_ID_MEMORY_OPERATION);
        
        // Allocate memory from pool for log entry
        void *log_buffer = pico_rtos_memory_pool_alloc(&test_memory_pool, 1000);
        
        if (log_buffer) {
            // Simulate log data creation
            snprintf((char*)log_buffer, 64, "Log entry %lu from logger %lu at %lu ms",
                    log_entries, logger_id, start_time);
            
            // Simulate log processing
            pico_rtos_task_delay(10 + (log_entries % 20));
            
            // Free the memory
            if (!pico_rtos_memory_pool_free(&test_memory_pool, log_buffer)) {
                printf("[Logger %lu] ERROR: Failed to free log buffer\n", logger_id);
            }
            
            log_entries++;
            
            if (log_entries % 50 == 0) {
                printf("[Logger %lu] Logged %lu entries\n", logger_id, log_entries);
            }
            
        } else {
            printf("[Logger %lu] ERROR: Failed to allocate log buffer (entry %lu)\n", 
                   logger_id, log_entries);
        }
        
        profile_function_exit(FUNC_ID_MEMORY_OPERATION);
        uint32_t execution_time = (pico_rtos_get_tick_count() - start_time) * 1000;
        update_function_performance(FUNC_ID_MEMORY_OPERATION, execution_time);
        
        pico_rtos_task_delay(200 + (logger_id * 100));  // Staggered logging rates
    }
    
    printf("[Logger %lu] Data logging task stopped after %lu entries\n", logger_id, log_entries);
}

/**
 * @brief Communication task with error simulation
 */
void communication_task(void *param) {
    uint32_t comm_id = (uint32_t)param;
    uint32_t message_count = 0;
    uint32_t error_count = 0;
    
    printf("[Comm %lu] Communication task started\n", comm_id);
    
    while (system_running) {
        uint32_t start_time = pico_rtos_get_tick_count();
        profile_function_enter(FUNC_ID_COMMUNICATION);
        
        // Simulate message preparation
        char message[128];
        snprintf(message, sizeof(message), "Message %lu from comm %lu", message_count, comm_id);
        
        // Simulate communication with variable delay and errors
        uint32_t comm_delay = 20 + (message_count % 100);
        pico_rtos_task_delay(comm_delay);
        
        // Simulate communication errors
        bool comm_success = true;
        if ((message_count % 25) == 0) {  // 4% error rate
            comm_success = false;
            error_count++;
            printf("[Comm %lu] COMMUNICATION ERROR: Message %lu failed to send\n", 
                   comm_id, message_count);
        }
        
        if (comm_success) {
            message_count++;
            
            if (message_count % 100 == 0) {
                printf("[Comm %lu] Sent %lu messages, %lu errors\n", 
                       comm_id, message_count, error_count);
            }
        }
        
        profile_function_exit(FUNC_ID_COMMUNICATION);
        uint32_t execution_time = (pico_rtos_get_tick_count() - start_time) * 1000;
        update_function_performance(FUNC_ID_COMMUNICATION, execution_time);
        
        pico_rtos_task_delay(500 + (comm_id * 200));
    }
    
    printf("[Comm %lu] Communication task stopped (sent %lu, errors %lu)\n", 
           comm_id, message_count, error_count);
}

// =============================================================================
// DEBUGGING AND MONITORING TASKS
// =============================================================================

/**
 * @brief Task inspector - monitors all tasks in the system
 */
void task_inspector_task(void *param) {
    pico_rtos_task_info_t task_info_array[16];
    uint32_t inspection_cycle = 0;
    
    printf("[Task Inspector] Task inspection started\n");
    
    while (system_running) {
        inspection_cycle++;
        
        printf("\n=== TASK INSPECTION REPORT #%lu ===\n", inspection_cycle);
        
        // Get information about all tasks
        uint32_t task_count = pico_rtos_debug_get_all_task_info(task_info_array, 16);
        
        printf("Total tasks: %lu\n", task_count);
        printf("%-15s %-8s %-10s %-10s %-12s %-10s\n", 
               "Task Name", "State", "Priority", "Stack Use", "CPU Time", "Switches");
        printf("%-15s %-8s %-10s %-10s %-12s %-10s\n", 
               "---------------", "--------", "----------", "----------", "------------", "----------");
        
        for (uint32_t i = 0; i < task_count; i++) {
            const char *state_str;
            switch (task_info_array[i].state) {
                case PICO_RTOS_TASK_STATE_READY: state_str = "READY"; break;
                case PICO_RTOS_TASK_STATE_RUNNING: state_str = "RUNNING"; break;
                case PICO_RTOS_TASK_STATE_BLOCKED: state_str = "BLOCKED"; break;
                case PICO_RTOS_TASK_STATE_SUSPENDED: state_str = "SUSPEND"; break;
                default: state_str = "UNKNOWN"; break;
            }
            
            printf("%-15s %-8s %-10lu %-10lu %-12llu %-10lu\n",
                   task_info_array[i].name,
                   state_str,
                   task_info_array[i].priority,
                   task_info_array[i].stack_usage,
                   task_info_array[i].cpu_time_us,
                   task_info_array[i].context_switches);
            
            // Check for potential issues
            if (task_info_array[i].stack_usage > task_info_array[i].stack_high_water * 0.9f) {
                printf("  WARNING: %s stack usage high (%.1f%%)\n",
                       task_info_array[i].name,
                       (float)task_info_array[i].stack_usage / task_info_array[i].stack_high_water * 100.0f);
            }
        }
        
        printf("==========================================\n\n");
        
        pico_rtos_task_delay(10000);  // Inspect every 10 seconds
    }
    
    printf("[Task Inspector] Task inspection stopped\n");
}

/**
 * @brief Performance analyzer - analyzes profiling data
 */
void performance_analyzer_task(void *param) {
    uint32_t analysis_cycle = 0;
    
    printf("[Performance Analyzer] Performance analysis started\n");
    
    while (system_running) {
        analysis_cycle++;
        
        printf("\n=== PERFORMANCE ANALYSIS REPORT #%lu ===\n", analysis_cycle);
        
        // Analyze manual profiling results
        printf("Function Performance Analysis:\n");
        printf("%-15s %-8s %-12s %-10s %-10s %-10s %-12s\n",
               "Function", "Calls", "Total(μs)", "Avg(μs)", "Min(μs)", "Max(μs)", "Violations");
        printf("%-15s %-8s %-12s %-10s %-10s %-10s %-12s\n",
               "--------", "-----", "---------", "-------", "-------", "-------", "----------");
        
        for (uint32_t i = 0; i < 10; i++) {
            if (function_performance[i].call_count > 0) {
                const char *function_names[] = {
                    "sensor_read", "data_process", "queue_send", "queue_receive",
                    "mutex_lock", "semaphore_take", "task_delay", "func_7", "func_8", "func_9"
                };
                
                uint32_t avg_time = function_performance[i].call_count > 0 ? 
                    function_performance[i].total_execution_time / function_performance[i].call_count : 0;
                
                printf("%-15s %-8lu %-12lu %-10lu %-10lu %-10lu %-12lu\n",
                       function_names[i],
                       function_performance[i].call_count,
                       function_performance[i].total_execution_time,
                       avg_time,
                       function_performance[i].min_execution_time,
                       function_performance[i].max_execution_time,
                       function_performance[i].threshold_violations);
                
                // Check for performance issues
                if (function_performance[i].max_execution_time > PERFORMANCE_THRESHOLD_ERROR) {
                    printf("  ⚠️  ERROR: Function exceeds critical threshold!\n");
                } else if (avg_time > PERFORMANCE_THRESHOLD_WARN) {
                    printf("  ⚠️  WARNING: Function approaching threshold\n");
                }
            }
        }
        
        // Memory pool statistics
        pico_rtos_memory_pool_stats_t pool_stats;
        if (pico_rtos_memory_pool_get_stats(&test_memory_pool, &pool_stats)) {
            printf("\nMemory Pool Statistics:\n");
            printf("  Total blocks: %lu, Free: %lu, Allocated: %lu\n",
                   pool_stats.total_blocks, pool_stats.free_blocks, pool_stats.allocated_blocks);
            printf("  Peak usage: %lu blocks (%.1f%%)\n",
                   pool_stats.peak_allocated,
                   (float)pool_stats.peak_allocated / pool_stats.total_blocks * 100.0f);
            printf("  Total allocations: %lu, Failures: %lu\n",
                   pool_stats.allocation_count, pool_stats.allocation_failures);
            
            if (pool_stats.allocation_failures > 0) {
                printf("  WARNING: Memory allocation failures detected\n");
            }
        }
        
        printf("=============================================\n\n");
        
        // Trigger shutdown after extended analysis
        if (analysis_cycle >= 8) {
            printf("[Performance Analyzer] Analysis complete, triggering shutdown\n");
            system_running = false;
        }
        
        pico_rtos_task_delay(12000);  // Analyze every 12 seconds
    }
    
    printf("[Performance Analyzer] Performance analysis stopped\n");
}

/**
 * @brief System health monitor
 */
void system_health_monitor_task(void *param) {
    uint32_t health_checks = 0;
    
    printf("[Health Monitor] System health monitoring started\n");
    
    while (system_running) {
        health_checks++;
        
        // Get system statistics
        pico_rtos_system_stats_t system_stats;
        pico_rtos_get_system_stats(&system_stats);
        
        printf("[Health Monitor] Check #%lu: %lu tasks, %lu KB memory, %lu%% CPU\n",
               health_checks, system_stats.total_tasks, system_stats.current_memory / 1024,
               100 - (system_stats.idle_counter * 100 / pico_rtos_get_uptime_ms()));
        
        // Check for system health issues
        if (system_stats.current_memory > system_stats.peak_memory * 0.9f) {
            printf("[Health Monitor] WARNING: Memory usage approaching peak\n");
        }
        
        if (system_stats.blocked_tasks > system_stats.total_tasks / 2) {
            printf("[Health Monitor] WARNING: High number of blocked tasks\n");
        }
        
        pico_rtos_task_delay(5000);  // Health check every 5 seconds
    }
    
    printf("[Health Monitor] System health monitoring stopped after %lu checks\n", health_checks);
}

/**
 * @brief LED status indicator
 */
void led_status_task(void *param) {
    bool led_state = false;
    uint32_t blink_pattern = 0;
    
    while (system_running) {
        // Different blink patterns based on system state
        if (profiling_enabled && tracing_enabled) {
            // Fast double blink - full debugging active
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            pico_rtos_task_delay(100);
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            pico_rtos_task_delay(100);
            pico_rtos_task_delay(800);
        } else if (profiling_enabled || tracing_enabled) {
            // Single fast blink - partial debugging
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            pico_rtos_task_delay(200);
        } else {
            // Slow blink - normal operation
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            pico_rtos_task_delay(1000);
        }
    }
    
    // Turn off LED on shutdown
    gpio_put(LED_PIN, 0);
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Initialize GPIO for LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    printf("\n=== Pico-RTOS v0.3.0 Debugging and Profiling Real-Time Analysis Example ===\n");
    printf("This example demonstrates:\n");
    printf("1. Runtime task inspection and monitoring\n");
    printf("2. Execution time profiling for performance analysis\n");
    printf("3. System event tracing for behavior analysis\n");
    printf("4. Memory usage tracking and leak detection\n");
    printf("5. Real-time performance metrics collection\n");
    printf("6. Configurable assertion handling and error reporting\n\n");
    
    // Initialize RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize RTOS\n");
        return -1;
    }
    
    // Initialize profiling system
    printf("Initializing debugging and profiling systems...\n");
    // Initialize manual profiling data
    for (int i = 0; i < 10; i++) {
        function_performance[i].call_count = 0;
        function_performance[i].function_calls = 0;
        function_performance[i].total_execution_time = 0;
        function_performance[i].total_time_us = 0;
        function_performance[i].min_execution_time = 0;
        function_performance[i].min_time_us = 0;
        function_performance[i].max_execution_time = 0;
        function_performance[i].max_time_us = 0;
        function_performance[i].threshold_violations = 0;
        function_performance[i].entry_time = 0;
    }
    
    // Initialize memory pool for testing
    if (!pico_rtos_memory_pool_init(&test_memory_pool, pool_memory, 64, 32)) {
        printf("ERROR: Failed to initialize test memory pool\n");
        return -1;
    }
    
    // Initialize synchronization primitives
    if (!pico_rtos_mutex_init(&performance_mutex) ||
        !pico_rtos_mutex_init(&metrics_mutex)) {
        printf("ERROR: Failed to initialize mutexes\n");
        return -1;
    }
    
    // Enable manual profiling
    profiling_enabled = true;
    
    printf("Debugging and profiling systems initialized\n\n");
    
    // Create real-time control tasks
    printf("Creating real-time control tasks...\n");
    static pico_rtos_task_t rt_control1_task, rt_control2_task;
    
    if (!pico_rtos_task_create(&rt_control1_task, "RTControl1", realtime_control_task, (void*)1, 1024, 7) ||
        !pico_rtos_task_create(&rt_control2_task, "RTControl2", realtime_control_task, (void*)2, 1024, 7)) {
        printf("ERROR: Failed to create real-time control tasks\n");
        return -1;
    }
    
    // Create data logging tasks
    printf("Creating data logging tasks...\n");
    static pico_rtos_task_t logger1_task, logger2_task;
    
    if (!pico_rtos_task_create(&logger1_task, "Logger1", data_logging_task, (void*)1, 1024, 3) ||
        !pico_rtos_task_create(&logger2_task, "Logger2", data_logging_task, (void*)2, 1024, 3)) {
        printf("ERROR: Failed to create data logging tasks\n");
        return -1;
    }
    
    // Create communication tasks
    printf("Creating communication tasks...\n");
    static pico_rtos_task_t comm1_task, comm2_task;
    
    if (!pico_rtos_task_create(&comm1_task, "Comm1", communication_task, (void*)1, 1024, 4) ||
        !pico_rtos_task_create(&comm2_task, "Comm2", communication_task, (void*)2, 1024, 4)) {
        printf("ERROR: Failed to create communication tasks\n");
        return -1;
    }
    
    // Create debugging and monitoring tasks
    printf("Creating debugging and monitoring tasks...\n");
    static pico_rtos_task_t inspector_task, analyzer_task, health_task, led_task;
    
    if (!pico_rtos_task_create(&inspector_task, "Inspector", task_inspector_task, NULL, 1024, 1) ||
        !pico_rtos_task_create(&analyzer_task, "Analyzer", performance_analyzer_task, NULL, 1024, 1) ||
        !pico_rtos_task_create(&health_task, "HealthMon", system_health_monitor_task, NULL, 1024, 2) ||
        !pico_rtos_task_create(&led_task, "LED", led_status_task, NULL, 256, 1)) {
        printf("ERROR: Failed to create debugging and monitoring tasks\n");
        return -1;
    }
    
    printf("All tasks created successfully\n");
    printf("Profiling enabled: %s\n", profiling_enabled ? "YES" : "NO");
    printf("Tracing enabled: %s\n", tracing_enabled ? "YES" : "NO");
    printf("Debug mode: %s\n", debug_mode ? "YES" : "NO");
    printf("Starting scheduler...\n\n");
    
    // Start the RTOS scheduler
    pico_rtos_start();
    
    // Should never reach here
    printf("ERROR: Scheduler returned unexpectedly\n");
    return -1;
}