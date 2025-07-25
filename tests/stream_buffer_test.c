#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"
#include "pico_rtos/stream_buffer.h"

// =============================================================================
// TEST CONFIGURATION
// =============================================================================

#define STREAM_BUFFER_TEST_TASK_STACK_SIZE 512
#define STREAM_BUFFER_TEST_TASK_PRIORITY 1
#define STREAM_BUFFER_SIZE 256
#define TEST_MESSAGE_MAX_SIZE 64

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[STREAM_BUFFER_SIZE];
    bool test_passed;
    uint32_t test_count;
    uint32_t error_count;
} stream_buffer_test_context_t;

static stream_buffer_test_context_t test_ctx;

// Test messages of various sizes
static const char *test_messages[] = {
    "Hello",                                    // 5 bytes
    "Stream Buffer Test Message",               // 27 bytes  
    "This is a longer test message for the stream buffer implementation", // 67 bytes
    "Short",                                    // 5 bytes
    "Medium length message for testing",       // 30 bytes
    ""                                         // 0 bytes (empty message)
};

static const uint32_t num_test_messages = sizeof(test_messages) / sizeof(test_messages[0]);

// =============================================================================
// TEST HELPER FUNCTIONS
// =============================================================================

static void test_assert(bool condition, const char *test_name) {
    test_ctx.test_count++;
    if (!condition) {
        printf("FAIL: %s\n", test_name);
        test_ctx.error_count++;
        test_ctx.test_passed = false;
    } else {
        printf("PASS: %s\n", test_name);
    }
}

static void print_test_summary(void) {
    printf("\n=== Stream Buffer Test Summary ===\n");
    printf("Total tests: %lu\n", test_ctx.test_count);
    printf("Passed: %lu\n", test_ctx.test_count - test_ctx.error_count);
    printf("Failed: %lu\n", test_ctx.error_count);
    printf("Overall result: %s\n", test_ctx.test_passed ? "PASS" : "FAIL");
    printf("==================================\n\n");
}

// =============================================================================
// BASIC FUNCTIONALITY TESTS
// =============================================================================

static void test_stream_buffer_initialization(void) {
    printf("\n--- Testing Stream Buffer Initialization ---\n");
    
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[128];
    
    // Test successful initialization
    bool result = pico_rtos_stream_buffer_init(&stream, buffer, sizeof(buffer));
    test_assert(result == true, "Stream buffer initialization should succeed");
    
    // Test initial state
    test_assert(pico_rtos_stream_buffer_is_empty(&stream), "New stream buffer should be empty");
    test_assert(!pico_rtos_stream_buffer_is_full(&stream), "New stream buffer should not be full");
    test_assert(pico_rtos_stream_buffer_bytes_available(&stream) == 0, "New stream buffer should have 0 bytes available");
    test_assert(pico_rtos_stream_buffer_free_space(&stream) > 0, "New stream buffer should have free space");
    
    // Test invalid parameters
    result = pico_rtos_stream_buffer_init(NULL, buffer, sizeof(buffer));
    test_assert(result == false, "Initialization with NULL stream should fail");
    
    result = pico_rtos_stream_buffer_init(&stream, NULL, sizeof(buffer));
    test_assert(result == false, "Initialization with NULL buffer should fail");
    
    result = pico_rtos_stream_buffer_init(&stream, buffer, PICO_RTOS_STREAM_BUFFER_MIN_SIZE - 1);
    test_assert(result == false, "Initialization with too small buffer should fail");
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&stream);
}

static void test_stream_buffer_basic_send_receive(void) {
    printf("\n--- Testing Basic Send/Receive Operations ---\n");
    
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[128];
    
    bool result = pico_rtos_stream_buffer_init(&stream, buffer, sizeof(buffer));
    test_assert(result == true, "Stream buffer initialization should succeed");
    
    // Test sending and receiving different message sizes
    for (uint32_t i = 0; i < num_test_messages; i++) {
        const char *test_msg = test_messages[i];
        uint32_t msg_len = strlen(test_msg);
        
        // Skip empty messages for basic test
        if (msg_len == 0) continue;
        
        printf("Testing message %lu: \"%s\" (%lu bytes)\n", i, test_msg, msg_len);
        
        // Send message
        uint32_t sent = pico_rtos_stream_buffer_send(&stream, test_msg, msg_len, PICO_RTOS_NO_WAIT);
        test_assert(sent == msg_len, "Should send complete message");
        
        // Check buffer state
        test_assert(!pico_rtos_stream_buffer_is_empty(&stream), "Buffer should not be empty after send");
        test_assert(pico_rtos_stream_buffer_bytes_available(&stream) > 0, "Should have bytes available after send");
        
        // Peek at message length
        uint32_t peeked_len = pico_rtos_stream_buffer_peek_message_length(&stream);
        test_assert(peeked_len == msg_len, "Peeked message length should match sent length");
        
        // Receive message
        char received_msg[TEST_MESSAGE_MAX_SIZE];
        uint32_t received = pico_rtos_stream_buffer_receive(&stream, received_msg, sizeof(received_msg), PICO_RTOS_NO_WAIT);
        test_assert(received == msg_len, "Should receive complete message");
        
        // Verify message content
        received_msg[received] = '\0'; // Null terminate for comparison
        test_assert(strcmp(received_msg, test_msg) == 0, "Received message should match sent message");
        
        // Check buffer state after receive
        test_assert(pico_rtos_stream_buffer_is_empty(&stream), "Buffer should be empty after receive");
        test_assert(pico_rtos_stream_buffer_bytes_available(&stream) == 0, "Should have 0 bytes available after receive");
    }
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&stream);
}

static void test_stream_buffer_multiple_messages(void) {
    printf("\n--- Testing Multiple Messages ---\n");
    
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[512]; // Larger buffer for multiple messages
    
    bool result = pico_rtos_stream_buffer_init(&stream, buffer, sizeof(buffer));
    test_assert(result == true, "Stream buffer initialization should succeed");
    
    // Send multiple messages
    uint32_t total_sent = 0;
    for (uint32_t i = 0; i < num_test_messages; i++) {
        const char *test_msg = test_messages[i];
        uint32_t msg_len = strlen(test_msg);
        
        // Skip empty messages
        if (msg_len == 0) continue;
        
        uint32_t sent = pico_rtos_stream_buffer_send(&stream, test_msg, msg_len, PICO_RTOS_NO_WAIT);
        if (sent == msg_len) {
            total_sent++;
            printf("Sent message %lu: \"%s\"\n", i, test_msg);
        }
    }
    
    test_assert(total_sent > 0, "Should have sent at least one message");
    test_assert(!pico_rtos_stream_buffer_is_empty(&stream), "Buffer should not be empty with multiple messages");
    
    // Receive all messages and verify order
    uint32_t total_received = 0;
    for (uint32_t i = 0; i < num_test_messages && total_received < total_sent; i++) {
        const char *expected_msg = test_messages[i];
        uint32_t expected_len = strlen(expected_msg);
        
        // Skip empty messages
        if (expected_len == 0) continue;
        
        char received_msg[TEST_MESSAGE_MAX_SIZE];
        uint32_t received = pico_rtos_stream_buffer_receive(&stream, received_msg, sizeof(received_msg), PICO_RTOS_NO_WAIT);
        
        if (received > 0) {
            received_msg[received] = '\0';
            test_assert(received == expected_len, "Received length should match expected");
            test_assert(strcmp(received_msg, expected_msg) == 0, "Received message should match expected");
            printf("Received message %lu: \"%s\"\n", i, received_msg);
            total_received++;
        }
    }
    
    test_assert(total_received == total_sent, "Should receive all sent messages");
    test_assert(pico_rtos_stream_buffer_is_empty(&stream), "Buffer should be empty after receiving all messages");
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&stream);
}

static void test_stream_buffer_circular_behavior(void) {
    printf("\n--- Testing Circular Buffer Behavior ---\n");
    
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[64]; // Small buffer to test wrap-around
    
    bool result = pico_rtos_stream_buffer_init(&stream, buffer, sizeof(buffer));
    test_assert(result == true, "Stream buffer initialization should succeed");
    
    const char *test_msg = "Test";
    uint32_t msg_len = strlen(test_msg);
    
    // Fill buffer partially, then receive some messages to create wrap-around scenario
    uint32_t messages_sent = 0;
    uint32_t messages_received = 0;
    
    // Send messages until buffer is nearly full
    while (true) {
        uint32_t sent = pico_rtos_stream_buffer_send(&stream, test_msg, msg_len, PICO_RTOS_NO_WAIT);
        if (sent == 0) break; // Buffer full
        messages_sent++;
    }
    
    test_assert(messages_sent > 0, "Should have sent at least one message");
    printf("Sent %lu messages to fill buffer\n", messages_sent);
    
    // Receive half the messages
    uint32_t messages_to_receive = messages_sent / 2;
    for (uint32_t i = 0; i < messages_to_receive; i++) {
        char received_msg[TEST_MESSAGE_MAX_SIZE];
        uint32_t received = pico_rtos_stream_buffer_receive(&stream, received_msg, sizeof(received_msg), PICO_RTOS_NO_WAIT);
        if (received > 0) {
            messages_received++;
        }
    }
    
    test_assert(messages_received == messages_to_receive, "Should have received expected number of messages");
    printf("Received %lu messages to create space\n", messages_received);
    
    // Now send more messages (should wrap around)
    uint32_t additional_sent = 0;
    while (additional_sent < 3) { // Try to send a few more
        uint32_t sent = pico_rtos_stream_buffer_send(&stream, test_msg, msg_len, PICO_RTOS_NO_WAIT);
        if (sent == 0) break; // Buffer full
        additional_sent++;
    }
    
    test_assert(additional_sent > 0, "Should have sent additional messages after wrap-around");
    printf("Sent %lu additional messages after wrap-around\n", additional_sent);
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&stream);
}

static void test_stream_buffer_edge_cases(void) {
    printf("\n--- Testing Edge Cases ---\n");
    
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[128];
    
    bool result = pico_rtos_stream_buffer_init(&stream, buffer, sizeof(buffer));
    test_assert(result == true, "Stream buffer initialization should succeed");
    
    // Test sending to full buffer with no wait
    const char *fill_msg = "Fill message for testing buffer full condition";
    uint32_t fill_len = strlen(fill_msg);
    
    // Fill buffer completely
    uint32_t messages_sent = 0;
    while (true) {
        uint32_t sent = pico_rtos_stream_buffer_send(&stream, fill_msg, fill_len, PICO_RTOS_NO_WAIT);
        if (sent == 0) break;
        messages_sent++;
    }
    
    test_assert(messages_sent > 0, "Should have sent messages to fill buffer");
    test_assert(pico_rtos_stream_buffer_is_full(&stream) || 
                pico_rtos_stream_buffer_free_space(&stream) < (fill_len + 4), 
                "Buffer should be full or nearly full");
    
    // Try to send another message (should fail immediately)
    uint32_t sent = pico_rtos_stream_buffer_send(&stream, "Extra", 5, PICO_RTOS_NO_WAIT);
    test_assert(sent == 0, "Send to full buffer with no wait should fail");
    
    // Test receiving from empty buffer with no wait
    pico_rtos_stream_buffer_flush(&stream);
    test_assert(pico_rtos_stream_buffer_is_empty(&stream), "Buffer should be empty after flush");
    
    char received_msg[TEST_MESSAGE_MAX_SIZE];
    uint32_t received = pico_rtos_stream_buffer_receive(&stream, received_msg, sizeof(received_msg), PICO_RTOS_NO_WAIT);
    test_assert(received == 0, "Receive from empty buffer with no wait should fail");
    
    // Test message larger than receive buffer
    const char *large_msg = "This is a very long message that is larger than the receive buffer to test truncation behavior";
    uint32_t large_len = strlen(large_msg);
    
    sent = pico_rtos_stream_buffer_send(&stream, large_msg, large_len, PICO_RTOS_NO_WAIT);
    test_assert(sent == large_len, "Should send large message successfully");
    
    char small_buffer[20];
    received = pico_rtos_stream_buffer_receive(&stream, small_buffer, sizeof(small_buffer) - 1, PICO_RTOS_NO_WAIT);
    test_assert(received == sizeof(small_buffer) - 1, "Should receive truncated message");
    
    small_buffer[received] = '\0';
    test_assert(strncmp(small_buffer, large_msg, received) == 0, "Truncated message should match beginning of sent message");
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&stream);
}

static void test_stream_buffer_statistics(void) {
    printf("\n--- Testing Statistics ---\n");
    
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[256];
    
    bool result = pico_rtos_stream_buffer_init(&stream, buffer, sizeof(buffer));
    test_assert(result == true, "Stream buffer initialization should succeed");
    
    // Get initial statistics
    pico_rtos_stream_buffer_stats_t stats;
    result = pico_rtos_stream_buffer_get_stats(&stream, &stats);
    test_assert(result == true, "Should get statistics successfully");
    test_assert(stats.messages_sent == 0, "Initial messages sent should be 0");
    test_assert(stats.messages_received == 0, "Initial messages received should be 0");
    test_assert(stats.bytes_sent == 0, "Initial bytes sent should be 0");
    test_assert(stats.bytes_received == 0, "Initial bytes received should be 0");
    
    // Send some messages and check statistics
    const char *test_msg = "Statistics test message";
    uint32_t msg_len = strlen(test_msg);
    uint32_t num_messages = 3;
    
    for (uint32_t i = 0; i < num_messages; i++) {
        uint32_t sent = pico_rtos_stream_buffer_send(&stream, test_msg, msg_len, PICO_RTOS_NO_WAIT);
        test_assert(sent == msg_len, "Should send message successfully");
    }
    
    result = pico_rtos_stream_buffer_get_stats(&stream, &stats);
    test_assert(result == true, "Should get statistics successfully");
    test_assert(stats.messages_sent == num_messages, "Messages sent count should be correct");
    test_assert(stats.bytes_sent == num_messages * msg_len, "Bytes sent count should be correct");
    test_assert(stats.peak_usage > 0, "Peak usage should be greater than 0");
    
    // Receive messages and check statistics
    for (uint32_t i = 0; i < num_messages; i++) {
        char received_msg[TEST_MESSAGE_MAX_SIZE];
        uint32_t received = pico_rtos_stream_buffer_receive(&stream, received_msg, sizeof(received_msg), PICO_RTOS_NO_WAIT);
        test_assert(received == msg_len, "Should receive message successfully");
    }
    
    result = pico_rtos_stream_buffer_get_stats(&stream, &stats);
    test_assert(result == true, "Should get statistics successfully");
    test_assert(stats.messages_received == num_messages, "Messages received count should be correct");
    test_assert(stats.bytes_received == num_messages * msg_len, "Bytes received count should be correct");
    
    // Test statistics reset
    pico_rtos_stream_buffer_reset_stats(&stream);
    result = pico_rtos_stream_buffer_get_stats(&stream, &stats);
    test_assert(result == true, "Should get statistics successfully");
    test_assert(stats.messages_sent == 0, "Messages sent should be reset to 0");
    test_assert(stats.messages_received == 0, "Messages received should be reset to 0");
    test_assert(stats.bytes_sent == 0, "Bytes sent should be reset to 0");
    test_assert(stats.bytes_received == 0, "Bytes received should be reset to 0");
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&stream);
}

// =============================================================================
// CONCURRENT ACCESS TESTS (Basic - will be enhanced in subtask 3.2)
// =============================================================================

static volatile bool sender_task_running = false;
static volatile bool receiver_task_running = false;
static volatile uint32_t concurrent_messages_sent = 0;
static volatile uint32_t concurrent_messages_received = 0;

static void concurrent_sender_task(void *param) {
    stream_buffer_test_context_t *ctx = (stream_buffer_test_context_t *)param;
    sender_task_running = true;
    
    const char *msg = "Concurrent test message";
    uint32_t msg_len = strlen(msg);
    
    for (uint32_t i = 0; i < 5; i++) {
        uint32_t sent = pico_rtos_stream_buffer_send(&ctx->stream, msg, msg_len, 100); // 100ms timeout
        if (sent == msg_len) {
            concurrent_messages_sent++;
        }
        pico_rtos_task_delay(10); // Small delay between sends
    }
    
    sender_task_running = false;
    pico_rtos_task_suspend(pico_rtos_get_current_task());
}

static void concurrent_receiver_task(void *param) {
    stream_buffer_test_context_t *ctx = (stream_buffer_test_context_t *)param;
    receiver_task_running = true;
    
    char received_msg[TEST_MESSAGE_MAX_SIZE];
    
    while (sender_task_running || !pico_rtos_stream_buffer_is_empty(&ctx->stream)) {
        uint32_t received = pico_rtos_stream_buffer_receive(&ctx->stream, received_msg, sizeof(received_msg), 100); // 100ms timeout
        if (received > 0) {
            concurrent_messages_received++;
        }
        pico_rtos_task_delay(15); // Small delay between receives
    }
    
    receiver_task_running = false;
    pico_rtos_task_suspend(pico_rtos_get_current_task());
}

static void test_stream_buffer_concurrent_access(void) {
    printf("\n--- Testing Basic Concurrent Access ---\n");
    
    bool result = pico_rtos_stream_buffer_init(&test_ctx.stream, test_ctx.buffer, sizeof(test_ctx.buffer));
    test_assert(result == true, "Stream buffer initialization should succeed");
    
    // Reset counters
    concurrent_messages_sent = 0;
    concurrent_messages_received = 0;
    
    // Create sender and receiver tasks
    pico_rtos_task_t sender_task, receiver_task;
    
    result = pico_rtos_task_create(&sender_task, "Sender", concurrent_sender_task, &test_ctx, 
                                  STREAM_BUFFER_TEST_TASK_STACK_SIZE, STREAM_BUFFER_TEST_TASK_PRIORITY);
    test_assert(result == true, "Should create sender task successfully");
    
    result = pico_rtos_task_create(&receiver_task, "Receiver", concurrent_receiver_task, &test_ctx,
                                  STREAM_BUFFER_TEST_TASK_STACK_SIZE, STREAM_BUFFER_TEST_TASK_PRIORITY);
    test_assert(result == true, "Should create receiver task successfully");
    
    // Wait for tasks to complete
    uint32_t timeout_count = 0;
    while ((sender_task_running || receiver_task_running) && timeout_count < 1000) {
        pico_rtos_task_delay(10);
        timeout_count++;
    }
    
    test_assert(timeout_count < 1000, "Tasks should complete within timeout");
    test_assert(concurrent_messages_sent > 0, "Should have sent messages concurrently");
    test_assert(concurrent_messages_received > 0, "Should have received messages concurrently");
    
    printf("Concurrent test: Sent %lu, Received %lu messages\n", 
           concurrent_messages_sent, concurrent_messages_received);
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&test_ctx.stream);
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

static void run_all_stream_buffer_tests(void) {
    printf("\n========================================\n");
    printf("Starting Stream Buffer Unit Tests\n");
    printf("========================================\n");
    
    // Initialize test context
    memset(&test_ctx, 0, sizeof(test_ctx));
    test_ctx.test_passed = true;
    
    // Run all tests
    test_stream_buffer_initialization();
    test_stream_buffer_basic_send_receive();
    test_stream_buffer_multiple_messages();
    test_stream_buffer_circular_behavior();
    test_stream_buffer_edge_cases();
    test_stream_buffer_statistics();
    test_stream_buffer_concurrent_access();
    
    // Print summary
    print_test_summary();
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Wait a moment for USB serial to be ready
    sleep_ms(2000);
    
    printf("Stream Buffer Test Starting...\n");
    
    // Initialize Pico-RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    // Run tests
    run_all_stream_buffer_tests();
    
    // Start scheduler for concurrent tests
    printf("Starting RTOS scheduler for concurrent tests...\n");
    pico_rtos_start();
    
    // Should never reach here
    return 0;
}