#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pico/stdlib.h"
#include "pico_rtos.h"
#include "pico_rtos/stream_buffer.h"

// =============================================================================
// PERFORMANCE TEST CONFIGURATION
// =============================================================================

#define PERFORMANCE_TEST_TASK_STACK_SIZE 1024
#define PERFORMANCE_TEST_TASK_PRIORITY 1
#define STREAM_BUFFER_SIZE 2048
#define NUM_TEST_ITERATIONS 100
#define LARGE_MESSAGE_SIZE 512  // Above zero-copy threshold
#define SMALL_MESSAGE_SIZE 32   // Below zero-copy threshold

// =============================================================================
// TEST DATA STRUCTURES
// =============================================================================

typedef struct {
    pico_rtos_stream_buffer_t stream;
    uint8_t buffer[STREAM_BUFFER_SIZE];
    
    // Performance metrics
    uint32_t standard_send_time_us;
    uint32_t standard_receive_time_us;
    uint32_t zero_copy_send_time_us;
    uint32_t zero_copy_receive_time_us;
    
    // Test data
    uint8_t test_data[LARGE_MESSAGE_SIZE];
    uint8_t receive_buffer[LARGE_MESSAGE_SIZE];
    
    // Test control
    volatile bool test_complete;
} performance_test_context_t;

static performance_test_context_t test_ctx;

// =============================================================================
// PERFORMANCE MEASUREMENT HELPERS
// =============================================================================

static inline uint32_t get_time_us(void) {
    return time_us_32();
}

static void fill_test_data(uint8_t *data, uint32_t size, uint8_t pattern) {
    for (uint32_t i = 0; i < size; i++) {
        data[i] = pattern + (i & 0xFF);
    }
}

static bool verify_test_data(const uint8_t *data, uint32_t size, uint8_t pattern) {
    for (uint32_t i = 0; i < size; i++) {
        if (data[i] != (uint8_t)(pattern + (i & 0xFF))) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// STANDARD OPERATION PERFORMANCE TESTS
// =============================================================================

static void test_standard_operations_performance(void) {
    printf("\n--- Testing Standard Operations Performance ---\n");
    
    // Initialize stream buffer
    bool result = pico_rtos_stream_buffer_init(&test_ctx.stream, 
                                              test_ctx.buffer, 
                                              sizeof(test_ctx.buffer));
    if (!result) {
        printf("ERROR: Failed to initialize stream buffer\n");
        return;
    }
    
    // Disable zero-copy for this test
    pico_rtos_stream_buffer_set_zero_copy(&test_ctx.stream, false);
    
    // Fill test data
    fill_test_data(test_ctx.test_data, LARGE_MESSAGE_SIZE, 0xAA);
    
    printf("Testing standard operations with %d byte messages...\n", LARGE_MESSAGE_SIZE);
    
    uint32_t total_send_time = 0;
    uint32_t total_receive_time = 0;
    uint32_t successful_iterations = 0;
    
    for (uint32_t i = 0; i < NUM_TEST_ITERATIONS; i++) {
        // Measure send time
        uint32_t send_start = get_time_us();
        uint32_t sent = pico_rtos_stream_buffer_send(&test_ctx.stream,
                                                    test_ctx.test_data,
                                                    LARGE_MESSAGE_SIZE,
                                                    PICO_RTOS_NO_WAIT);
        uint32_t send_end = get_time_us();
        
        if (sent != LARGE_MESSAGE_SIZE) {
            printf("Send failed at iteration %lu\n", i);
            continue;
        }
        
        // Measure receive time
        uint32_t receive_start = get_time_us();
        uint32_t received = pico_rtos_stream_buffer_receive(&test_ctx.stream,
                                                           test_ctx.receive_buffer,
                                                           sizeof(test_ctx.receive_buffer),
                                                           PICO_RTOS_NO_WAIT);
        uint32_t receive_end = get_time_us();
        
        if (received != LARGE_MESSAGE_SIZE) {
            printf("Receive failed at iteration %lu (received %lu bytes)\n", i, received);
            continue;
        }
        
        // Verify data integrity
        if (!verify_test_data(test_ctx.receive_buffer, LARGE_MESSAGE_SIZE, 0xAA)) {
            printf("Data corruption detected at iteration %lu\n", i);
            continue;
        }
        
        total_send_time += (send_end - send_start);
        total_receive_time += (receive_end - receive_start);
        successful_iterations++;
        
        // Progress indicator
        if ((i + 1) % 20 == 0) {
            printf("Completed %lu/%d iterations\n", i + 1, NUM_TEST_ITERATIONS);
        }
    }
    
    if (successful_iterations > 0) {
        test_ctx.standard_send_time_us = total_send_time / successful_iterations;
        test_ctx.standard_receive_time_us = total_receive_time / successful_iterations;
        
        printf("Standard operations results (%lu successful iterations):\n", successful_iterations);
        printf("  Average send time: %lu us\n", test_ctx.standard_send_time_us);
        printf("  Average receive time: %lu us\n", test_ctx.standard_receive_time_us);
        printf("  Total time per message: %lu us\n", 
               test_ctx.standard_send_time_us + test_ctx.standard_receive_time_us);
    } else {
        printf("ERROR: No successful standard operations\n");
    }
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&test_ctx.stream);
}

// =============================================================================
// ZERO-COPY OPERATION PERFORMANCE TESTS
// =============================================================================

static void test_zero_copy_operations_performance(void) {
    printf("\n--- Testing Zero-Copy Operations Performance ---\n");
    
    // Initialize stream buffer
    bool result = pico_rtos_stream_buffer_init(&test_ctx.stream, 
                                              test_ctx.buffer, 
                                              sizeof(test_ctx.buffer));
    if (!result) {
        printf("ERROR: Failed to initialize stream buffer\n");
        return;
    }
    
    // Enable zero-copy for this test
    pico_rtos_stream_buffer_set_zero_copy(&test_ctx.stream, true);
    
    printf("Testing zero-copy operations with %d byte messages...\n", LARGE_MESSAGE_SIZE);
    
    uint32_t total_send_time = 0;
    uint32_t total_receive_time = 0;
    uint32_t successful_iterations = 0;
    
    for (uint32_t i = 0; i < NUM_TEST_ITERATIONS; i++) {
        pico_rtos_stream_buffer_zero_copy_t send_zero_copy;
        pico_rtos_stream_buffer_zero_copy_t receive_zero_copy;
        
        // Measure zero-copy send time
        uint32_t send_start = get_time_us();
        
        // Start zero-copy send
        bool send_started = pico_rtos_stream_buffer_zero_copy_send_start(&test_ctx.stream,
                                                                        LARGE_MESSAGE_SIZE,
                                                                        &send_zero_copy,
                                                                        PICO_RTOS_NO_WAIT);
        if (!send_started) {
            printf("Zero-copy send start failed at iteration %lu\n", i);
            continue;
        }
        
        // Fill data directly in buffer
        fill_test_data((uint8_t *)send_zero_copy.buffer, LARGE_MESSAGE_SIZE, 0xBB);
        
        // Complete zero-copy send
        bool send_completed = pico_rtos_stream_buffer_zero_copy_send_complete(&test_ctx.stream,
                                                                             &send_zero_copy,
                                                                             LARGE_MESSAGE_SIZE);
        uint32_t send_end = get_time_us();
        
        if (!send_completed) {
            printf("Zero-copy send complete failed at iteration %lu\n", i);
            continue;
        }
        
        // Measure zero-copy receive time
        uint32_t receive_start = get_time_us();
        
        // Start zero-copy receive
        bool receive_started = pico_rtos_stream_buffer_zero_copy_receive_start(&test_ctx.stream,
                                                                              &receive_zero_copy,
                                                                              PICO_RTOS_NO_WAIT);
        if (!receive_started) {
            printf("Zero-copy receive start failed at iteration %lu\n", i);
            continue;
        }
        
        // Verify data directly from buffer
        bool data_valid = verify_test_data((const uint8_t *)receive_zero_copy.buffer, 
                                          receive_zero_copy.size, 0xBB);
        
        // Complete zero-copy receive
        bool receive_completed = pico_rtos_stream_buffer_zero_copy_receive_complete(&test_ctx.stream,
                                                                                   &receive_zero_copy);
        uint32_t receive_end = get_time_us();
        
        if (!receive_completed) {
            printf("Zero-copy receive complete failed at iteration %lu\n", i);
            continue;
        }
        
        if (!data_valid) {
            printf("Data corruption detected at iteration %lu\n", i);
            continue;
        }
        
        total_send_time += (send_end - send_start);
        total_receive_time += (receive_end - receive_start);
        successful_iterations++;
        
        // Progress indicator
        if ((i + 1) % 20 == 0) {
            printf("Completed %lu/%d iterations\n", i + 1, NUM_TEST_ITERATIONS);
        }
    }
    
    if (successful_iterations > 0) {
        test_ctx.zero_copy_send_time_us = total_send_time / successful_iterations;
        test_ctx.zero_copy_receive_time_us = total_receive_time / successful_iterations;
        
        printf("Zero-copy operations results (%lu successful iterations):\n", successful_iterations);
        printf("  Average send time: %lu us\n", test_ctx.zero_copy_send_time_us);
        printf("  Average receive time: %lu us\n", test_ctx.zero_copy_receive_time_us);
        printf("  Total time per message: %lu us\n", 
               test_ctx.zero_copy_send_time_us + test_ctx.zero_copy_receive_time_us);
    } else {
        printf("ERROR: No successful zero-copy operations\n");
    }
    
    // Cleanup
    pico_rtos_stream_buffer_delete(&test_ctx.stream);
}

// =============================================================================
// PERFORMANCE COMPARISON AND ANALYSIS
// =============================================================================

static void analyze_performance_results(void) {
    printf("\n=== PERFORMANCE ANALYSIS ===\n");
    
    if (test_ctx.standard_send_time_us == 0 || test_ctx.zero_copy_send_time_us == 0) {
        printf("ERROR: Incomplete performance data\n");
        return;
    }
    
    uint32_t standard_total = test_ctx.standard_send_time_us + test_ctx.standard_receive_time_us;
    uint32_t zero_copy_total = test_ctx.zero_copy_send_time_us + test_ctx.zero_copy_receive_time_us;
    
    printf("Message size: %d bytes\n", LARGE_MESSAGE_SIZE);
    printf("Test iterations: %d\n", NUM_TEST_ITERATIONS);
    printf("\nTiming Comparison:\n");
    printf("                    Standard    Zero-Copy    Improvement\n");
    printf("Send time (us):     %8lu    %8lu    ", 
           test_ctx.standard_send_time_us, test_ctx.zero_copy_send_time_us);
    
    if (test_ctx.zero_copy_send_time_us < test_ctx.standard_send_time_us) {
        float improvement = ((float)(test_ctx.standard_send_time_us - test_ctx.zero_copy_send_time_us) / 
                            test_ctx.standard_send_time_us) * 100.0f;
        printf("%.1f%%\n", improvement);
    } else {
        float degradation = ((float)(test_ctx.zero_copy_send_time_us - test_ctx.standard_send_time_us) / 
                            test_ctx.standard_send_time_us) * 100.0f;
        printf("-%.1f%%\n", degradation);
    }
    
    printf("Receive time (us):  %8lu    %8lu    ", 
           test_ctx.standard_receive_time_us, test_ctx.zero_copy_receive_time_us);
    
    if (test_ctx.zero_copy_receive_time_us < test_ctx.standard_receive_time_us) {
        float improvement = ((float)(test_ctx.standard_receive_time_us - test_ctx.zero_copy_receive_time_us) / 
                            test_ctx.standard_receive_time_us) * 100.0f;
        printf("%.1f%%\n", improvement);
    } else {
        float degradation = ((float)(test_ctx.zero_copy_receive_time_us - test_ctx.standard_receive_time_us) / 
                            test_ctx.standard_receive_time_us) * 100.0f;
        printf("-%.1f%%\n", degradation);
    }
    
    printf("Total time (us):    %8lu    %8lu    ", standard_total, zero_copy_total);
    
    if (zero_copy_total < standard_total) {
        float improvement = ((float)(standard_total - zero_copy_total) / standard_total) * 100.0f;
        printf("%.1f%%\n", improvement);
    } else {
        float degradation = ((float)(zero_copy_total - standard_total) / standard_total) * 100.0f;
        printf("-%.1f%%\n", degradation);
    }
    
    // Calculate throughput
    uint32_t standard_throughput_mbps = (uint32_t)(((uint64_t)LARGE_MESSAGE_SIZE * 8 * 1000000) / (standard_total * 1024 * 1024));
    uint32_t zero_copy_throughput_mbps = (uint32_t)(((uint64_t)LARGE_MESSAGE_SIZE * 8 * 1000000) / (zero_copy_total * 1024 * 1024));
    
    printf("\nThroughput Analysis:\n");
    printf("Standard operations: %lu Mbps\n", standard_throughput_mbps);
    printf("Zero-copy operations: %lu Mbps\n", zero_copy_throughput_mbps);
    
    // Memory efficiency analysis
    printf("\nMemory Efficiency:\n");
    printf("Standard operations: 2x memory usage (source + buffer copy)\n");
    printf("Zero-copy operations: 1x memory usage (direct buffer access)\n");
    
    // Recommendations
    printf("\nRecommendations:\n");
    if (zero_copy_total < standard_total) {
        printf("✓ Zero-copy provides performance benefit for %d byte messages\n", LARGE_MESSAGE_SIZE);
        printf("✓ Consider using zero-copy for messages >= %d bytes\n", PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD);
    } else {
        printf("⚠ Zero-copy overhead exceeds benefits for %d byte messages\n", LARGE_MESSAGE_SIZE);
        printf("⚠ Consider increasing zero-copy threshold or optimizing implementation\n");
    }
    
    printf("✓ Zero-copy reduces memory pressure and eliminates data copying\n");
    printf("✓ Zero-copy is most beneficial for large, frequent message transfers\n");
}

// =============================================================================
// THRESHOLD ANALYSIS TEST
// =============================================================================

static void test_threshold_analysis(void) {
    printf("\n--- Testing Zero-Copy Threshold Analysis ---\n");
    
    uint32_t test_sizes[] = {32, 64, 128, 256, 512, 1024};
    uint32_t num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    printf("Message Size | Standard (us) | Zero-Copy (us) | Improvement\n");
    printf("-------------|---------------|----------------|------------\n");
    
    for (uint32_t s = 0; s < num_sizes; s++) {
        uint32_t message_size = test_sizes[s];
        
        if (message_size > STREAM_BUFFER_SIZE / 4) {
            printf("%12lu | Too large for buffer\n", message_size);
            continue;
        }
        
        // Test standard operations
        bool result = pico_rtos_stream_buffer_init(&test_ctx.stream, 
                                                  test_ctx.buffer, 
                                                  sizeof(test_ctx.buffer));
        if (!result) continue;
        
        pico_rtos_stream_buffer_set_zero_copy(&test_ctx.stream, false);
        fill_test_data(test_ctx.test_data, message_size, 0xCC);
        
        uint32_t standard_time = 0;
        uint32_t iterations = 10;
        
        for (uint32_t i = 0; i < iterations; i++) {
            uint32_t start = get_time_us();
            
            pico_rtos_stream_buffer_send(&test_ctx.stream, test_ctx.test_data, message_size, PICO_RTOS_NO_WAIT);
            pico_rtos_stream_buffer_receive(&test_ctx.stream, test_ctx.receive_buffer, sizeof(test_ctx.receive_buffer), PICO_RTOS_NO_WAIT);
            
            uint32_t end = get_time_us();
            standard_time += (end - start);
        }
        standard_time /= iterations;
        
        pico_rtos_stream_buffer_delete(&test_ctx.stream);
        
        // Test zero-copy operations (only if above threshold)
        uint32_t zero_copy_time = 0;
        if (message_size >= PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD) {
            result = pico_rtos_stream_buffer_init(&test_ctx.stream, 
                                                 test_ctx.buffer, 
                                                 sizeof(test_ctx.buffer));
            if (!result) continue;
            
            pico_rtos_stream_buffer_set_zero_copy(&test_ctx.stream, true);
            
            for (uint32_t i = 0; i < iterations; i++) {
                pico_rtos_stream_buffer_zero_copy_t send_zc, recv_zc;
                
                uint32_t start = get_time_us();
                
                if (pico_rtos_stream_buffer_zero_copy_send_start(&test_ctx.stream, message_size, &send_zc, PICO_RTOS_NO_WAIT)) {
                    fill_test_data((uint8_t *)send_zc.buffer, message_size, 0xCC);
                    pico_rtos_stream_buffer_zero_copy_send_complete(&test_ctx.stream, &send_zc, message_size);
                    
                    if (pico_rtos_stream_buffer_zero_copy_receive_start(&test_ctx.stream, &recv_zc, PICO_RTOS_NO_WAIT)) {
                        pico_rtos_stream_buffer_zero_copy_receive_complete(&test_ctx.stream, &recv_zc);
                    }
                }
                
                uint32_t end = get_time_us();
                zero_copy_time += (end - start);
            }
            zero_copy_time /= iterations;
            
            pico_rtos_stream_buffer_delete(&test_ctx.stream);
        }
        
        // Print results
        printf("%12lu | %13lu | ", message_size, standard_time);
        
        if (zero_copy_time > 0) {
            printf("%14lu | ", zero_copy_time);
            if (zero_copy_time < standard_time) {
                float improvement = ((float)(standard_time - zero_copy_time) / standard_time) * 100.0f;
                printf("%.1f%%", improvement);
            } else {
                float degradation = ((float)(zero_copy_time - standard_time) / standard_time) * 100.0f;
                printf("-%.1f%%", degradation);
            }
        } else {
            printf("      N/A      | Below threshold");
        }
        printf("\n");
    }
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

static void run_performance_tests(void) {
    printf("\n========================================\n");
    printf("Stream Buffer Performance Test\n");
    printf("========================================\n");
    
    // Initialize test context
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    printf("Configuration:\n");
    printf("  Buffer size: %d bytes\n", STREAM_BUFFER_SIZE);
    printf("  Large message size: %d bytes\n", LARGE_MESSAGE_SIZE);
    printf("  Zero-copy threshold: %d bytes\n", PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD);
    printf("  Test iterations: %d\n", NUM_TEST_ITERATIONS);
    
    // Run performance tests
    test_standard_operations_performance();
    test_zero_copy_operations_performance();
    analyze_performance_results();
    test_threshold_analysis();
    
    printf("\n========================================\n");
    printf("Performance Test Complete\n");
    printf("========================================\n");
    
    test_ctx.test_complete = true;
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Wait a moment for USB serial to be ready
    sleep_ms(2000);
    
    printf("Stream Buffer Performance Test Starting...\n");
    
    // Initialize Pico-RTOS
    if (!pico_rtos_init()) {
        printf("ERROR: Failed to initialize Pico-RTOS\n");
        return -1;
    }
    
    // Run performance tests
    run_performance_tests();
    
    // Keep running to show results
    while (!test_ctx.test_complete) {
        sleep_ms(1000);
    }
    
    printf("Test completed. System will continue running...\n");
    
    // Start scheduler (though we don't need it for this test)
    pico_rtos_start();
    
    return 0;
}