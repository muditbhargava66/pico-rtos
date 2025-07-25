#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"
#include "pico_rtos/stream_buffer.h"

// =============================================================================
// CONCURRENT TEST CONFIGURATION
// =============================================================================

#define CONCURRENT_TEST_TASK_STACK_SIZE 512
#define CONCURRENT_TEST_TASK_PRIORITY 1
#define STREAM_BUFFER_SIZE 512
#define NUM_SENDER_TASKS 3
#define NUM_RECEIVER_TASKS 2
#define MESSAGES_PER_SENDER 10
#define TEST_TIMEOUT_MS 5000

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[STREAM_BUFFER_SIZE];
    
    // Test synchronization
    volatile uint32_t messages_sent;
    volatile uint32_t messages_received;
    volatile uint32_t send_errors;
    volatile uint32_t receive_errors;
    volatile uint32_t data_corruption_errors;
    
    // Task completion tracking
    volatile uint32_t senders_completed;
    volatile uint32_t receivers_completed;
    volatile bool test_complete;
    
    // Statistics
    uint32_t total_bytes_sent;
    uint32_t total_bytes_received;
    uint32_t max_concurrent_senders;
    uint32_t max_concurrent_receivers;
} concurrent_test_context_t;

static concurrent_test_context_t test_ctx;

// Test message patterns for corruption detection
static const char *test_patterns[] = {
    "Pattern_A_12345",
    "Pattern_B_67890", 
    "Pattern_C_ABCDEF",
    "Pattern_D_GHIJKL",
    "Pattern_E_MNOPQR"
};
static const uint32_t num_patterns = sizeof(test_patterns) / sizeof(test_patterns[0]);

// =============================================================================
// CONCURRENT SENDER TASKS
// =============================================================================

static void concurrent_sender_task(void *param) {
    uint32_t task_id = (uint32_t)(uintptr_t)param;
    char message_buffer[64];
    uint32_t messages_sent_by_task = 0;
    uint32_t send_failures = 0;
    
    printf("Sender task %lu started\n", task_id);
    
    for (uint32_t i = 0; i < MESSAGES_PER_SENDER; i++) {
        // Create unique message with task ID and sequence number
        uint32_t pattern_idx = (task_id + i) % num_patterns;
        snprintf(message_buffer, sizeof(message_buffer), 
                "Task%lu_Seq%lu_%s", task_id, i, test_patterns[pattern_idx]);
        
        uint32_t message_len = strlen(message_buffer);
        
        // Send with timeout to avoid infinite blocking
        uint32_t sent = pico_rtos_stream_buffer_send(&test_ctx.stream, 
                                                    message_buffer, 
                                                    message_len, 
                                                    1000); // 1 second timeout
        
        if (sent == message_len) {
            messages_sent_by_task++;
            __atomic_fetch_add(&test_ctx.messages_sent, 1, __ATOMIC_SEQ_CST);
            __atomic_fetch_add(&test_ctx.total_bytes_sent, message_len, __ATOMIC_SEQ_CST);
        } else {
            send_failures++;
            __atomic_fetch_add(&test_ctx.send_errors, 1, __ATOMIC_SEQ_CST);
            printf("Sender %lu: Send failed for message %lu (sent %lu/%lu bytes)\n", 
                   task_id, i, sent, message_len);
        }
        
        // Small delay to allow interleaving
        pico_rtos_task_delay(10 + (task_id * 5)); // Staggered delays
    }
    
    printf("Sender task %lu completed: %lu messages sent, %lu failures\n", 
           task_id, messages_sent_by_task, send_failures);
    
    __atomic_fetch_add(&test_ctx.senders_completed, 1, __ATOMIC_SEQ_CST);
    pico_rtos_task_suspend(pico_rtos_get_current_task());
}

// =============================================================================
// CONCURRENT RECEIVER TASKS
// =============================================================================

static void concurrent_receiver_task(void *param) {
    uint32_t task_id = (uint32_t)(uintptr_t)param;
    char receive_buffer[128];
    uint32_t messages_received_by_task = 0;
    uint32_t receive_failures = 0;
    uint32_t corruption_errors = 0;
    
    printf("Receiver task %lu started\n", task_id);
    
    // Continue receiving until all senders are done and buffer is empty
    while (!test_ctx.test_complete || !pico_rtos_stream_buffer_is_empty(&test_ctx.stream)) {
        uint32_t received = pico_rtos_stream_buffer_receive(&test_ctx.stream,
                                                           receive_buffer,
                                                           sizeof(receive_buffer) - 1,
                                                           500); // 500ms timeout
        
        if (received > 0) {
            receive_buffer[received] = '\0'; // Null terminate
            messages_received_by_task++;
            __atomic_fetch_add(&test_ctx.messages_received, 1, __ATOMIC_SEQ_CST);
            __atomic_fetch_add(&test_ctx.total_bytes_received, received, __ATOMIC_SEQ_CST);
            
            // Validate message format: TaskX_SeqY_PatternZ
            bool format_valid = false;
            uint32_t sender_id, seq_num;
            char pattern_part[32];
            
            if (sscanf(receive_buffer, "Task%lu_Seq%lu_%31s", &sender_id, &seq_num, pattern_part) == 3) {
                // Check if pattern is one of the expected patterns
                for (uint32_t p = 0; p < num_patterns; p++) {
                    if (strcmp(pattern_part, test_patterns[p]) == 0) {
                        format_valid = true;
                        break;
                    }
                }
            }
            
            if (!format_valid) {
                corruption_errors++;
                __atomic_fetch_add(&test_ctx.data_corruption_errors, 1, __ATOMIC_SEQ_CST);
                printf("Receiver %lu: Data corruption detected: '%s'\n", task_id, receive_buffer);
            }
            
            // Log every 10th message to avoid spam
            if (messages_received_by_task % 10 == 0) {
                printf("Receiver %lu: Received message %lu: '%s'\n", 
                       task_id, messages_received_by_task, receive_buffer);
            }
        } else {
            receive_failures++;
            if (receive_failures % 10 == 0) {
                printf("Receiver %lu: Receive timeout/failure count: %lu\n", 
                       task_id, receive_failures);
            }
        }
        
        // Check if test should complete
        if (test_ctx.senders_completed >= NUM_SENDER_TASKS && 
            pico_rtos_stream_buffer_is_empty(&test_ctx.stream)) {
            break;
        }
        
        // Small delay to allow other receivers to run
        pico_rtos_task_delay(5);
    }
    
    printf("Receiver task %lu completed: %lu messages received, %lu failures, %lu corruptions\n", 
           task_id, messages_received_by_task, receive_failures, corruption_errors);
    
    __atomic_fetch_add(&test_ctx.receivers_completed, 1, __ATOMIC_SEQ_CST);
    pico_rtos_task_suspend(pico_rtos_get_current_task());
}

// =============================================================================
// TEST COORDINATOR TASK
// =============================================================================

static void test_coordinator_task(void *param) {
    (void)param;
    
    printf("Test coordinator started\n");
    
    uint32_t start_time = pico_rtos_get_tick_count();
    uint32_t last_stats_time = start_time;
    
    // Monitor test progress
    while (test_ctx.senders_completed < NUM_SENDER_TASKS || 
           test_ctx.receivers_completed < NUM_RECEIVER_TASKS) {
        
        uint32_t current_time = pico_rtos_get_tick_count();
        
        // Print periodic statistics
        if (current_time - last_stats_time >= 1000) { // Every second
            pico_rtos_stream_buffer_stats_t stats;
            pico_rtos_stream_buffer_get_stats(&test_ctx.stream, &stats);
            
            printf("=== Test Progress ===\n");
            printf("Time: %lu ms\n", current_time - start_time);
            printf("Senders completed: %lu/%d\n", test_ctx.senders_completed, NUM_SENDER_TASKS);
            printf("Receivers completed: %lu/%d\n", test_ctx.receivers_completed, NUM_RECEIVER_TASKS);
            printf("Messages sent: %lu\n", test_ctx.messages_sent);
            printf("Messages received: %lu\n", test_ctx.messages_received);
            printf("Buffer usage: %lu/%lu bytes\n", stats.bytes_available, stats.buffer_size);
            printf("Blocked senders: %lu\n", stats.blocked_senders);
            printf("Blocked receivers: %lu\n", stats.blocked_receivers);
            printf("Send errors: %lu\n", test_ctx.send_errors);
            printf("Receive errors: %lu\n", test_ctx.receive_errors);
            printf("Data corruption errors: %lu\n", test_ctx.data_corruption_errors);
            printf("====================\n\n");
            
            last_stats_time = current_time;
        }
        
        // Check for timeout
        if (current_time - start_time > TEST_TIMEOUT_MS) {
            printf("ERROR: Test timeout after %lu ms\n", current_time - start_time);
            break;
        }
        
        pico_rtos_task_delay(100);
    }
    
    // Signal test completion
    test_ctx.test_complete = true;
    
    // Wait a bit more for receivers to finish
    pico_rtos_task_delay(1000);
    
    // Print final results
    uint32_t total_time = pico_rtos_get_tick_count() - start_time;
    
    printf("\n=== CONCURRENT TEST RESULTS ===\n");
    printf("Test duration: %lu ms\n", total_time);
    printf("Expected messages: %d\n", NUM_SENDER_TASKS * MESSAGES_PER_SENDER);
    printf("Messages sent: %lu\n", test_ctx.messages_sent);
    printf("Messages received: %lu\n", test_ctx.messages_received);
    printf("Total bytes sent: %lu\n", test_ctx.total_bytes_sent);
    printf("Total bytes received: %lu\n", test_ctx.total_bytes_received);
    printf("Send errors: %lu\n", test_ctx.send_errors);
    printf("Receive errors: %lu\n", test_ctx.receive_errors);
    printf("Data corruption errors: %lu\n", test_ctx.data_corruption_errors);
    
    // Calculate success rate
    uint32_t expected_messages = NUM_SENDER_TASKS * MESSAGES_PER_SENDER;
    float send_success_rate = (float)test_ctx.messages_sent / expected_messages * 100.0f;
    float receive_success_rate = (float)test_ctx.messages_received / test_ctx.messages_sent * 100.0f;
    
    printf("Send success rate: %.1f%%\n", send_success_rate);
    printf("Receive success rate: %.1f%%\n", receive_success_rate);
    
    // Final buffer statistics
    pico_rtos_stream_buffer_stats_t final_stats;
    pico_rtos_stream_buffer_get_stats(&test_ctx.stream, &final_stats);
    printf("Peak buffer usage: %lu bytes\n", final_stats.peak_usage);
    printf("Final buffer state: %lu bytes available\n", final_stats.bytes_available);
    
    // Test verdict
    bool test_passed = (test_ctx.data_corruption_errors == 0) &&
                      (test_ctx.messages_received == test_ctx.messages_sent) &&
                      (send_success_rate >= 95.0f);
    
    printf("\nTEST RESULT: %s\n", test_passed ? "PASS" : "FAIL");
    printf("===============================\n\n");
    
    pico_rtos_task_suspend(pico_rtos_get_current_task());
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

static void run_concurrent_stream_buffer_test(void) {
    printf("\n========================================\n");
    printf("Starting Stream Buffer Concurrent Test\n");
    printf("========================================\n");
    
    // Initialize test context
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize stream buffer
    bool result = pico_rtos_stream_buffer_init(&test_ctx.stream, 
                                              test_ctx.buffer, 
                                              sizeof(test_ctx.buffer));
    if (!result) {
        printf("ERROR: Failed to initialize stream buffer\n");
        return;
    }
    
    printf("Stream buffer initialized: %d bytes\n", STREAM_BUFFER_SIZE);
    printf("Creating %d sender tasks, %d receiver tasks\n", NUM_SENDER_TASKS, NUM_RECEIVER_TASKS);
    printf("Each sender will send %d messages\n", MESSAGES_PER_SENDER);
    
    // Create sender tasks
    pico_rtos_task_t sender_tasks[NUM_SENDER_TASKS];
    for (uint32_t i = 0; i < NUM_SENDER_TASKS; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "Sender_%lu", i);
        
        result = pico_rtos_task_create(&sender_tasks[i], 
                                      task_name,
                                      concurrent_sender_task, 
                                      (void *)(uintptr_t)i,
                                      CONCURRENT_TEST_TASK_STACK_SIZE, 
                                      CONCURRENT_TEST_TASK_PRIORITY);
        if (!result) {
            printf("ERROR: Failed to create sender task %lu\n", i);
            return;
        }
    }
    
    // Create receiver tasks
    pico_rtos_task_t receiver_tasks[NUM_RECEIVER_TASKS];
    for (uint32_t i = 0; i < NUM_RECEIVER_TASKS; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "Receiver_%lu", i);
        
        result = pico_rtos_task_create(&receiver_tasks[i], 
                                      task_name,
                                      concurrent_receiver_task, 
                                      (void *)(uintptr_t)i,
                                      CONCURRENT_TEST_TASK_STACK_SIZE, 
                                      CONCURRENT_TEST_TASK_PRIORITY);
        if (!result) {
            printf("ERROR: Failed to create receiver task %lu\n", i);
            return;
        }
    }
    
    // Create coordinator task
    pico_rtos_task_t coordinator_task;
    result = pico_rtos_task_create(&coordinator_task,
                                  "Coordinator",
                                  test_coordinator_task,
                                  NULL,
                                  CONCURRENT_TEST_TASK_STACK_SIZE,
                                  CONCURRENT_TEST_TASK_PRIORITY + 1); // Higher priority
    if (!result) {
        printf("ERROR: Failed to create coordinator task\n");
        return;
    }
    
    printf("All tasks created successfully\n");
    printf("Starting concurrent test...\n\n");
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Wait a moment for USB serial to be ready
    sleep_ms(2000);
    
    printf("Stream Buffer Concurrent Test Starting...\n");
    
    // Initialize Pico-RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    // Run concurrent test
    run_concurrent_stream_buffer_test();
    
    // Start scheduler
    printf("Starting RTOS scheduler...\n");
    pico_rtos_start();
    
    // Should never reach here
    return 0;
}