/**
 * @file io.c
 * @brief Thread-Safe I/O Abstraction Layer Implementation
 * 
 * This module implements a thread-safe abstraction layer for I/O operations,
 * providing automatic serialization and device management for Pico-RTOS.
 */

#include "pico_rtos/io.h"
#include "pico_rtos/task.h"
#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include <string.h>

// =============================================================================
// INTERNAL DATA STRUCTURES
// =============================================================================

/**
 * @brief Global I/O subsystem state
 */
typedef struct {
    bool initialized;
    pico_rtos_io_device_t *devices[PICO_RTOS_IO_MAX_DEVICES];
    pico_rtos_io_handle_t handles[PICO_RTOS_IO_MAX_HANDLES];
    uint32_t device_count;
    uint32_t next_device_id;
    uint32_t next_handle_id;
    pico_rtos_mutex_t global_mutex;
} pico_rtos_io_subsystem_t;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

static pico_rtos_io_subsystem_t g_io_subsystem = {0};

// =============================================================================
// ERROR DESCRIPTION STRINGS
// =============================================================================

static const char *io_error_strings[] = {
    "No error",
    "Device not found",
    "Device busy",
    "Device not initialized",
    "Invalid handle",
    "Invalid operation",
    "Timeout",
    "Buffer too small",
    "Device error",
    "Access denied",
    "Resource exhausted",
    "Invalid parameter",
    "Not supported"
};

static const char *device_type_strings[] = {
    "UART",
    "SPI",
    "I2C",
    "GPIO",
    "ADC",
    "PWM",
    "Timer",
    "Flash",
    "Custom"
};

static const char *mode_strings[] = {
    "Blocking",
    "Non-blocking",
    "Timeout"
};

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Find free device slot
 * @return Index of free slot, or -1 if none available
 */
static int find_free_device_slot(void)
{
    for (int i = 0; i < PICO_RTOS_IO_MAX_DEVICES; i++) {
        if (g_io_subsystem.devices[i] == NULL) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Find free handle slot
 * @return Index of free slot, or -1 if none available
 */
static int find_free_handle_slot(void)
{
    for (int i = 0; i < PICO_RTOS_IO_MAX_HANDLES; i++) {
        if (!g_io_subsystem.handles[i].valid) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Validate device pointer
 * @param device Device pointer to validate
 * @return true if valid, false otherwise
 */
static bool is_device_valid(pico_rtos_io_device_t *device)
{
    if (device == NULL) return false;
    
    for (int i = 0; i < PICO_RTOS_IO_MAX_DEVICES; i++) {
        if (g_io_subsystem.devices[i] == device) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Validate handle pointer
 * @param handle Handle pointer to validate
 * @return true if valid, false otherwise
 */
static bool is_handle_valid_internal(pico_rtos_io_handle_t *handle)
{
    if (handle == NULL) return false;
    if (!handle->valid) return false;
    
    // Check if handle is within our handle array
    if (handle < g_io_subsystem.handles || 
        handle >= g_io_subsystem.handles + PICO_RTOS_IO_MAX_HANDLES) {
        return false;
    }
    
    return is_device_valid(handle->device);
}

/**
 * @brief Update device statistics
 * @param device Device to update
 * @param operation Operation type
 * @param bytes Number of bytes transferred
 * @param error Error code (0 for success)
 */
static void update_device_stats(pico_rtos_io_device_t *device,
                               pico_rtos_io_operation_t operation,
                               size_t bytes,
                               pico_rtos_io_error_t error)
{
    if (device == NULL) return;
    
    device->last_operation_time = pico_rtos_get_tick_count();
    
    if (error != PICO_RTOS_IO_ERROR_NONE) {
        device->error_count++;
        device->last_error = error;
    }
    
    switch (operation) {
        case PICO_RTOS_IO_OP_READ:
            device->read_operations++;
            device->bytes_read += bytes;
            break;
        case PICO_RTOS_IO_OP_WRITE:
            device->write_operations++;
            device->bytes_written += bytes;
            break;
        case PICO_RTOS_IO_OP_CONTROL:
            device->control_operations++;
            break;
        default:
            break;
    }
}

/**
 * @brief Update handle statistics
 * @param handle Handle to update
 * @param bytes Number of bytes transferred
 * @param error Error code (0 for success)
 */
static void update_handle_stats(pico_rtos_io_handle_t *handle,
                               size_t bytes,
                               pico_rtos_io_error_t error)
{
    if (handle == NULL) return;
    
    handle->last_access_time = pico_rtos_get_tick_count();
    handle->operations_count++;
    handle->bytes_transferred += bytes;
    
    if (error != PICO_RTOS_IO_ERROR_NONE) {
        handle->errors++;
    }
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_io_init(void)
{
    if (g_io_subsystem.initialized) {
        return true;
    }
    
    // Initialize global mutex
    if (!pico_rtos_mutex_init(&g_io_subsystem.global_mutex)) {
        PICO_RTOS_LOG_ERROR("Failed to initialize I/O global mutex");
        return false;
    }
    
    // Clear device array
    memset(g_io_subsystem.devices, 0, sizeof(g_io_subsystem.devices));
    
    // Clear handle array
    memset(g_io_subsystem.handles, 0, sizeof(g_io_subsystem.handles));
    
    // Initialize counters
    g_io_subsystem.device_count = 0;
    g_io_subsystem.next_device_id = 1;
    g_io_subsystem.next_handle_id = 1;
    
    g_io_subsystem.initialized = true;
    
    PICO_RTOS_LOG_INFO("I/O subsystem initialized");
    return true;
}

bool pico_rtos_io_register_device(pico_rtos_io_device_t *device,
                                 const char *name,
                                 pico_rtos_io_device_type_t type,
                                 const pico_rtos_io_device_ops_t *ops,
                                 void *device_handle)
{
    if (!g_io_subsystem.initialized) {
        PICO_RTOS_LOG_ERROR("I/O subsystem not initialized");
        return false;
    }
    
    if (device == NULL || name == NULL || ops == NULL) {
        PICO_RTOS_LOG_ERROR("Invalid parameters for device registration");
        return false;
    }
    
    // Acquire global lock
    if (!pico_rtos_mutex_lock(&g_io_subsystem.global_mutex, PICO_RTOS_WAIT_FOREVER)) {
        PICO_RTOS_LOG_ERROR("Failed to acquire global mutex");
        return false;
    }
    
    // Check if device name already exists
    for (int i = 0; i < PICO_RTOS_IO_MAX_DEVICES; i++) {
        if (g_io_subsystem.devices[i] != NULL &&
            strcmp(g_io_subsystem.devices[i]->name, name) == 0) {
            pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
            PICO_RTOS_LOG_ERROR("Device name '%s' already registered", name);
            return false;
        }
    }
    
    // Find free slot
    int slot = find_free_device_slot();
    if (slot < 0) {
        pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
        PICO_RTOS_LOG_ERROR("No free device slots available");
        return false;
    }
    
    // Initialize device structure
    memset(device, 0, sizeof(pico_rtos_io_device_t));
    device->name = name;
    device->type = type;
    device->device_id = g_io_subsystem.next_device_id++;
    device->device_handle = device_handle;
    device->ops = ops;
    device->initialized = false;
    device->reference_count = 0;
    device->flags = 0;
    
    // Initialize device mutex
    if (!pico_rtos_mutex_init(&device->access_mutex)) {
        pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
        PICO_RTOS_LOG_ERROR("Failed to initialize device mutex for '%s'", name);
        return false;
    }
    
    // Initialize device if init function provided
    if (ops->init != NULL) {
        if (!ops->init(device, NULL)) {
            pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
            PICO_RTOS_LOG_ERROR("Device initialization failed for '%s'", name);
            return false;
        }
    }
    
    device->initialized = true;
    
    // Register device
    g_io_subsystem.devices[slot] = device;
    g_io_subsystem.device_count++;
    
    pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
    
    PICO_RTOS_LOG_INFO("Registered device '%s' (type: %s, ID: %u)", 
                       name, device_type_strings[type], device->device_id);
    return true;
}

bool pico_rtos_io_unregister_device(pico_rtos_io_device_t *device)
{
    if (!g_io_subsystem.initialized || device == NULL) {
        return false;
    }
    
    // Acquire global lock
    if (!pico_rtos_mutex_lock(&g_io_subsystem.global_mutex, PICO_RTOS_WAIT_FOREVER)) {
        return false;
    }
    
    // Find device in registry
    int slot = -1;
    for (int i = 0; i < PICO_RTOS_IO_MAX_DEVICES; i++) {
        if (g_io_subsystem.devices[i] == device) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
        PICO_RTOS_LOG_ERROR("Device not found in registry");
        return false;
    }
    
    // Check if device has open handles
    if (device->reference_count > 0) {
        pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
        PICO_RTOS_LOG_ERROR("Cannot unregister device '%s' - %u handles still open", 
                           device->name, device->reference_count);
        return false;
    }
    
    // Deinitialize device if function provided
    if (device->ops->deinit != NULL) {
        device->ops->deinit(device);
    }
    
    // Remove from registry
    g_io_subsystem.devices[slot] = NULL;
    g_io_subsystem.device_count--;
    
    PICO_RTOS_LOG_INFO("Unregistered device '%s'", device->name);
    
    pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
    return true;
}

pico_rtos_io_device_t *pico_rtos_io_find_device(const char *name)
{
    if (!g_io_subsystem.initialized || name == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < PICO_RTOS_IO_MAX_DEVICES; i++) {
        if (g_io_subsystem.devices[i] != NULL &&
            strcmp(g_io_subsystem.devices[i]->name, name) == 0) {
            return g_io_subsystem.devices[i];
        }
    }
    
    return NULL;
}

pico_rtos_io_device_t *pico_rtos_io_find_device_by_type(pico_rtos_io_device_type_t type, 
                                                        uint32_t index)
{
    if (!g_io_subsystem.initialized) {
        return NULL;
    }
    
    uint32_t count = 0;
    for (int i = 0; i < PICO_RTOS_IO_MAX_DEVICES; i++) {
        if (g_io_subsystem.devices[i] != NULL &&
            g_io_subsystem.devices[i]->type == type) {
            if (count == index) {
                return g_io_subsystem.devices[i];
            }
            count++;
        }
    }
    
    return NULL;
}

bool pico_rtos_io_get_device_list(pico_rtos_io_device_t **devices,
                                 uint32_t max_devices,
                                 uint32_t *actual_count)
{
    if (!g_io_subsystem.initialized || devices == NULL || actual_count == NULL) {
        return false;
    }
    
    *actual_count = 0;
    
    for (int i = 0; i < PICO_RTOS_IO_MAX_DEVICES && *actual_count < max_devices; i++) {
        if (g_io_subsystem.devices[i] != NULL) {
            devices[*actual_count] = g_io_subsystem.devices[i];
            (*actual_count)++;
        }
    }
    
    return true;
}

pico_rtos_io_handle_t *pico_rtos_io_open(const char *device_name,
                                        pico_rtos_io_mode_t mode,
                                        uint32_t timeout)
{
    if (!g_io_subsystem.initialized || device_name == NULL) {
        return NULL;
    }
    
    // Find device
    pico_rtos_io_device_t *device = pico_rtos_io_find_device(device_name);
    if (device == NULL) {
        PICO_RTOS_LOG_ERROR("Device '%s' not found", device_name);
        return NULL;
    }
    
    // Acquire global lock
    if (!pico_rtos_mutex_lock(&g_io_subsystem.global_mutex, PICO_RTOS_WAIT_FOREVER)) {
        PICO_RTOS_LOG_ERROR("Failed to acquire global mutex");
        return NULL;
    }
    
    // Find free handle slot
    int slot = find_free_handle_slot();
    if (slot < 0) {
        pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
        PICO_RTOS_LOG_ERROR("No free handle slots available");
        return NULL;
    }
    
    // Initialize handle
    pico_rtos_io_handle_t *handle = &g_io_subsystem.handles[slot];
    memset(handle, 0, sizeof(pico_rtos_io_handle_t));
    
    handle->device = device;
    handle->handle_id = g_io_subsystem.next_handle_id++;
    handle->mode = mode;
    handle->timeout = (timeout == 0) ? PICO_RTOS_IO_DEFAULT_TIMEOUT : timeout;
    handle->flags = 0;
    handle->user_data = NULL;
    handle->valid = true;
    handle->created_time = pico_rtos_get_tick_count();
    handle->last_access_time = handle->created_time;
    
    // Increment device reference count
    device->reference_count++;
    
    pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
    
    PICO_RTOS_LOG_DEBUG("Opened handle %u for device '%s'", 
                        handle->handle_id, device_name);
    return handle;
}

pico_rtos_io_handle_t *pico_rtos_io_open_by_type(pico_rtos_io_device_type_t type,
                                                 uint32_t index,
                                                 pico_rtos_io_mode_t mode,
                                                 uint32_t timeout)
{
    pico_rtos_io_device_t *device = pico_rtos_io_find_device_by_type(type, index);
    if (device == NULL) {
        PICO_RTOS_LOG_ERROR("Device of type %s (index %u) not found", 
                           device_type_strings[type], index);
        return NULL;
    }
    
    return pico_rtos_io_open(device->name, mode, timeout);
}

bool pico_rtos_io_close(pico_rtos_io_handle_t *handle)
{
    if (!is_handle_valid_internal(handle)) {
        return false;
    }
    
    // Acquire global lock
    if (!pico_rtos_mutex_lock(&g_io_subsystem.global_mutex, PICO_RTOS_WAIT_FOREVER)) {
        return false;
    }
    
    // Decrement device reference count
    handle->device->reference_count--;
    
    // Mark handle as invalid
    handle->valid = false;
    
    PICO_RTOS_LOG_DEBUG("Closed handle %u for device '%s'", 
                        handle->handle_id, handle->device->name);
    
    pico_rtos_mutex_unlock(&g_io_subsystem.global_mutex);
    return true;
}

bool pico_rtos_io_is_handle_valid(pico_rtos_io_handle_t *handle)
{
    return is_handle_valid_internal(handle);
}

pico_rtos_io_error_t pico_rtos_io_read(pico_rtos_io_handle_t *handle,
                                      void *buffer,
                                      size_t size,
                                      size_t *bytes_read)
{
    return pico_rtos_io_read_timeout(handle, buffer, size, bytes_read, handle->timeout);
}

pico_rtos_io_error_t pico_rtos_io_read_timeout(pico_rtos_io_handle_t *handle,
                                              void *buffer,
                                              size_t size,
                                              size_t *bytes_read,
                                              uint32_t timeout)
{
    if (!is_handle_valid_internal(handle) || buffer == NULL || size == 0) {
        return PICO_RTOS_IO_ERROR_INVALID_PARAMETER;
    }
    
    pico_rtos_io_device_t *device = handle->device;
    
    if (!device->initialized) {
        return PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED;
    }
    
    if (device->ops->read == NULL) {
        return PICO_RTOS_IO_ERROR_NOT_SUPPORTED;
    }
    
    // Acquire device lock for serialization
    uint32_t lock_timeout = (handle->mode == PICO_RTOS_IO_MODE_NON_BLOCKING) ? 
                           PICO_RTOS_NO_WAIT : timeout;
    
    if (!pico_rtos_mutex_lock(&device->access_mutex, lock_timeout)) {
        return PICO_RTOS_IO_ERROR_DEVICE_BUSY;
    }
    
    // Perform read operation
    size_t actual_bytes = 0;
    pico_rtos_io_error_t result = device->ops->read(device, buffer, size, &actual_bytes, timeout);
    
    // Update statistics
    update_device_stats(device, PICO_RTOS_IO_OP_READ, actual_bytes, result);
    update_handle_stats(handle, actual_bytes, result);
    
    if (bytes_read != NULL) {
        *bytes_read = actual_bytes;
    }
    
    pico_rtos_mutex_unlock(&device->access_mutex);
    return result;
}

pico_rtos_io_error_t pico_rtos_io_write(pico_rtos_io_handle_t *handle,
                                       const void *buffer,
                                       size_t size,
                                       size_t *bytes_written)
{
    return pico_rtos_io_write_timeout(handle, buffer, size, bytes_written, handle->timeout);
}

pico_rtos_io_error_t pico_rtos_io_write_timeout(pico_rtos_io_handle_t *handle,
                                               const void *buffer,
                                               size_t size,
                                               size_t *bytes_written,
                                               uint32_t timeout)
{
    if (!is_handle_valid_internal(handle) || buffer == NULL || size == 0) {
        return PICO_RTOS_IO_ERROR_INVALID_PARAMETER;
    }
    
    pico_rtos_io_device_t *device = handle->device;
    
    if (!device->initialized) {
        return PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED;
    }
    
    if (device->ops->write == NULL) {
        return PICO_RTOS_IO_ERROR_NOT_SUPPORTED;
    }
    
    // Acquire device lock for serialization
    uint32_t lock_timeout = (handle->mode == PICO_RTOS_IO_MODE_NON_BLOCKING) ? 
                           PICO_RTOS_NO_WAIT : timeout;
    
    if (!pico_rtos_mutex_lock(&device->access_mutex, lock_timeout)) {
        return PICO_RTOS_IO_ERROR_DEVICE_BUSY;
    }
    
    // Perform write operation
    size_t actual_bytes = 0;
    pico_rtos_io_error_t result = device->ops->write(device, buffer, size, &actual_bytes, timeout);
    
    // Update statistics
    update_device_stats(device, PICO_RTOS_IO_OP_WRITE, actual_bytes, result);
    update_handle_stats(handle, actual_bytes, result);
    
    if (bytes_written != NULL) {
        *bytes_written = actual_bytes;
    }
    
    pico_rtos_mutex_unlock(&device->access_mutex);
    return result;
}

pico_rtos_io_error_t pico_rtos_io_control(pico_rtos_io_handle_t *handle,
                                         uint32_t command,
                                         void *arg)
{
    if (!is_handle_valid_internal(handle)) {
        return PICO_RTOS_IO_ERROR_INVALID_HANDLE;
    }
    
    pico_rtos_io_device_t *device = handle->device;
    
    if (!device->initialized) {
        return PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED;
    }
    
    if (device->ops->control == NULL) {
        return PICO_RTOS_IO_ERROR_NOT_SUPPORTED;
    }
    
    // Acquire device lock for serialization
    uint32_t lock_timeout = (handle->mode == PICO_RTOS_IO_MODE_NON_BLOCKING) ? 
                           PICO_RTOS_NO_WAIT : handle->timeout;
    
    if (!pico_rtos_mutex_lock(&device->access_mutex, lock_timeout)) {
        return PICO_RTOS_IO_ERROR_DEVICE_BUSY;
    }
    
    // Perform control operation
    pico_rtos_io_error_t result = device->ops->control(device, command, arg);
    
    // Update statistics
    update_device_stats(device, PICO_RTOS_IO_OP_CONTROL, 0, result);
    update_handle_stats(handle, 0, result);
    
    pico_rtos_mutex_unlock(&device->access_mutex);
    return result;
}

pico_rtos_io_error_t pico_rtos_io_get_status(pico_rtos_io_handle_t *handle,
                                            uint32_t *status)
{
    if (!is_handle_valid_internal(handle) || status == NULL) {
        return PICO_RTOS_IO_ERROR_INVALID_PARAMETER;
    }
    
    pico_rtos_io_device_t *device = handle->device;
    
    if (!device->initialized) {
        return PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED;
    }
    
    if (device->ops->status == NULL) {
        return PICO_RTOS_IO_ERROR_NOT_SUPPORTED;
    }
    
    // Status operations typically don't need serialization
    pico_rtos_io_error_t result = device->ops->status(device, status);
    
    // Update statistics
    update_device_stats(device, PICO_RTOS_IO_OP_STATUS, 0, result);
    update_handle_stats(handle, 0, result);
    
    return result;
}

pico_rtos_io_error_t pico_rtos_io_flush(pico_rtos_io_handle_t *handle)
{
    // Flush is typically implemented as a control operation
    return pico_rtos_io_control(handle, 0xFFFFFFFF, NULL); // Special flush command
}

// Configuration API implementations
bool pico_rtos_io_set_mode(pico_rtos_io_handle_t *handle, pico_rtos_io_mode_t mode)
{
    if (!is_handle_valid_internal(handle)) {
        return false;
    }
    
    handle->mode = mode;
    return true;
}

bool pico_rtos_io_set_timeout(pico_rtos_io_handle_t *handle, uint32_t timeout)
{
    if (!is_handle_valid_internal(handle)) {
        return false;
    }
    
    handle->timeout = timeout;
    return true;
}

bool pico_rtos_io_set_flags(pico_rtos_io_handle_t *handle, uint32_t flags)
{
    if (!is_handle_valid_internal(handle)) {
        return false;
    }
    
    handle->flags = flags;
    return true;
}

uint32_t pico_rtos_io_get_flags(pico_rtos_io_handle_t *handle)
{
    if (!is_handle_valid_internal(handle)) {
        return 0;
    }
    
    return handle->flags;
}

// Statistics API implementations
bool pico_rtos_io_get_device_stats(pico_rtos_io_device_t *device,
                                  pico_rtos_io_device_stats_t *stats)
{
    if (!is_device_valid(device) || stats == NULL) {
        return false;
    }
    
    stats->read_operations = device->read_operations;
    stats->write_operations = device->write_operations;
    stats->control_operations = device->control_operations;
    stats->error_count = device->error_count;
    stats->bytes_read = device->bytes_read;
    stats->bytes_written = device->bytes_written;
    stats->reference_count = device->reference_count;
    stats->last_error = device->last_error;
    stats->last_operation_time = device->last_operation_time;
    stats->uptime = pico_rtos_get_tick_count();
    
    return true;
}

bool pico_rtos_io_get_handle_stats(pico_rtos_io_handle_t *handle,
                                  pico_rtos_io_handle_stats_t *stats)
{
    if (!is_handle_valid_internal(handle) || stats == NULL) {
        return false;
    }
    
    uint32_t current_time = pico_rtos_get_tick_count();
    
    stats->operations_count = handle->operations_count;
    stats->bytes_transferred = handle->bytes_transferred;
    stats->errors = handle->errors;
    stats->created_time = handle->created_time;
    stats->last_access_time = handle->last_access_time;
    stats->lifetime = current_time - handle->created_time;
    
    return true;
}

bool pico_rtos_io_reset_device_stats(pico_rtos_io_device_t *device)
{
    if (!is_device_valid(device)) {
        return false;
    }
    
    device->read_operations = 0;
    device->write_operations = 0;
    device->control_operations = 0;
    device->error_count = 0;
    device->bytes_read = 0;
    device->bytes_written = 0;
    device->last_error = 0;
    device->last_operation_time = pico_rtos_get_tick_count();
    
    return true;
}

bool pico_rtos_io_reset_handle_stats(pico_rtos_io_handle_t *handle)
{
    if (!is_handle_valid_internal(handle)) {
        return false;
    }
    
    handle->operations_count = 0;
    handle->bytes_transferred = 0;
    handle->errors = 0;
    handle->last_access_time = pico_rtos_get_tick_count();
    
    return true;
}

// Utility functions
const char *pico_rtos_io_get_error_string(pico_rtos_io_error_t error)
{
    if (error >= PICO_RTOS_IO_ERROR_DEVICE_NOT_FOUND && 
        error <= PICO_RTOS_IO_ERROR_NOT_SUPPORTED) {
        int index = error - PICO_RTOS_IO_ERROR_DEVICE_NOT_FOUND + 1;
        if (index < sizeof(io_error_strings) / sizeof(io_error_strings[0])) {
            return io_error_strings[index];
        }
    }
    
    return io_error_strings[0]; // "No error"
}

const char *pico_rtos_io_get_device_type_string(pico_rtos_io_device_type_t type)
{
    if (type < sizeof(device_type_strings) / sizeof(device_type_strings[0])) {
        return device_type_strings[type];
    }
    return "Unknown";
}

const char *pico_rtos_io_get_mode_string(pico_rtos_io_mode_t mode)
{
    if (mode < sizeof(mode_strings) / sizeof(mode_strings[0])) {
        return mode_strings[mode];
    }
    return "Unknown";
}