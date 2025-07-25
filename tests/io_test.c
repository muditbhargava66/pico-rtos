/**
 * @file io_test.c
 * @brief Unit tests for thread-safe I/O abstraction layer
 */

#include "pico_rtos/io.h"
#include "pico_rtos/task.h"
#include "pico_rtos.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// =============================================================================
// TEST DEVICE IMPLEMENTATION
// =============================================================================

typedef struct {
    uint8_t buffer[256];
    size_t buffer_size;
    size_t read_pos;
    size_t write_pos;
    bool initialized;
    uint32_t status;
} test_device_data_t;

static test_device_data_t test_device_data = {0};

static bool test_device_init(pico_rtos_io_device_t *device, const void *config)
{
    (void)config;
    test_device_data_t *data = (test_device_data_t *)device->private_data;
    if (data == NULL) {
        device->private_data = &test_device_data;
        data = &test_device_data;
    }
    
    memset(data->buffer, 0, sizeof(data->buffer));
    data->buffer_size = sizeof(data->buffer);
    data->read_pos = 0;
    data->write_pos = 0;
    data->initialized = true;
    data->status = 0x12345678;
    
    return true;
}

static void test_device_deinit(pico_rtos_io_device_t *device)
{
    test_device_data_t *data = (test_device_data_t *)device->private_data;
    if (data != NULL) {
        data->initialized = false;
    }
}

static pico_rtos_io_error_t test_device_read(pico_rtos_io_device_t *device,
                                            void *buffer,
                                            size_t size,
                                            size_t *bytes_read,
                                            uint32_t timeout)
{
    (void)timeout;
    test_device_data_t *data = (test_device_data_t *)device->private_data;
    
    if (!data->initialized) {
        return PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED;
    }
    
    size_t available = (data->write_pos >= data->read_pos) ? 
                      (data->write_pos - data->read_pos) :
                      (data->buffer_size - data->read_pos + data->write_pos);
    
    size_t to_read = (size < available) ? size : available;
    
    if (to_read == 0) {
        *bytes_read = 0;
        return PICO_RTOS_IO_ERROR_NONE;
    }
    
    uint8_t *dst = (uint8_t *)buffer;
    for (size_t i = 0; i < to_read; i++) {
        dst[i] = data->buffer[data->read_pos];
        data->read_pos = (data->read_pos + 1) % data->buffer_size;
    }
    
    *bytes_read = to_read;
    return PICO_RTOS_IO_ERROR_NONE;
}

static pico_rtos_io_error_t test_device_write(pico_rtos_io_device_t *device,
                                             const void *buffer,
                                             size_t size,
                                             size_t *bytes_written,
                                             uint32_t timeout)
{
    (void)timeout;
    test_device_data_t *data = (test_device_data_t *)device->private_data;
    
    if (!data->initialized) {
        return PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED;
    }
    
    size_t space = (data->read_pos > data->write_pos) ?
                  (data->read_pos - data->write_pos - 1) :
                  (data->buffer_size - data->write_pos + data->read_pos - 1);
    
    size_t to_write = (size < space) ? size : space;
    
    if (to_write == 0) {
        *bytes_written = 0;
        return PICO_RTOS_IO_ERROR_DEVICE_BUSY;
    }
    
    const uint8_t *src = (const uint8_t *)buffer;
    for (size_t i = 0; i < to_write; i++) {
        data->buffer[data->write_pos] = src[i];
        data->write_pos = (data->write_pos + 1) % data->buffer_size;
    }
    
    *bytes_written = to_write;
    return PICO_RTOS_IO_ERROR_NONE;
}

static pico_rtos_io_error_t test_device_control(pico_rtos_io_device_t *device,
                                               uint32_t command,
                                               void *arg)
{
    test_device_data_t *data = (test_device_data_t *)device->private_data;
    
    if (!data->initialized) {
        return PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED;
    }
    
    switch (command) {
        case 1: // Reset buffer
            data->read_pos = 0;
            data->write_pos = 0;
            memset(data->buffer, 0, sizeof(data->buffer));
            break;
        case 2: // Set status
            if (arg != NULL) {
                data->status = *(uint32_t *)arg;
            }
            break;
        default:
            return PICO_RTOS_IO_ERROR_INVALID_OPERATION;
    }
    
    return PICO_RTOS_IO_ERROR_NONE;
}

static pico_rtos_io_error_t test_device_status(pico_rtos_io_device_t *device,
                                              uint32_t *status)
{
    test_device_data_t *data = (test_device_data_t *)device->private_data;
    
    if (!data->initialized) {
        return PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED;
    }
    
    *status = data->status;
    return PICO_RTOS_IO_ERROR_NONE;
}

static const pico_rtos_io_device_ops_t test_device_ops = {
    .init = test_device_init,
    .deinit = test_device_deinit,
    .read = test_device_read,
    .write = test_device_write,
    .control = test_device_control,
    .status = test_device_status
};

// =============================================================================
// TEST FUNCTIONS
// =============================================================================

static void test_io_init(void)
{
    printf("Testing I/O subsystem initialization...\n");
    
    // Test initialization
    assert(pico_rtos_io_init() == true);
    
    // Test double initialization
    assert(pico_rtos_io_init() == true);
    
    printf("✓ I/O initialization test passed\n");
}

static void test_device_registration(void)
{
    printf("Testing device registration...\n");
    
    static pico_rtos_io_device_t test_device;
    
    // Test device registration
    assert(pico_rtos_io_register_device(&test_device, "test_uart", 
                                       PICO_RTOS_IO_DEVICE_UART,
                                       &test_device_ops, NULL) == true);
    
    // Test duplicate name registration
    static pico_rtos_io_device_t test_device2;
    assert(pico_rtos_io_register_device(&test_device2, "test_uart",
                                       PICO_RTOS_IO_DEVICE_UART,
                                       &test_device_ops, NULL) == false);
    
    // Test device finding
    pico_rtos_io_device_t *found = pico_rtos_io_find_device("test_uart");
    assert(found == &test_device);
    
    // Test device finding by type
    found = pico_rtos_io_find_device_by_type(PICO_RTOS_IO_DEVICE_UART, 0);
    assert(found == &test_device);
    
    // Test non-existent device
    found = pico_rtos_io_find_device("nonexistent");
    assert(found == NULL);
    
    printf("✓ Device registration test passed\n");
}

static void test_handle_operations(void)
{
    printf("Testing handle operations...\n");
    
    // Test handle opening
    pico_rtos_io_handle_t *handle = pico_rtos_io_open("test_uart", 
                                                     PICO_RTOS_IO_MODE_BLOCKING,
                                                     1000);
    assert(handle != NULL);
    assert(pico_rtos_io_is_handle_valid(handle) == true);
    
    // Test handle opening by type
    pico_rtos_io_handle_t *handle2 = pico_rtos_io_open_by_type(PICO_RTOS_IO_DEVICE_UART, 0,
                                                              PICO_RTOS_IO_MODE_NON_BLOCKING,
                                                              500);
    assert(handle2 != NULL);
    assert(pico_rtos_io_is_handle_valid(handle2) == true);
    
    // Test handle configuration
    assert(pico_rtos_io_set_mode(handle, PICO_RTOS_IO_MODE_TIMEOUT) == true);
    assert(pico_rtos_io_set_timeout(handle, 2000) == true);
    assert(pico_rtos_io_set_flags(handle, 0x1234) == true);
    assert(pico_rtos_io_get_flags(handle) == 0x1234);
    
    // Test handle closing
    assert(pico_rtos_io_close(handle2) == true);
    assert(pico_rtos_io_is_handle_valid(handle2) == false);
    
    // Test invalid handle operations
    assert(pico_rtos_io_close(handle2) == false);
    assert(pico_rtos_io_set_mode(handle2, PICO_RTOS_IO_MODE_BLOCKING) == false);
    
    // Keep one handle open for further tests
    printf("✓ Handle operations test passed\n");
}

static void test_io_operations(void)
{
    printf("Testing I/O operations...\n");
    
    pico_rtos_io_handle_t *handle = pico_rtos_io_open("test_uart",
                                                     PICO_RTOS_IO_MODE_BLOCKING,
                                                     1000);
    assert(handle != NULL);
    
    // Test write operation
    const char *test_data = "Hello, World!";
    size_t bytes_written;
    pico_rtos_io_error_t result = pico_rtos_io_write(handle, test_data, 
                                                    strlen(test_data), 
                                                    &bytes_written);
    assert(result == PICO_RTOS_IO_ERROR_NONE);
    assert(bytes_written == strlen(test_data));
    
    // Test read operation
    char read_buffer[64];
    size_t bytes_read;
    result = pico_rtos_io_read(handle, read_buffer, sizeof(read_buffer), &bytes_read);
    assert(result == PICO_RTOS_IO_ERROR_NONE);
    assert(bytes_read == strlen(test_data));
    assert(memcmp(read_buffer, test_data, bytes_read) == 0);
    
    // Test control operation
    result = pico_rtos_io_control(handle, 1, NULL); // Reset buffer
    assert(result == PICO_RTOS_IO_ERROR_NONE);
    
    // Test status operation
    uint32_t status;
    result = pico_rtos_io_get_status(handle, &status);
    assert(result == PICO_RTOS_IO_ERROR_NONE);
    assert(status == 0x12345678);
    
    // Test invalid operations
    result = pico_rtos_io_read(NULL, read_buffer, sizeof(read_buffer), &bytes_read);
    assert(result == PICO_RTOS_IO_ERROR_INVALID_PARAMETER);
    
    result = pico_rtos_io_write(handle, NULL, 10, &bytes_written);
    assert(result == PICO_RTOS_IO_ERROR_INVALID_PARAMETER);
    
    pico_rtos_io_close(handle);
    printf("✓ I/O operations test passed\n");
}

static void test_statistics(void)
{
    printf("Testing statistics...\n");
    
    pico_rtos_io_handle_t *handle = pico_rtos_io_open("test_uart",
                                                     PICO_RTOS_IO_MODE_BLOCKING,
                                                     1000);
    assert(handle != NULL);
    
    // Perform some operations
    const char *test_data = "Test data";
    size_t bytes_written;
    pico_rtos_io_write(handle, test_data, strlen(test_data), &bytes_written);
    
    char read_buffer[32];
    size_t bytes_read;
    pico_rtos_io_read(handle, read_buffer, sizeof(read_buffer), &bytes_read);
    
    pico_rtos_io_control(handle, 1, NULL);
    
    // Test device statistics
    pico_rtos_io_device_stats_t device_stats;
    assert(pico_rtos_io_get_device_stats(handle->device, &device_stats) == true);
    assert(device_stats.read_operations > 0);
    assert(device_stats.write_operations > 0);
    assert(device_stats.control_operations > 0);
    assert(device_stats.bytes_read > 0);
    assert(device_stats.bytes_written > 0);
    
    // Test handle statistics
    pico_rtos_io_handle_stats_t handle_stats;
    assert(pico_rtos_io_get_handle_stats(handle, &handle_stats) == true);
    assert(handle_stats.operations_count > 0);
    assert(handle_stats.bytes_transferred > 0);
    
    // Test statistics reset
    assert(pico_rtos_io_reset_device_stats(handle->device) == true);
    assert(pico_rtos_io_reset_handle_stats(handle) == true);
    
    pico_rtos_io_close(handle);
    printf("✓ Statistics test passed\n");
}

static void test_concurrent_access(void)
{
    printf("Testing concurrent access serialization...\n");
    
    // This test would ideally use multiple tasks, but for now we'll test
    // the basic serialization mechanism by opening multiple handles
    
    pico_rtos_io_handle_t *handle1 = pico_rtos_io_open("test_uart",
                                                      PICO_RTOS_IO_MODE_BLOCKING,
                                                      1000);
    pico_rtos_io_handle_t *handle2 = pico_rtos_io_open("test_uart",
                                                      PICO_RTOS_IO_MODE_NON_BLOCKING,
                                                      100);
    
    assert(handle1 != NULL);
    assert(handle2 != NULL);
    assert(handle1->device == handle2->device);
    assert(handle1->device->reference_count == 2);
    
    // Test that both handles can perform operations
    const char *data1 = "Data from handle 1";
    const char *data2 = "Data from handle 2";
    
    size_t bytes_written;
    assert(pico_rtos_io_write(handle1, data1, strlen(data1), &bytes_written) == PICO_RTOS_IO_ERROR_NONE);
    assert(pico_rtos_io_write(handle2, data2, strlen(data2), &bytes_written) == PICO_RTOS_IO_ERROR_NONE);
    
    char read_buffer[64];
    size_t bytes_read;
    assert(pico_rtos_io_read(handle1, read_buffer, sizeof(read_buffer), &bytes_read) == PICO_RTOS_IO_ERROR_NONE);
    
    pico_rtos_io_close(handle1);
    assert(handle1->device->reference_count == 1);
    
    pico_rtos_io_close(handle2);
    
    printf("✓ Concurrent access test passed\n");
}

static void test_utility_functions(void)
{
    printf("Testing utility functions...\n");
    
    // Test error string function
    const char *error_str = pico_rtos_io_get_error_string(PICO_RTOS_IO_ERROR_DEVICE_NOT_FOUND);
    assert(error_str != NULL);
    assert(strlen(error_str) > 0);
    
    // Test device type string function
    const char *type_str = pico_rtos_io_get_device_type_string(PICO_RTOS_IO_DEVICE_UART);
    assert(type_str != NULL);
    assert(strcmp(type_str, "UART") == 0);
    
    // Test mode string function
    const char *mode_str = pico_rtos_io_get_mode_string(PICO_RTOS_IO_MODE_BLOCKING);
    assert(mode_str != NULL);
    assert(strcmp(mode_str, "Blocking") == 0);
    
    printf("✓ Utility functions test passed\n");
}

// =============================================================================
// MAIN TEST FUNCTION
// =============================================================================

int main(void)
{
    printf("Starting I/O abstraction layer tests...\n\n");
    
    // Initialize RTOS (required for mutex operations)
    assert(pico_rtos_init() == true);
    
    // Run tests
    test_io_init();
    test_device_registration();
    test_handle_operations();
    test_io_operations();
    test_statistics();
    test_concurrent_access();
    test_utility_functions();
    
    printf("\n✓ All I/O abstraction layer tests passed!\n");
    return 0;
}

// Task function for testing (if needed)
void io_test_task(void *param)
{
    (void)param;
    main();
    pico_rtos_task_delete(NULL);
}