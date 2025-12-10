/**
 * @file integration_component_interactions_test.c
 * @brief Integration tests for component interactions in Pico-RTOS v0.3.1
 * 
 * Tests complex scenarios combining:
 * - Event groups with multi-core task coordination
 * - Stream buffers with memory pools
 * - Profiling and tracing with all synchronization primitives
 * - Cross-component error handling and recovery
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

#ifdef PICO_RTOS_ENABLE_EXECUTION_PROFILING
#include "pico_rtos/profiler.h"
#endif

#ifdef PICO_RTOS_ENABLE_SYSTEM_TRACING
#include "pico_rtos/trace.h"
#endif

// Mock definitions for disabled features
#ifndef PICO_RTOS_ENABLE_EVENT_GROUPS
typedef struct { int dummy; } pico_rtos_event_group_t;
typedef struct { int dummy; } pico_rtos_event_group_stats_t;
#define PICO_RTOS_EVENT_BIT_0 (1UL << 0)
#define PICO_RTOS_EVENT_BIT_1 (1UL << 1)
#define PICO_RTOS_EVENT_BIT_2 (1UL << 2)
#define PICO_RTOS_EVENT_BIT_3 (1UL << 3)
#define PICO_RTOS_EVENT_BIT_4 (1UL << 4)
#define PICO_RTOS_EVENT_BIT_5 (1UL << 5)
static inline bool pico_rtos_event_group_init(pico_rtos_event_group_t *group) { (void)group; return false; }
static inline bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *group, uint32_t bits) { (void)group; (void)bits; return false; }
static inline uint32_t pico_rtos_event_group_wait_bits(pico_rtos_event_group_t *group, uint32_t bits, bool wait_all, bool clear, uint32_t timeout) { (void)group; (void)bits; (void)wait_all; (void)clear; (void)timeout; return 0; }
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

#ifndef PICO_RTOS_ENABLE_MULTI_CORE
static inline bool pico_rtos_smp_init(void) { return false; }
static inline uint32_t pico_rtos_smp_get_core_id(void) { return 0; }
typedef enum { PICO_RTOS_CORE_AFFINITY_NONE, PICO_RTOS_CORE_AFFINITY_CORE0, PICO_RTOS_CORE_AFFINITY_CORE1, PICO_RTOS_CORE_AFFINITY_ANY } pico_rtos_core_affinity_t;
static inline bool pico_rtos_smp_set_task_affinity(pico_rtos_task_t *task, pico_rtos_core_affinity_t affinity) { (void)task; (void)affinity; return false; }
#endif

#ifndef PICO_RTOS_ENABLE_EXECUTION_PROFILING
static inline bool pico_rtos_profiler_init(void) { return false; }
#endif

#ifndef PICO_RTOS_ENABLE_SYSTEM_TRACING
static inline bool pico_rtos_trace_init(void) { return false; }
#endif

// Test configuration
#define INTEGRATION_TASK_STACK_SIZE 1024
#define NUM_INTEGRATION_TASKS 4
#define INTEGRATION_TEST_DURATION_MS 5000
#define STREAM_BUFFER_SIZE 1024
#define MEMORY_POOL_BLOCKS 16
#define MEMORY_POOL_BLOCK_SIZE 128

// Integration test objects
static pico_rtos_event_group_t coordination_events;
static pico_rtos_stream_buffer_t data_stream;
static uint8_t stream_memory[STREAM_BUFFER_SIZE];
static pico_rtos_memory_pool_t shared_memory_pool;
static uint8_t pool_memory[MEMORY_POOL_BLOCKS * MEMORY_POOL_BLOCK_SIZE + 256];

// Test state tracking
static volatile bool test_passed = true;
static volatile int test_count = 0;
static volatile int test_failures = 0;

// Integration test data structure
typedef struct {
    uint32_t sequence_number;
    uint32_t source_task_id;
    uint32_t source_core_id;
    uint32_t timestamp;
    uint8_t data_payload[64];
    uint32_t checksum;
} integration_message_t;

// Task coordination events
#define EVENT_PRODUCER_READY    PICO_RTOS_EVENT_BIT_0
#define EVENT_CONSUMER_READY    PICO_RTOS_EVENT_BIT_1
#define EVENT_DATA_AVAILABLE    PICO_RTOS_EVENT_BIT_2
#define EVENT_PROCESSING_DONE   PICO_RTOS_EVENT_BIT_3
#define EVENT_ERROR_OCCURRED    PICO_RTOS_EVENT_BIT_4
#define EVENT_SHUTDOWN_REQUEST  PICO_RTOS_EVENT_BIT_5

// Test statistics
typedef struct {
    uint32_t task_id;
    uint32_t assigned_core;
    uint32_t messages_produced;
    uint32_t messages_consumed;
    uint32_t memory_allocations;
    uint32_t memory_deallocations;
    uint32_t event_operations;
    uint32_t error_recoveries;
    bool task_completed;
    uint32_t execution_time_ms;
} integration_task_stats_t;

static integration_task_stats_t task_stats[NUM_INTEGRATION_TASKS];

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
// UTILITY FUNCTIONS
// =============================================================================

static uint32_t calculate_checksum(const uint8_t *data, size_t length) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
        checksum = (checksum << 1) | (checksum >> 31); // Rotate left
    }
    return checksum;
}

static bool validate_message(const integration_message_t *msg) {
    uint32_t calculated_checksum = calculate_checksum(msg->data_payload, 
                                                     sizeof(msg->data_payload));
    return (msg->checksum == calculated_checksum);
}

// =============================================================================
// INTEGRATION TEST TASKS
// =============================================================================

static void integration_producer_task(void *param) {
    integration_task_stats_t *stats = (integration_task_stats_t *)param;
    stats->assigned_core = pico_rtos_smp_get_core_id();
    uint32_t start_time = pico_rtos_get_tick_count();
    
    // Signal producer ready
    pico_rtos_event_group_set_bits(&coordination_events, EVENT_PRODUCER_READY);
    stats->event_operations++;
    
    // Wait for consumer to be ready
    uint32_t events = pico_rtos_event_group_wait_bits(&coordination_events,
                                                     EVENT_CONSUMER_READY,
                                                     false, false, 1000);
    if (events & EVENT_CONSUMER_READY) {
        stats->event_operations++;
    }
    
    uint32_t sequence = 0;
    uint32_t test_end_time = start_time + INTEGRATION_TEST_DURATION_MS;
    
    while (pico_rtos_get_tick_count() < test_end_time) {
        // Check for shutdown request
        events = pico_rtos_event_group_wait_bits(&coordination_events,
                                                EVENT_SHUTDOWN_REQUEST,
                                                false, false, 0);
        if (events & EVENT_SHUTDOWN_REQUEST) {
            break;
        }
        
        // Allocate memory for message
        void *mem_block = pico_rtos_memory_pool_alloc(&shared_memory_pool, 100);
        if (mem_block) {
            stats->memory_allocations++;
            
            // Create integration message
            integration_message_t *msg = (integration_message_t *)mem_block;
            msg->sequence_number = sequence++;
            msg->source_task_id = stats->task_id;
            msg->source_core_id = stats->assigned_core;
            msg->timestamp = pico_rtos_get_tick_count();
            
            // Fill payload with test data
            for (int i = 0; i < sizeof(msg->data_payload); i++) {
                msg->data_payload[i] = (uint8_t)(sequence + i + stats->task_id);
            }
            
            // Calculate checksum
            msg->checksum = calculate_checksum(msg->data_payload, 
                                             sizeof(msg->data_payload));
            
            // Send message via stream buffer
            uint32_t sent = pico_rtos_stream_buffer_send(&data_stream,
                                                        msg,
                                                        sizeof(integration_message_t),
                                                        50);
            if (sent == sizeof(integration_message_t)) {
                stats->messages_produced++;
                
                // Signal data available
                pico_rtos_event_group_set_bits(&coordination_events, EVENT_DATA_AVAILABLE);
                stats->event_operations++;
            } else {
                // Handle send failure
                pico_rtos_event_group_set_bits(&coordination_events, EVENT_ERROR_OCCURRED);
                stats->error_recoveries++;
            }
            
            // Free memory
            pico_rtos_memory_pool_free(&shared_memory_pool, mem_block);
            stats->memory_deallocations++;
        } else {
            // Memory allocation failed - signal error
            pico_rtos_event_group_set_bits(&coordination_events, EVENT_ERROR_OCCURRED);
            stats->error_recoveries++;
            pico_rtos_task_delay(10); // Back off
        }
        
        pico_rtos_task_delay(20); // Control production rate
    }
    
    stats->execution_time_ms = pico_rtos_get_tick_count() - start_time;
    stats->task_completed = true;
    pico_rtos_task_delete(NULL);
}

static void integration_consumer_task(void *param) {
    integration_task_stats_t *stats = (integration_task_stats_t *)param;
    stats->assigned_core = pico_rtos_smp_get_core_id();
    uint32_t start_time = pico_rtos_get_tick_count();
    
    // Signal consumer ready
    pico_rtos_event_group_set_bits(&coordination_events, EVENT_CONSUMER_READY);
    stats->event_operations++;
    
    // Wait for producer to be ready
    uint32_t events = pico_rtos_event_group_wait_bits(&coordination_events,
                                                     EVENT_PRODUCER_READY,
                                                     false, false, 1000);
    if (events & EVENT_PRODUCER_READY) {
        stats->event_operations++;
    }
    
    uint32_t test_end_time = start_time + INTEGRATION_TEST_DURATION_MS;
    
    while (pico_rtos_get_tick_count() < test_end_time) {
        // Check for shutdown request
        events = pico_rtos_event_group_wait_bits(&coordination_events,
                                                EVENT_SHUTDOWN_REQUEST,
                                                false, false, 0);
        if (events & EVENT_SHUTDOWN_REQUEST) {
            break;
        }
        
        // Wait for data available event
        events = pico_rtos_event_group_wait_bits(&coordination_events,
                                                EVENT_DATA_AVAILABLE,
                                                false, true, 100); // Clear on exit
        if (events & EVENT_DATA_AVAILABLE) {
            stats->event_operations++;
            
            // Allocate memory for receiving message
            void *mem_block = pico_rtos_memory_pool_alloc(&shared_memory_pool, 100);
            if (mem_block) {
                stats->memory_allocations++;
                
                // Receive message from stream buffer
                uint32_t received = pico_rtos_stream_buffer_receive(&data_stream,
                                                                   mem_block,
                                                                   sizeof(integration_message_t),
                                                                   50);
                
                if (received == sizeof(integration_message_t)) {
                    integration_message_t *msg = (integration_message_t *)mem_block;
                    
                    // Validate message integrity
                    if (validate_message(msg)) {
                        stats->messages_consumed++;
                        
                        // Process message (simulate work)
                        pico_rtos_task_delay(5);
                        
                        // Signal processing done
                        pico_rtos_event_group_set_bits(&coordination_events, 
                                                      EVENT_PROCESSING_DONE);
                        stats->event_operations++;
                    } else {
                        // Message validation failed
                        pico_rtos_event_group_set_bits(&coordination_events, 
                                                      EVENT_ERROR_OCCURRED);
                        stats->error_recoveries++;
                    }
                } else {
                    // Receive failed
                    pico_rtos_event_group_set_bits(&coordination_events, 
                                                  EVENT_ERROR_OCCURRED);
                    stats->error_recoveries++;
                }
                
                // Free memory
                pico_rtos_memory_pool_free(&shared_memory_pool, mem_block);
                stats->memory_deallocations++;
            } else {
                // Memory allocation failed
                pico_rtos_event_group_set_bits(&coordination_events, EVENT_ERROR_OCCURRED);
                stats->error_recoveries++;
                pico_rtos_task_delay(10); // Back off
            }
        }
        
        pico_rtos_task_delay(15); // Control consumption rate
    }
    
    stats->execution_time_ms = pico_rtos_get_tick_count() - start_time;
    stats->task_completed = true;
    pico_rtos_task_delete(NULL);
}

static void integration_monitor_task(void *param) {
    integration_task_stats_t *stats = (integration_task_stats_t *)param;
    stats->assigned_core = pico_rtos_smp_get_core_id();
    uint32_t start_time = pico_rtos_get_tick_count();
    
    uint32_t test_end_time = start_time + INTEGRATION_TEST_DURATION_MS;
    uint32_t last_stats_time = start_time;
    
    while (pico_rtos_get_tick_count() < test_end_time) {
        // Check for errors
        uint32_t events = pico_rtos_event_group_wait_bits(&coordination_events,
                                                         EVENT_ERROR_OCCURRED,
                                                         false, true, 100);
        if (events & EVENT_ERROR_OCCURRED) {
            stats->event_operations++;
            stats->error_recoveries++;
            
            // Log error and attempt recovery
            printf("Monitor: Error detected, attempting recovery\n");
            
            // Clear any stuck states and retry
            pico_rtos_task_delay(50);
        }
        
        // Periodic statistics reporting
        uint32_t current_time = pico_rtos_get_tick_count();
        if (current_time - last_stats_time > 1000) { // Every second
            // Get system statistics
            uint32_t free_memory = pico_rtos_memory_pool_get_free_count(&shared_memory_pool);
            uint32_t stream_bytes = pico_rtos_stream_buffer_bytes_available(&data_stream);
            uint32_t current_events = pico_rtos_event_group_get_bits(&coordination_events);
            
            printf("Monitor: Free mem: %lu, Stream bytes: %lu, Events: 0x%lx\n",
                   free_memory, stream_bytes, current_events);
            
            last_stats_time = current_time;
        }
        
        pico_rtos_task_delay(100);
    }
    
    // Signal shutdown to other tasks
    pico_rtos_event_group_set_bits(&coordination_events, EVENT_SHUTDOWN_REQUEST);
    stats->event_operations++;
    
    stats->execution_time_ms = pico_rtos_get_tick_count() - start_time;
    stats->task_completed = true;
    pico_rtos_task_delete(NULL);
}

// =============================================================================
// INTEGRATION TEST FUNCTIONS
// =============================================================================

static void test_integration_initialization(void) {
    printf("\n=== Testing Integration Component Initialization ===\n");
    
    // Initialize all components
    bool event_result = pico_rtos_event_group_init(&coordination_events);
    TEST_ASSERT(event_result, "Coordination event group initialization should succeed");
    
    bool stream_result = pico_rtos_stream_buffer_init(&data_stream,
                                                     stream_memory,
                                                     sizeof(stream_memory));
    TEST_ASSERT(stream_result, "Data stream buffer initialization should succeed");
    
    bool pool_result = pico_rtos_memory_pool_init(&shared_memory_pool,
                                                 pool_memory,
                                                 MEMORY_POOL_BLOCK_SIZE,
                                                 MEMORY_POOL_BLOCKS);
    TEST_ASSERT(pool_result, "Shared memory pool initialization should succeed");
    
    // Initialize SMP if available
    bool smp_result = pico_rtos_smp_init();
    TEST_ASSERT(smp_result, "SMP initialization should succeed");
    
    // Configure SMP for integration testing
    pico_rtos_smp_set_assignment_strategy(PICO_RTOS_ASSIGN_LEAST_LOADED);
    pico_rtos_smp_set_load_balancing(true);
    
    // Initialize profiling and tracing
    bool profiler_result = pico_rtos_profiler_init();
    TEST_ASSERT(profiler_result, "Profiler initialization should succeed");
    
    bool trace_result = pico_rtos_trace_init();
    TEST_ASSERT(trace_result, "Trace system initialization should succeed");
    
    printf("All integration components initialized successfully\n");
}

static void test_event_groups_with_multicore_coordination(void) {
    printf("\n=== Testing Event Groups with Multi-Core Coordination ===\n");
    
    // Create integration tasks
    pico_rtos_task_t tasks[NUM_INTEGRATION_TASKS];
    uint32_t task_stacks[NUM_INTEGRATION_TASKS][INTEGRATION_TASK_STACK_SIZE / sizeof(uint32_t)];
    
    // Initialize task statistics
    for (int i = 0; i < NUM_INTEGRATION_TASKS; i++) {
        memset(&task_stats[i], 0, sizeof(integration_task_stats_t));
        task_stats[i].task_id = i;
    }
    
    // Create producer tasks
    bool result = pico_rtos_task_create(&tasks[0], "producer_0",
                                       integration_producer_task,
                                       &task_stats[0],
                                       sizeof(task_stacks[0]), 6);
    TEST_ASSERT(result, "Producer task 0 creation should succeed");
    pico_rtos_smp_set_task_affinity(&tasks[0], PICO_RTOS_CORE_AFFINITY_CORE0);
    
    result = pico_rtos_task_create(&tasks[1], "producer_1",
                                  integration_producer_task,
                                  &task_stats[1],
                                  sizeof(task_stacks[1]), 6);
    TEST_ASSERT(result, "Producer task 1 creation should succeed");
    pico_rtos_smp_set_task_affinity(&tasks[1], PICO_RTOS_CORE_AFFINITY_CORE1);
    
    // Create consumer task
    result = pico_rtos_task_create(&tasks[2], "consumer",
                                  integration_consumer_task,
                                  &task_stats[2],
                                  sizeof(task_stacks[2]), 5);
    TEST_ASSERT(result, "Consumer task creation should succeed");
    pico_rtos_smp_set_task_affinity(&tasks[2], PICO_RTOS_CORE_AFFINITY_ANY);
    
    // Create monitor task
    result = pico_rtos_task_create(&tasks[3], "monitor",
                                  integration_monitor_task,
                                  &task_stats[3],
                                  sizeof(task_stacks[3]), 7);
    TEST_ASSERT(result, "Monitor task creation should succeed");
    pico_rtos_smp_set_task_affinity(&tasks[3], PICO_RTOS_CORE_AFFINITY_ANY);
    
    printf("Created %d integration tasks with multi-core coordination\n", NUM_INTEGRATION_TASKS);
}

static void test_stream_buffers_with_memory_pools(void) {
    printf("\n=== Testing Stream Buffers with Memory Pools Integration ===\n");
    
    // Wait for integration test to complete
    bool all_completed = false;
    uint32_t timeout_count = 0;
    const uint32_t max_timeout = INTEGRATION_TEST_DURATION_MS + 2000;
    
    while (!all_completed && timeout_count < max_timeout) {
        all_completed = true;
        for (int i = 0; i < NUM_INTEGRATION_TASKS; i++) {
            if (!task_stats[i].task_completed) {
                all_completed = false;
                break;
            }
        }
        pico_rtos_task_delay(100);
        timeout_count += 100;
    }
    
    TEST_ASSERT(all_completed, "All integration tasks should complete");
    
    // Analyze results
    printf("\nIntegration Test Results:\n");
    printf("Task ID | Core | Produced | Consumed | Mem Alloc | Mem Free | Events | Errors | Time(ms)\n");
    printf("--------|------|----------|----------|-----------|----------|--------|--------|----------\n");
    
    uint32_t total_produced = 0, total_consumed = 0;
    uint32_t total_mem_alloc = 0, total_mem_free = 0;
    uint32_t total_events = 0, total_errors = 0;
    
    for (int i = 0; i < NUM_INTEGRATION_TASKS; i++) {
        integration_task_stats_t *stats = &task_stats[i];
        printf("   %2lu   |  %lu   |    %4lu  |    %4lu  |    %4lu   |   %4lu   |  %4lu  |  %4lu  |   %4lu\n",
               stats->task_id, stats->assigned_core, stats->messages_produced,
               stats->messages_consumed, stats->memory_allocations,
               stats->memory_deallocations, stats->event_operations,
               stats->error_recoveries, stats->execution_time_ms);
        
        total_produced += stats->messages_produced;
        total_consumed += stats->messages_consumed;
        total_mem_alloc += stats->memory_allocations;
        total_mem_free += stats->memory_deallocations;
        total_events += stats->event_operations;
        total_errors += stats->error_recoveries;
    }
    
    printf("\nTotals: Produced: %lu, Consumed: %lu, Mem Ops: %lu/%lu, Events: %lu, Errors: %lu\n",
           total_produced, total_consumed, total_mem_alloc, total_mem_free, 
           total_events, total_errors);
    
    // Validate integration
    TEST_ASSERT(total_produced > 0, "Messages should be produced");
    TEST_ASSERT(total_consumed > 0, "Messages should be consumed");
    TEST_ASSERT(total_mem_alloc == total_mem_free, "All allocated memory should be freed");
    TEST_ASSERT(total_events > 0, "Event operations should occur");
    
    // Check final system state
    uint32_t free_blocks = pico_rtos_memory_pool_get_free_count(&shared_memory_pool);
    uint32_t total_blocks = pico_rtos_memory_pool_get_total_count(&shared_memory_pool);
    TEST_ASSERT(free_blocks == total_blocks, "All memory blocks should be free");
    
    bool stream_empty = pico_rtos_stream_buffer_is_empty(&data_stream);
    printf("Stream buffer final state: %s\n", stream_empty ? "empty" : "has data");
}

static void test_profiling_and_tracing_integration(void) {
    printf("\n=== Testing Profiling and Tracing Integration ===\n");
    
    // Get system inspection statistics (using actual debug API)
    pico_rtos_system_inspection_t system_stats;
    bool system_result = pico_rtos_debug_get_system_inspection(&system_stats);
    TEST_ASSERT(system_result, "Should get system inspection statistics");
    
    printf("System stats: %lu context switches, %lu total tasks\n",
           system_stats.total_context_switches, system_stats.total_tasks);
    
    // Get task information for current task
    pico_rtos_task_info_t task_info;
    bool task_result = pico_rtos_debug_get_task_info(NULL, &task_info);
    TEST_ASSERT(task_result, "Should get current task information");
    
    printf("Current task: %s, CPU time: %llu us, context switches: %lu\n",
           task_info.name ? task_info.name : "unnamed", 
           task_info.cpu_time_us, task_info.context_switches);
    
    // Test that debug system captured the integration test activity
    TEST_ASSERT(system_stats.total_context_switches > 0, 
               "Context switches should be recorded during integration test");
    
    // Get SMP statistics
    pico_rtos_smp_scheduler_t smp_stats;
    bool smp_result = pico_rtos_smp_get_stats(&smp_stats);
    TEST_ASSERT(smp_result, "Should get SMP statistics");
    
    printf("SMP stats: %lu migrations, load balancing: %s\n",
           smp_stats.migration_count, 
           smp_stats.load_balancing_enabled ? "enabled" : "disabled");
}

static void test_cross_component_error_handling(void) {
    printf("\n=== Testing Cross-Component Error Handling ===\n");
    
    // Test error propagation between components
    uint32_t error_events = 0;
    for (int i = 0; i < NUM_INTEGRATION_TASKS; i++) {
        error_events += task_stats[i].error_recoveries;
    }
    
    printf("Total error recovery events: %lu\n", error_events);
    
    // Test component state after errors (using available functions)
    // Event groups and memory pools don't have validate functions, so we test basic operations
    uint32_t event_bits = pico_rtos_event_group_get_bits(&coordination_events);
    printf("Event group current bits: 0x%08lx\n", event_bits);
    
    // Test memory pool by attempting allocation/deallocation
    void *test_ptr = pico_rtos_memory_pool_alloc(&shared_memory_pool, PICO_RTOS_NO_WAIT);
    if (test_ptr) {
        pico_rtos_memory_pool_free(&shared_memory_pool, test_ptr);
        printf("Memory pool allocation/deallocation test passed\n");
    }
    
    // Test error statistics
    pico_rtos_event_group_stats_t event_stats;
    if (pico_rtos_event_group_get_stats(&coordination_events, &event_stats)) {
        printf("Event group: %lu set ops, %lu clear ops\n",
               event_stats.total_set_operations, event_stats.total_clear_operations);
    }
    
    pico_rtos_stream_buffer_stats_t stream_stats;
    if (pico_rtos_stream_buffer_get_stats(&data_stream, &stream_stats)) {
        printf("Stream buffer: %lu sent, %lu received, peak usage: %lu\n",
               stream_stats.messages_sent, stream_stats.messages_received,
               stream_stats.peak_usage);
    }
    
    // Verify error handling didn't leave system in inconsistent state
    TEST_ASSERT(event_stats.total_set_operations >= event_stats.total_clear_operations,
               "Event operations should be consistent");
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

static void run_integration_tests(void) {
    printf("\n");
    printf("======================================================\n");
    printf("  Integration Component Interaction Tests (v0.3.1)   \n");
    printf("======================================================\n");
    
    test_integration_initialization();
    test_event_groups_with_multicore_coordination();
    test_stream_buffers_with_memory_pools();
    test_profiling_and_tracing_integration();
    test_cross_component_error_handling();
    
    printf("\n");
    printf("======================================================\n");
    printf("         Integration Test Results                     \n");
    printf("======================================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_count - test_failures);
    printf("Failed: %d\n", test_failures);
    printf("Success rate: %.1f%%\n", 
           test_count > 0 ? (100.0 * (test_count - test_failures)) / test_count : 0.0);
    printf("======================================================\n");
    
    if (test_failures == 0) {
        printf("🎉 All integration tests PASSED!\n");
    } else {
        printf("❌ Some integration tests FAILED!\n");
    }
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("Integration Component Interaction Test Starting...\n");
    
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    run_integration_tests();
    
    return (test_failures == 0) ? 0 : 1;
}