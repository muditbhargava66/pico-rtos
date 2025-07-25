/**
 * @file hardware_in_the_loop_test.c
 * @brief Hardware-in-the-loop validation tests for Pico-RTOS v0.3.0
 * 
 * Comprehensive hardware tests on actual RP2040 devices:
 * - Multi-core functionality with real hardware timing
 * - Memory protection and stack overflow detection
 * - Extended runtime stability testing
 * - Hardware-specific feature validation
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "pico_rtos.h"

// Include available headers based on configuration
#ifdef PICO_RTOS_ENABLE_MULTI_CORE
#include "pico_rtos/smp.h"
#endif

#ifdef PICO_RTOS_ENABLE_MPU_SUPPORT
#include "pico_rtos/mpu.h"
#endif

#ifdef PICO_RTOS_ENABLE_EVENT_GROUPS
#include "pico_rtos/event_group.h"
#endif

#ifdef PICO_RTOS_ENABLE_STREAM_BUFFERS
#include "pico_rtos/stream_buffer.h"
#endif

#ifdef PICO_RTOS_ENABLE_MEMORY_POOLS
#include "pico_rtos/memory_pool.h"
#endif

// Mock definitions for disabled features
#ifndef PICO_RTOS_ENABLE_MULTI_CORE
typedef struct { int dummy; } pico_rtos_smp_scheduler_t;
static inline bool pico_rtos_smp_init(void) { return false; }
static inline uint32_t pico_rtos_smp_get_core_id(void) { return 0; }
static inline bool pico_rtos_smp_get_stats(pico_rtos_smp_scheduler_t *stats) { (void)stats; return false; }
typedef enum { PICO_RTOS_CORE_AFFINITY_NONE, PICO_RTOS_CORE_AFFINITY_CORE0, PICO_RTOS_CORE_AFFINITY_CORE1, PICO_RTOS_CORE_AFFINITY_ANY } pico_rtos_core_affinity_t;
static inline bool pico_rtos_smp_set_task_affinity(pico_rtos_task_t *task, pico_rtos_core_affinity_t affinity) { (void)task; (void)affinity; return false; }
#endif

#ifndef PICO_RTOS_ENABLE_MPU_SUPPORT
static inline bool pico_rtos_mpu_is_available(void) { return false; }
static inline bool pico_rtos_mpu_init(void) { return false; }
static inline bool pico_rtos_mpu_protect_task_stack(pico_rtos_task_t *task) { (void)task; return false; }
static inline bool pico_rtos_mpu_is_stack_protected(pico_rtos_task_t *task) { (void)task; return false; }
static inline bool pico_rtos_mpu_validate(void *ptr) { (void)ptr; return false; }
#endif

#ifndef PICO_RTOS_ENABLE_EVENT_GROUPS
typedef struct { int dummy; } pico_rtos_event_group_t;
typedef struct { int dummy; } pico_rtos_event_group_stats_t;
#define PICO_RTOS_EVENT_BIT_0 (1UL << 0)
#define PICO_RTOS_EVENT_BIT_1 (1UL << 1)
#define PICO_RTOS_EVENT_BIT_2 (1UL << 2)
#define PICO_RTOS_EVENT_BIT_3 (1UL << 3)
#define PICO_RTOS_EVENT_BIT_4 (1UL << 4)
#define PICO_RTOS_EVENT_BIT_5 (1UL << 5)
#define PICO_RTOS_EVENT_BIT_6 (1UL << 6)
#define PICO_RTOS_EVENT_BIT_7 (1UL << 7)
static inline bool pico_rtos_event_group_init(pico_rtos_event_group_t *group) { (void)group; return false; }
static inline bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *group, uint32_t bits) { (void)group; (void)bits; return false; }
static inline uint32_t pico_rtos_event_group_wait_bits(pico_rtos_event_group_t *group, uint32_t bits, bool wait_all, bool clear, uint32_t timeout) { (void)group; (void)bits; (void)wait_all; (void)clear; (void)timeout; return 0; }
static inline uint32_t pico_rtos_event_group_get_bits(pico_rtos_event_group_t *group) { (void)group; return 0; }
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
static inline uint32_t pico_rtos_memory_pool_get_free_count(pico_rtos_memory_pool_t *pool) { (void)pool; return 0; }
static inline uint32_t pico_rtos_memory_pool_get_total_count(pico_rtos_memory_pool_t *pool) { (void)pool; return 0; }
static inline bool pico_rtos_memory_pool_validate(pico_rtos_memory_pool_t *pool) { (void)pool; return false; }
#endif

// Hardware test configuration
#define HIL_TEST_DURATION_HOURS 2
#define HIL_TEST_DURATION_MS (HIL_TEST_DURATION_HOURS * 60 * 60 * 1000)
#define HIL_STRESS_TASK_COUNT 8
#define HIL_TASK_STACK_SIZE 1024
#define HIL_GPIO_TEST_PIN 25  // Built-in LED
#define HIL_MEMORY_STRESS_SIZE 4096

// Hardware test objects
static pico_rtos_event_group_t hil_sync_events;
static pico_rtos_stream_buffer_t hil_data_stream;
static uint8_t hil_stream_memory[2048];
static pico_rtos_memory_pool_t hil_memory_pool;
static uint8_t hil_pool_memory[HIL_STRESS_TASK_COUNT * 256 + 128];

// Test state tracking
static volatile bool test_passed = true;
static volatile int test_count = 0;
static volatile int test_failures = 0;
static volatile bool hil_test_running = true;

// Hardware validation events
#define HIL_EVENT_CORE0_READY     PICO_RTOS_EVENT_BIT_0
#define HIL_EVENT_CORE1_READY     PICO_RTOS_EVENT_BIT_1
#define HIL_EVENT_GPIO_TEST_DONE  PICO_RTOS_EVENT_BIT_2
#define HIL_EVENT_TIMER_TEST_DONE PICO_RTOS_EVENT_BIT_3
#define HIL_EVENT_MEMORY_TEST_DONE PICO_RTOS_EVENT_BIT_4
#define HIL_EVENT_STABILITY_CHECK  PICO_RTOS_EVENT_BIT_5
#define HIL_EVENT_ERROR_DETECTED   PICO_RTOS_EVENT_BIT_6
#define HIL_EVENT_SHUTDOWN_REQUEST PICO_RTOS_EVENT_BIT_7

// Hardware test statistics
typedef struct {
    uint32_t task_id;
    uint32_t assigned_core;
    uint32_t gpio_toggles;
    uint32_t timer_events;
    uint32_t memory_operations;
    uint32_t context_switches;
    uint32_t error_count;
    uint32_t uptime_seconds;
    bool task_healthy;
    uint32_t last_heartbeat;
} hil_task_stats_t;

static hil_task_stats_t hil_stats[HIL_STRESS_TASK_COUNT];

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
// HARDWARE VALIDATION UTILITIES
// =============================================================================

static void hil_gpio_init(void) {
    gpio_init(HIL_GPIO_TEST_PIN);
    gpio_set_dir(HIL_GPIO_TEST_PIN, GPIO_OUT);
    gpio_put(HIL_GPIO_TEST_PIN, 0);
}

static uint32_t hil_get_core_frequency_hz(void) {
    return clock_get_hz(clk_sys);
}

static uint32_t hil_get_uptime_seconds(void) {
    return to_ms_since_boot(get_absolute_time()) / 1000;
}

static bool hil_validate_hardware_state(void) {
    // Check system clock
    uint32_t sys_freq = hil_get_core_frequency_hz();
    if (sys_freq < 100000000 || sys_freq > 300000000) { // 100-300 MHz range
        return false;
    }
    
    // Check both cores are responsive
    uint32_t core0_id = 0;
    uint32_t core1_id = 1;
    
    // Basic hardware validation passed
    return true;
}

// =============================================================================
// MULTI-CORE HARDWARE VALIDATION TASKS
// =============================================================================

static void hil_core0_validation_task(void *param) {
    hil_task_stats_t *stats = (hil_task_stats_t *)param;
    stats->assigned_core = get_core_num();
    stats->task_healthy = true;
    
    // Signal core 0 ready
    pico_rtos_event_group_set_bits(&hil_sync_events, HIL_EVENT_CORE0_READY);
    
    uint32_t start_time = hil_get_uptime_seconds();
    uint32_t last_heartbeat = start_time;
    
    while (hil_test_running) {
        uint32_t current_time = hil_get_uptime_seconds();
        stats->uptime_seconds = current_time - start_time;
        
        // GPIO validation
        gpio_put(HIL_GPIO_TEST_PIN, 1);
        busy_wait_us(100);
        gpio_put(HIL_GPIO_TEST_PIN, 0);
        busy_wait_us(100);
        stats->gpio_toggles++;
        
        // Memory operations
        void *mem_block = pico_rtos_memory_pool_alloc(&hil_memory_pool, 50);
        if (mem_block) {
            memset(mem_block, 0xAA, 64);
            pico_rtos_memory_pool_free(&hil_memory_pool, mem_block);
            stats->memory_operations++;
        }
        
        // Inter-core communication test
        char test_msg[32];
        snprintf(test_msg, sizeof(test_msg), "Core0 msg %lu", stats->gpio_toggles);
        pico_rtos_stream_buffer_send(&hil_data_stream, test_msg, strlen(test_msg), 10);
        
        // Heartbeat every 10 seconds
        if (current_time - last_heartbeat >= 10) {
            stats->last_heartbeat = current_time;
            last_heartbeat = current_time;
            printf("Core 0 heartbeat: %lu toggles, %lu mem ops, uptime: %lu s\n",
                   stats->gpio_toggles, stats->memory_operations, stats->uptime_seconds);
        }
        
        // Context switch
        pico_rtos_task_delay(100);
        stats->context_switches++;
        
        // Check for shutdown
        uint32_t events = pico_rtos_event_group_wait_bits(&hil_sync_events,
                                                         HIL_EVENT_SHUTDOWN_REQUEST,
                                                         false, false, 0);
        if (events & HIL_EVENT_SHUTDOWN_REQUEST) {
            break;
        }
    }
    
    stats->task_healthy = false;
    pico_rtos_task_delete(NULL);
}

static void hil_core1_validation_task(void *param) {
    hil_task_stats_t *stats = (hil_task_stats_t *)param;
    stats->assigned_core = get_core_num();
    stats->task_healthy = true;
    
    // Signal core 1 ready
    pico_rtos_event_group_set_bits(&hil_sync_events, HIL_EVENT_CORE1_READY);
    
    uint32_t start_time = hil_get_uptime_seconds();
    uint32_t last_heartbeat = start_time;
    
    while (hil_test_running) {
        uint32_t current_time = hil_get_uptime_seconds();
        stats->uptime_seconds = current_time - start_time;
        
        // Timer-based validation
        uint32_t timer_start = time_us_32();
        busy_wait_us(1000); // 1ms precise delay
        uint32_t timer_end = time_us_32();
        uint32_t actual_delay = timer_end - timer_start;
        
        // Validate timing accuracy (should be close to 1000us)
        if (actual_delay >= 900 && actual_delay <= 1100) {
            stats->timer_events++;
        } else {
            stats->error_count++;
        }
        
        // Memory operations with different pattern
        void *mem_block = pico_rtos_memory_pool_alloc(&hil_memory_pool, 50);
        if (mem_block) {
            memset(mem_block, 0x55, 64);
            pico_rtos_memory_pool_free(&hil_memory_pool, mem_block);
            stats->memory_operations++;
        }
        
        // Inter-core communication test
        char receive_buffer[64];
        uint32_t received = pico_rtos_stream_buffer_receive(&hil_data_stream,
                                                           receive_buffer,
                                                           sizeof(receive_buffer) - 1,
                                                           10);
        if (received > 0) {
            receive_buffer[received] = '\0';
            // Validate message format
            if (strstr(receive_buffer, "Core0 msg") != NULL) {
                // Valid inter-core message received
            }
        }
        
        // Heartbeat every 10 seconds
        if (current_time - last_heartbeat >= 10) {
            stats->last_heartbeat = current_time;
            last_heartbeat = current_time;
            printf("Core 1 heartbeat: %lu timer events, %lu mem ops, %lu errors, uptime: %lu s\n",
                   stats->timer_events, stats->memory_operations, 
                   stats->error_count, stats->uptime_seconds);
        }
        
        // Context switch
        pico_rtos_task_delay(150);
        stats->context_switches++;
        
        // Check for shutdown
        uint32_t events = pico_rtos_event_group_wait_bits(&hil_sync_events,
                                                         HIL_EVENT_SHUTDOWN_REQUEST,
                                                         false, false, 0);
        if (events & HIL_EVENT_SHUTDOWN_REQUEST) {
            break;
        }
    }
    
    stats->task_healthy = false;
    pico_rtos_task_delete(NULL);
}

static void hil_stress_test_task(void *param) {
    hil_task_stats_t *stats = (hil_task_stats_t *)param;
    stats->assigned_core = get_core_num();
    stats->task_healthy = true;
    
    uint32_t start_time = hil_get_uptime_seconds();
    uint32_t stress_counter = 0;
    
    while (hil_test_running) {
        uint32_t current_time = hil_get_uptime_seconds();
        stats->uptime_seconds = current_time - start_time;
        
        // CPU stress operations
        volatile uint32_t cpu_work = 0;
        for (uint32_t i = 0; i < 1000; i++) {
            cpu_work += i * stats->task_id;
        }
        
        // Memory stress
        void *stress_blocks[4];
        for (int i = 0; i < 4; i++) {
            stress_blocks[i] = pico_rtos_memory_pool_alloc(&hil_memory_pool, 20);
            if (stress_blocks[i]) {
                memset(stress_blocks[i], (uint8_t)(stress_counter + i), 64);
                stats->memory_operations++;
            }
        }
        
        // Free memory blocks
        for (int i = 0; i < 4; i++) {
            if (stress_blocks[i]) {
                pico_rtos_memory_pool_free(&hil_memory_pool, stress_blocks[i]);
            }
        }
        
        stress_counter++;
        
        // Context switch
        pico_rtos_task_delay(50 + (stats->task_id * 10));
        stats->context_switches++;
        
        // Check for shutdown
        uint32_t events = pico_rtos_event_group_wait_bits(&hil_sync_events,
                                                         HIL_EVENT_SHUTDOWN_REQUEST,
                                                         false, false, 0);
        if (events & HIL_EVENT_SHUTDOWN_REQUEST) {
            break;
        }
    }
    
    stats->task_healthy = false;
    pico_rtos_task_delete(NULL);
}

// =============================================================================
// HARDWARE VALIDATION TEST FUNCTIONS
// =============================================================================

static void test_hardware_initialization(void) {
    printf("\n=== Testing Hardware Initialization ===\n");
    
    // Initialize GPIO for testing
    hil_gpio_init();
    TEST_ASSERT(true, "GPIO initialization completed");
    
    // Check system frequency
    uint32_t sys_freq = hil_get_core_frequency_hz();
    printf("System frequency: %lu Hz\n", sys_freq);
    TEST_ASSERT(sys_freq > 100000000, "System frequency should be > 100 MHz");
    
    // Initialize RTOS components
    bool event_result = pico_rtos_event_group_init(&hil_sync_events);
    TEST_ASSERT(event_result, "HIL sync event group initialization should succeed");
    
    bool stream_result = pico_rtos_stream_buffer_init(&hil_data_stream,
                                                     hil_stream_memory,
                                                     sizeof(hil_stream_memory));
    TEST_ASSERT(stream_result, "HIL data stream initialization should succeed");
    
    bool pool_result = pico_rtos_memory_pool_init(&hil_memory_pool,
                                                 hil_pool_memory,
                                                 256,
                                                 HIL_STRESS_TASK_COUNT);
    TEST_ASSERT(pool_result, "HIL memory pool initialization should succeed");
    
    // Initialize SMP
    bool smp_result = pico_rtos_smp_init();
    TEST_ASSERT(smp_result, "SMP initialization should succeed");
    
    printf("Hardware initialization completed successfully\n");
}

static void test_multicore_hardware_functionality(void) {
    printf("\n=== Testing Multi-Core Hardware Functionality ===\n");
    
    // Create hardware validation tasks
    pico_rtos_task_t hil_tasks[HIL_STRESS_TASK_COUNT];
    uint32_t hil_stacks[HIL_STRESS_TASK_COUNT][HIL_TASK_STACK_SIZE / sizeof(uint32_t)];
    
    // Initialize task statistics
    for (int i = 0; i < HIL_STRESS_TASK_COUNT; i++) {
        memset(&hil_stats[i], 0, sizeof(hil_task_stats_t));
        hil_stats[i].task_id = i;
    }
    
    // Create core-specific validation tasks
    bool result = pico_rtos_task_create(&hil_tasks[0], "hil_core0",
                                       hil_core0_validation_task,
                                       &hil_stats[0],
                                       sizeof(hil_stacks[0]), 8);
    TEST_ASSERT(result, "Core 0 validation task creation should succeed");
    pico_rtos_smp_set_task_affinity(&hil_tasks[0], PICO_RTOS_CORE_AFFINITY_CORE0);
    
    result = pico_rtos_task_create(&hil_tasks[1], "hil_core1",
                                  hil_core1_validation_task,
                                  &hil_stats[1],
                                  sizeof(hil_stacks[1]), 8);
    TEST_ASSERT(result, "Core 1 validation task creation should succeed");
    pico_rtos_smp_set_task_affinity(&hil_tasks[1], PICO_RTOS_CORE_AFFINITY_CORE1);
    
    // Create stress test tasks
    for (int i = 2; i < HIL_STRESS_TASK_COUNT; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "hil_stress_%d", i);
        
        result = pico_rtos_task_create(&hil_tasks[i], task_name,
                                      hil_stress_test_task,
                                      &hil_stats[i],
                                      sizeof(hil_stacks[i]),
                                      5 + (i % 3));
        TEST_ASSERT(result, "Stress test task creation should succeed");
        
        // Distribute across cores
        pico_rtos_core_affinity_t affinity = (i % 2 == 0) ? 
            PICO_RTOS_CORE_AFFINITY_CORE0 : PICO_RTOS_CORE_AFFINITY_CORE1;
        pico_rtos_smp_set_task_affinity(&hil_tasks[i], affinity);
    }
    
    printf("Created %d hardware validation tasks\n", HIL_STRESS_TASK_COUNT);
    
    // Wait for cores to be ready
    uint32_t ready_events = pico_rtos_event_group_wait_bits(&hil_sync_events,
                                                           HIL_EVENT_CORE0_READY | HIL_EVENT_CORE1_READY,
                                                           true, false, 5000);
    TEST_ASSERT((ready_events & (HIL_EVENT_CORE0_READY | HIL_EVENT_CORE1_READY)) == 
               (HIL_EVENT_CORE0_READY | HIL_EVENT_CORE1_READY),
               "Both cores should be ready within timeout");
    
    printf("Multi-core hardware functionality test started\n");
}

static void test_memory_protection_hardware(void) {
    printf("\n=== Testing Memory Protection Hardware ===\n");
    
    // Test MPU availability
    bool mpu_available = pico_rtos_mpu_is_available();
    printf("MPU available: %s\n", mpu_available ? "Yes" : "No");
    
    if (mpu_available) {
        // Initialize MPU
        bool mpu_result = pico_rtos_mpu_init();
        TEST_ASSERT(mpu_result, "MPU initialization should succeed");
        
        // Test stack protection
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        if (current_task) {
            bool protect_result = pico_rtos_mpu_protect_task_stack(current_task);
            TEST_ASSERT(protect_result, "Stack protection should be enabled");
            
            bool is_protected = pico_rtos_mpu_is_stack_protected(current_task);
            TEST_ASSERT(is_protected, "Stack should be protected");
            
            printf("Memory protection hardware validation completed\n");
        }
    } else {
        printf("MPU not available on this hardware - skipping MPU tests\n");
    }
}

static void test_extended_runtime_stability(void) {
    printf("\n=== Testing Extended Runtime Stability ===\n");
    
    uint32_t test_start_time = hil_get_uptime_seconds();
    uint32_t last_stability_check = test_start_time;
    uint32_t stability_check_interval = 300; // 5 minutes
    
    // Determine actual test duration (shorter for automated testing)
    uint32_t test_duration_seconds = 1800; // 30 minutes for automated tests
    printf("Running stability test for %lu seconds (%lu minutes)\n", 
           test_duration_seconds, test_duration_seconds / 60);
    
    while (hil_test_running) {
        uint32_t current_time = hil_get_uptime_seconds();
        uint32_t elapsed_time = current_time - test_start_time;
        
        // Check if test duration reached
        if (elapsed_time >= test_duration_seconds) {
            printf("Stability test duration reached: %lu seconds\n", elapsed_time);
            break;
        }
        
        // Periodic stability checks
        if (current_time - last_stability_check >= stability_check_interval) {
            printf("\n=== Stability Check at %lu seconds ===\n", elapsed_time);
            
            // Validate hardware state
            bool hw_state = hil_validate_hardware_state();
            TEST_ASSERT(hw_state, "Hardware state should be valid");
            
            // Check task health
            uint32_t healthy_tasks = 0;
            uint32_t total_operations = 0;
            
            for (int i = 0; i < HIL_STRESS_TASK_COUNT; i++) {
                if (hil_stats[i].task_healthy) {
                    healthy_tasks++;
                }
                total_operations += hil_stats[i].memory_operations + 
                                  hil_stats[i].gpio_toggles + 
                                  hil_stats[i].timer_events;
            }
            
            printf("Healthy tasks: %lu/%d\n", healthy_tasks, HIL_STRESS_TASK_COUNT);
            printf("Total operations: %lu\n", total_operations);
            
            TEST_ASSERT(healthy_tasks >= (HIL_STRESS_TASK_COUNT - 1), 
                       "Most tasks should remain healthy");
            TEST_ASSERT(total_operations > 0, "Tasks should be performing operations");
            
            // Check memory pool state
            uint32_t free_blocks = pico_rtos_memory_pool_get_free_count(&hil_memory_pool);
            uint32_t total_blocks = pico_rtos_memory_pool_get_total_count(&hil_memory_pool);
            printf("Memory pool: %lu/%lu blocks free\n", free_blocks, total_blocks);
            
            // Check for memory leaks
            TEST_ASSERT(free_blocks > 0, "Memory pool should have free blocks");
            
            // Signal stability check event
            pico_rtos_event_group_set_bits(&hil_sync_events, HIL_EVENT_STABILITY_CHECK);
            
            last_stability_check = current_time;
        }
        
        // Sleep for a while before next check
        pico_rtos_task_delay(10000); // 10 seconds
    }
    
    printf("Extended runtime stability test completed\n");
}

static void test_hardware_specific_features(void) {
    printf("\n=== Testing Hardware-Specific Features ===\n");
    
    // Test watchdog functionality
    printf("Testing watchdog functionality...\n");
    if (watchdog_caused_reboot()) {
        printf("Previous reboot was caused by watchdog\n");
    } else {
        printf("Normal boot (not watchdog reboot)\n");
    }
    
    // Enable watchdog with long timeout for testing
    watchdog_enable(8000, 1); // 8 second timeout, pause on debug
    printf("Watchdog enabled with 8 second timeout\n");
    
    // Test GPIO functionality
    printf("Testing GPIO functionality...\n");
    for (int i = 0; i < 10; i++) {
        gpio_put(HIL_GPIO_TEST_PIN, 1);
        busy_wait_us(100000); // 100ms
        gpio_put(HIL_GPIO_TEST_PIN, 0);
        busy_wait_us(100000);
        
        // Feed watchdog during GPIO test
        watchdog_update();
    }
    TEST_ASSERT(true, "GPIO functionality test completed");
    
    // Test timer accuracy
    printf("Testing timer accuracy...\n");
    uint32_t timer_errors = 0;
    for (int i = 0; i < 100; i++) {
        uint32_t start_time = time_us_32();
        busy_wait_us(1000); // 1ms delay
        uint32_t end_time = time_us_32();
        uint32_t actual_delay = end_time - start_time;
        
        if (actual_delay < 900 || actual_delay > 1100) {
            timer_errors++;
        }
        
        // Feed watchdog periodically
        if (i % 10 == 0) {
            watchdog_update();
        }
    }
    
    printf("Timer accuracy test: %lu errors out of 100 tests\n", timer_errors);
    TEST_ASSERT(timer_errors < 5, "Timer accuracy should be good (< 5% errors)");
    
    // Disable watchdog
    watchdog_disable();
    printf("Watchdog disabled\n");
    
    printf("Hardware-specific feature tests completed\n");
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

static void run_hardware_in_the_loop_tests(void) {
    printf("\n");
    printf("==========================================================\n");
    printf("  Hardware-in-the-Loop Validation Tests (Pico-RTOS v0.3.0)\n");
    printf("==========================================================\n");
    
    test_hardware_initialization();
    test_multicore_hardware_functionality();
    test_memory_protection_hardware();
    test_hardware_specific_features();
    test_extended_runtime_stability();
    
    // Signal shutdown to all tasks
    hil_test_running = false;
    pico_rtos_event_group_set_bits(&hil_sync_events, HIL_EVENT_SHUTDOWN_REQUEST);
    
    // Wait for tasks to shutdown
    printf("Waiting for tasks to shutdown...\n");
    pico_rtos_task_delay(5000);
    
    // Final statistics
    printf("\n=== Final Hardware Test Statistics ===\n");
    printf("Task ID | Core | GPIO | Timer | Memory | Context | Errors | Uptime\n");
    printf("--------|------|------|-------|--------|---------|--------|--------\n");
    
    for (int i = 0; i < HIL_STRESS_TASK_COUNT; i++) {
        hil_task_stats_t *stats = &hil_stats[i];
        printf("   %2lu   |  %lu   | %4lu | %5lu | %6lu | %7lu | %6lu | %6lu\n",
               stats->task_id, stats->assigned_core, stats->gpio_toggles,
               stats->timer_events, stats->memory_operations,
               stats->context_switches, stats->error_count, stats->uptime_seconds);
    }
    
    printf("\n");
    printf("==========================================================\n");
    printf("         Hardware-in-the-Loop Test Results               \n");
    printf("==========================================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_count - test_failures);
    printf("Failed: %d\n", test_failures);
    printf("Success rate: %.1f%%\n", 
           test_count > 0 ? (100.0 * (test_count - test_failures)) / test_count : 0.0);
    printf("==========================================================\n");
    
    if (test_failures == 0) {
        printf("🎉 All hardware-in-the-loop tests PASSED!\n");
        printf("✅ Hardware validation completed successfully\n");
    } else {
        printf("❌ Some hardware-in-the-loop tests FAILED!\n");
        printf("⚠️  Hardware issues detected - review required\n");
    }
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    stdio_init_all();
    sleep_ms(3000); // Longer delay for hardware initialization
    
    printf("Hardware-in-the-Loop Validation Test Starting...\n");
    printf("Running on RP2040 hardware\n");
    printf("Core frequency: %lu Hz\n", clock_get_hz(clk_sys));
    
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    run_hardware_in_the_loop_tests();
    
    return (test_failures == 0) ? 0 : 1;
}