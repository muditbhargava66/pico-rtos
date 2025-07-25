/**
 * @file multicore_comprehensive_test.c
 * @brief Comprehensive multi-core tests for all v0.3.0 synchronization primitives
 * 
 * Tests multi-core task distribution, synchronization, and coordination
 * across event groups, stream buffers, memory pools, and SMP features.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
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

// Mock definitions for disabled features
#ifndef PICO_RTOS_ENABLE_EVENT_GROUPS
typedef struct { int dummy; } pico_rtos_event_group_t;
static inline bool pico_rtos_event_group_init(pico_rtos_event_group_t *group) { (void)group; return false; }
static inline bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *group, uint32_t bits) { (void)group; (void)bits; return false; }
static inline uint32_t pico_rtos_event_group_get_bits(pico_rtos_event_group_t *group) { (void)group; return 0; }
#endif

#ifndef PICO_RTOS_ENABLE_STREAM_BUFFERS
typedef struct { int dummy; } pico_rtos_stream_buffer_t;
static inline bool pico_rtos_stream_buffer_init(pico_rtos_stream_buffer_t *buffer, uint8_t *mem, size_t size) { (void)buffer; (void)mem; (void)size; return false; }
static inline uint32_t pico_rtos_stream_buffer_send(pico_rtos_stream_buffer_t *buffer, const void *data, size_t size, uint32_t timeout) { (void)buffer; (void)data; (void)size; (void)timeout; return 0; }
#endif

#ifndef PICO_RTOS_ENABLE_MEMORY_POOLS
typedef struct { int dummy; } pico_rtos_memory_pool_t;
static inline bool pico_rtos_memory_pool_init(pico_rtos_memory_pool_t *pool, uint8_t *mem, size_t block_size, size_t count) { (void)pool; (void)mem; (void)block_size; (void)count; return false; }
static inline void* pico_rtos_memory_pool_alloc(pico_rtos_memory_pool_t *pool, uint32_t timeout) { (void)pool; (void)timeout; return NULL; }
static inline bool pico_rtos_memory_pool_free(pico_rtos_memory_pool_t *pool, void *ptr) { (void)pool; (void)ptr; return false; }
#endif

#ifndef PICO_RTOS_ENABLE_MULTI_CORE
static inline bool pico_rtos_smp_init(void) { return false; }
static inline uint32_t pico_rtos_smp_get_core_id(void) { return 0; }
typedef enum { PICO_RTOS_CORE_AFFINITY_NONE, PICO_RTOS_CORE_AFFINITY_CORE0, PICO_RTOS_CORE_AFFINITY_CORE1, PICO_RTOS_CORE_AFFINITY_ANY } pico_rtos_core_affinity_t;
static inline bool pico_rtos_smp_set_task_affinity(pico_rtos_task_t *task, pico_rtos_core_affinity_t affinity) { (void)task; (void)affinity; return false; }
#endif

// Test configuration
#define TEST_TASK_STACK_SIZE 1024
#define NUM_TEST_TASKS 6
#define MULTICORE_TEST_ITERATIONS 100
#define STREAM_BUFFER_SIZE 512
#define MEMORY_POOL_BLOCKS 20
#define MEMORY_POOL_BLOCK_SIZE 64

// Test synchronization objects
static pico_rtos_event_group_t multicore_event_group;
static pico_rtos_stream_buffer_t multicore_stream_buffer;
static uint8_t stream_buffer_memory[STREAM_BUFFER_SIZE];
static pico_rtos_memory_pool_t multicore_memory_pool;
static uint8_t memory_pool_memory[MEMORY_POOL_BLOCKS * MEMORY_POOL_BLOCK_SIZE + 128];

// Test state tracking
static volatile bool test_passed = true;
static volatile int test_count = 0;
static volatile int test_failures = 0;

// Task coordination
typedef struct {
    uint32_t task_id;
    uint32_t assigned_core;
    uint32_t operations_completed;
    uint32_t events_processed;
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t memory_allocations;
    bool task_completed;
    uint32_t start_time;
    uint32_t end_time;
} multicore_task_stats_t;

static multicore_task_stats_t task_stats[NUM_TEST_TASKS];

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
// MULTI-CORE TASK FUNCTIONS
// =============================================================================

static void multicore_producer_task(void *param) {
    multicore_task_stats_t *stats = (multicore_task_stats_t *)param;
    stats->assigned_core = pico_rtos_smp_get_core_id();
    stats->start_time = pico_rtos_get_tick_count();
    
    char message_buffer[64];
    
    for (uint32_t i = 0; i < MULTICORE_TEST_ITERATIONS; i++) {
        // Send message via stream buffer
        snprintf(message_buffer, sizeof(message_buffer), 
                "Task %lu message %lu from core %lu", 
                stats->task_id, i, stats->assigned_core);
        
        uint32_t sent = pico_rtos_stream_buffer_send(&multicore_stream_buffer,
                                                    message_buffer,
                                                    strlen(message_buffer),
                                                    100);
        if (sent > 0) {
            stats->messages_sent++;
        }
        
        // Set event to notify consumers
        if (pico_rtos_event_group_set_bits(&multicore_event_group, 
                                          (1UL << stats->task_id))) {
            stats->events_processed++;
        }
        
        // Allocate and free memory
        void *mem_block = pico_rtos_memory_pool_alloc(&multicore_memory_pool, 10);
        if (mem_block) {
            stats->memory_allocations++;
            // Use the memory briefly
            memset(mem_block, (uint8_t)stats->task_id, MEMORY_POOL_BLOCK_SIZE);
            pico_rtos_memory_pool_free(&multicore_memory_pool, mem_block);
        }
        
        stats->operations_completed++;
        pico_rtos_task_delay(5); // Small delay to allow other tasks
    }
    
    stats->end_time = pico_rtos_get_tick_count();
    stats->task_completed = true;
    pico_rtos_task_delete(NULL);
}

static void multicore_consumer_task(void *param) {
    multicore_task_stats_t *stats = (multicore_task_stats_t *)param;
    stats->assigned_core = pico_rtos_smp_get_core_id();
    stats->start_time = pico_rtos_get_tick_count();
    
    char receive_buffer[128];
    
    for (uint32_t i = 0; i < MULTICORE_TEST_ITERATIONS; i++) {
        // Wait for event from producers
        uint32_t events = pico_rtos_event_group_wait_bits(&multicore_event_group,
                                                         0xFFFFFFFF, // Any event
                                                         false, // Don't wait for all
                                                         true,  // Clear on exit
                                                         50);   // Timeout
        if (events > 0) {
            stats->events_processed++;
        }
        
        // Try to receive message
        uint32_t received = pico_rtos_stream_buffer_receive(&multicore_stream_buffer,
                                                           receive_buffer,
                                                           sizeof(receive_buffer) - 1,
                                                           10);
        if (received > 0) {
            receive_buffer[received] = '\0';
            stats->messages_received++;
        }
        
        // Allocate and free memory
        void *mem_block = pico_rtos_memory_pool_alloc(&multicore_memory_pool, 10);
        if (mem_block) {
            stats->memory_allocations++;
            // Verify memory content if possible
            pico_rtos_memory_pool_free(&multicore_memory_pool, mem_block);
        }
        
        stats->operations_completed++;
        pico_rtos_task_delay(7); // Different delay pattern
    }
    
    stats->end_time = pico_rtos_get_tick_count();
    stats->task_completed = true;
    pico_rtos_task_delete(NULL);
}

// =============================================================================
// MULTI-CORE TEST SETUP AND EXECUTION
// =============================================================================

static void test_multicore_initialization(void) {
    printf("\n=== Testing Multi-Core Initialization ===\n");
    
    // Initialize SMP
    bool smp_result = pico_rtos_smp_init();
    TEST_ASSERT(smp_result, "SMP initialization should succeed");
    
    // Initialize synchronization objects
    bool event_result = pico_rtos_event_group_init(&multicore_event_group);
    TEST_ASSERT(event_result, "Multi-core event group initialization should succeed");
    
    bool stream_result = pico_rtos_stream_buffer_init(&multicore_stream_buffer,
                                                     stream_buffer_memory,
                                                     sizeof(stream_buffer_memory));
    TEST_ASSERT(stream_result, "Multi-core stream buffer initialization should succeed");
    
    bool pool_result = pico_rtos_memory_pool_init(&multicore_memory_pool,
                                                 memory_pool_memory,
                                                 MEMORY_POOL_BLOCK_SIZE,
                                                 MEMORY_POOL_BLOCKS);
    TEST_ASSERT(pool_result, "Multi-core memory pool initialization should succeed");
    
    // Configure SMP settings
    pico_rtos_smp_set_assignment_strategy(PICO_RTOS_ASSIGN_LEAST_LOADED);
    pico_rtos_smp_set_load_balancing(true);
    pico_rtos_smp_set_load_balance_threshold(25);
    
    printf("Multi-core initialization completed successfully\n");
}

static void test_multicore_task_distribution(void) {
    printf("\n=== Testing Multi-Core Task Distribution ===\n");
    
    pico_rtos_task_t tasks[NUM_TEST_TASKS];
    uint32_t task_stacks[NUM_TEST_TASKS][TEST_TASK_STACK_SIZE / sizeof(uint32_t)];
    
    // Initialize task statistics
    for (int i = 0; i < NUM_TEST_TASKS; i++) {
        memset(&task_stats[i], 0, sizeof(multicore_task_stats_t));
        task_stats[i].task_id = i;
    }
    
    // Create producer tasks (first half)
    for (int i = 0; i < NUM_TEST_TASKS / 2; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "producer_%d", i);
        
        bool result = pico_rtos_task_create(&tasks[i], task_name,
                                           multicore_producer_task,
                                           &task_stats[i],
                                           sizeof(task_stacks[i]),
                                           5 + i); // Different priorities
        TEST_ASSERT(result, "Producer task creation should succeed");
        
        // Set task affinity to distribute across cores
        pico_rtos_core_affinity_t affinity = (i % 2 == 0) ? 
            PICO_RTOS_CORE_AFFINITY_CORE0 : PICO_RTOS_CORE_AFFINITY_CORE1;
        pico_rtos_smp_set_task_affinity(&tasks[i], affinity);
    }
    
    // Create consumer tasks (second half)
    for (int i = NUM_TEST_TASKS / 2; i < NUM_TEST_TASKS; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "consumer_%d", i);
        
        bool result = pico_rtos_task_create(&tasks[i], task_name,
                                           multicore_consumer_task,
                                           &task_stats[i],
                                           sizeof(task_stacks[i]),
                                           3 + i); // Different priorities
        TEST_ASSERT(result, "Consumer task creation should succeed");
        
        // Set task affinity to distribute across cores
        pico_rtos_core_affinity_t affinity = (i % 2 == 0) ? 
            PICO_RTOS_CORE_AFFINITY_CORE1 : PICO_RTOS_CORE_AFFINITY_CORE0;
        pico_rtos_smp_set_task_affinity(&tasks[i], affinity);
    }
    
    printf("Created %d multi-core tasks with distributed affinities\n", NUM_TEST_TASKS);
}

static void test_multicore_execution_and_coordination(void) {
    printf("\n=== Testing Multi-Core Execution and Coordination ===\n");
    
    uint32_t test_start_time = pico_rtos_get_tick_count();
    
    // Wait for all tasks to complete
    bool all_completed = false;
    uint32_t timeout_count = 0;
    const uint32_t max_timeout = 10000; // 10 seconds
    
    while (!all_completed && timeout_count < max_timeout) {
        all_completed = true;
        for (int i = 0; i < NUM_TEST_TASKS; i++) {
            if (!task_stats[i].task_completed) {
                all_completed = false;
                break;
            }
        }
        pico_rtos_task_delay(10);
        timeout_count += 10;
    }
    
    uint32_t test_end_time = pico_rtos_get_tick_count();
    uint32_t total_test_time = test_end_time - test_start_time;
    
    TEST_ASSERT(all_completed, "All multi-core tasks should complete within timeout");
    TEST_ASSERT(timeout_count < max_timeout, "Tasks should complete before timeout");
    
    printf("Multi-core test completed in %lu ticks\n", total_test_time);
    
    // Analyze task distribution and performance
    uint32_t core0_tasks = 0, core1_tasks = 0;
    uint32_t total_operations = 0;
    uint32_t total_events = 0;
    uint32_t total_messages_sent = 0;
    uint32_t total_messages_received = 0;
    uint32_t total_memory_ops = 0;
    
    printf("\nTask Performance Analysis:\n");
    printf("Task ID | Core | Operations | Events | Msg Sent | Msg Recv | Mem Ops | Duration\n");
    printf("--------|------|------------|--------|----------|----------|---------|----------\n");
    
    for (int i = 0; i < NUM_TEST_TASKS; i++) {
        multicore_task_stats_t *stats = &task_stats[i];
        uint32_t duration = stats->end_time - stats->start_time;
        
        printf("   %2lu   |  %lu   |    %4lu    |  %4lu  |   %4lu   |   %4lu   |  %4lu   |   %4lu\n",
               stats->task_id, stats->assigned_core, stats->operations_completed,
               stats->events_processed, stats->messages_sent, stats->messages_received,
               stats->memory_allocations, duration);
        
        if (stats->assigned_core == 0) core0_tasks++;
        else if (stats->assigned_core == 1) core1_tasks++;
        
        total_operations += stats->operations_completed;
        total_events += stats->events_processed;
        total_messages_sent += stats->messages_sent;
        total_messages_received += stats->messages_received;
        total_memory_ops += stats->memory_allocations;
    }
    
    printf("\nDistribution Summary:\n");
    printf("Core 0 tasks: %lu, Core 1 tasks: %lu\n", core0_tasks, core1_tasks);
    printf("Total operations: %lu\n", total_operations);
    printf("Total events: %lu\n", total_events);
    printf("Messages sent: %lu, received: %lu\n", total_messages_sent, total_messages_received);
    printf("Memory operations: %lu\n", total_memory_ops);
    
    // Validate results
    TEST_ASSERT(core0_tasks > 0 && core1_tasks > 0, "Tasks should be distributed across both cores");
    TEST_ASSERT(total_operations > 0, "Tasks should complete operations");
    TEST_ASSERT(total_events > 0, "Event group operations should occur");
    TEST_ASSERT(total_messages_sent > 0, "Stream buffer messages should be sent");
    TEST_ASSERT(total_memory_ops > 0, "Memory pool operations should occur");
}

static void test_multicore_load_balancing(void) {
    printf("\n=== Testing Multi-Core Load Balancing ===\n");
    
    // Get load balancing statistics
    uint32_t total_migrations = 0;
    uint32_t last_balance_time = 0;
    uint32_t balance_cycles = 0;
    
    bool stats_result = pico_rtos_smp_get_load_balance_stats(&total_migrations,
                                                            &last_balance_time,
                                                            &balance_cycles);
    TEST_ASSERT(stats_result, "Should get load balance statistics");
    
    printf("Load balancing stats: %lu migrations, %lu balance cycles\n",
           total_migrations, balance_cycles);
    
    // Get core load information
    uint32_t core0_load = pico_rtos_smp_get_core_load(0);
    uint32_t core1_load = pico_rtos_smp_get_core_load(1);
    
    printf("Core loads: Core 0: %lu%%, Core 1: %lu%%\n", core0_load, core1_load);
    
    TEST_ASSERT(core0_load <= 100, "Core 0 load should be valid percentage");
    TEST_ASSERT(core1_load <= 100, "Core 1 load should be valid percentage");
    
    // Test manual load balancing
    uint32_t manual_migrations = pico_rtos_smp_balance_load();
    printf("Manual load balancing resulted in %lu migrations\n", manual_migrations);
}

static void test_multicore_synchronization_integrity(void) {
    printf("\n=== Testing Multi-Core Synchronization Integrity ===\n");
    
    // Check final state of synchronization objects
    uint32_t final_events = pico_rtos_event_group_get_bits(&multicore_event_group);
    printf("Final event group state: 0x%08lx\n", final_events);
    
    // Check stream buffer state
    bool stream_empty = pico_rtos_stream_buffer_is_empty(&multicore_stream_buffer);
    uint32_t available_bytes = pico_rtos_stream_buffer_bytes_available(&multicore_stream_buffer);
    printf("Stream buffer: empty=%s, available bytes=%lu\n", 
           stream_empty ? "true" : "false", available_bytes);
    
    // Check memory pool state
    uint32_t free_blocks = pico_rtos_memory_pool_get_free_count(&multicore_memory_pool);
    uint32_t allocated_blocks = pico_rtos_memory_pool_get_allocated_count(&multicore_memory_pool);
    printf("Memory pool: free=%lu, allocated=%lu\n", free_blocks, allocated_blocks);
    
    // Validate integrity
    TEST_ASSERT(allocated_blocks == 0, "All memory blocks should be freed after test");
    
    // Get and display statistics
    pico_rtos_event_group_stats_t event_stats;
    if (pico_rtos_event_group_get_stats(&multicore_event_group, &event_stats)) {
        printf("Event group stats: %lu set ops, %lu clear ops\n",
               event_stats.total_set_operations, event_stats.total_clear_operations);
    }
    
    pico_rtos_stream_buffer_stats_t stream_stats;
    if (pico_rtos_stream_buffer_get_stats(&multicore_stream_buffer, &stream_stats)) {
        printf("Stream buffer stats: %lu messages sent, %lu received\n",
               stream_stats.messages_sent, stream_stats.messages_received);
    }
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

static void run_multicore_comprehensive_tests(void) {
    printf("\n");
    printf("====================================================\n");
    printf("  Multi-Core Comprehensive Tests (Pico-RTOS v0.3.0)\n");
    printf("====================================================\n");
    
    test_multicore_initialization();
    test_multicore_task_distribution();
    test_multicore_execution_and_coordination();
    test_multicore_load_balancing();
    test_multicore_synchronization_integrity();
    
    printf("\n");
    printf("====================================================\n");
    printf("         Multi-Core Test Results                    \n");
    printf("====================================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_count - test_failures);
    printf("Failed: %d\n", test_failures);
    printf("Success rate: %.1f%%\n", 
           test_count > 0 ? (100.0 * (test_count - test_failures)) / test_count : 0.0);
    printf("====================================================\n");
    
    if (test_failures == 0) {
        printf("🎉 All multi-core comprehensive tests PASSED!\n");
    } else {
        printf("❌ Some multi-core tests FAILED!\n");
    }
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("Multi-Core Comprehensive Test Starting...\n");
    
#ifdef PICO_RTOS_ENABLE_MULTI_CORE
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    run_multicore_comprehensive_tests();
    
    return (test_failures == 0) ? 0 : 1;
#else
    printf("Multi-core support is not enabled in this build configuration.\n");
    printf("Enable PICO_RTOS_ENABLE_MULTI_CORE to run this test.\n");
    return 0;
#endif
}