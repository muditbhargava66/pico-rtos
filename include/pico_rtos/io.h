#ifndef PICO_RTOS_IO_H
#define PICO_RTOS_IO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "pico_rtos/mutex.h"
#include "pico_rtos/error.h"
#include "pico_rtos/config.h"

/**
 * @file io.h
 * @brief Thread-Safe I/O Abstraction Layer for Pico-RTOS
 * 
 * This module provides a thread-safe abstraction layer for I/O operations,
 * ensuring that multiple tasks can safely access I/O resources without
 * conflicts. It provides automatic serialization and device management.
 */

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef PICO_RTOS_IO_MAX_DEVICES
#define PICO_RTOS_IO_MAX_DEVICES 16
#endif

#ifndef PICO_RTOS_IO_MAX_HANDLES
#define PICO_RTOS_IO_MAX_HANDLES 32
#endif

#ifndef PICO_RTOS_IO_DEFAULT_TIMEOUT
#define PICO_RTOS_IO_DEFAULT_TIMEOUT 1000
#endif

// =============================================================================
// DEVICE TYPE DEFINITIONS
// =============================================================================

/**
 * @brief I/O device types
 */
typedef enum {
    PICO_RTOS_IO_DEVICE_UART = 0,
    PICO_RTOS_IO_DEVICE_SPI,
    PICO_RTOS_IO_DEVICE_I2C,
    PICO_RTOS_IO_DEVICE_GPIO,
    PICO_RTOS_IO_DEVICE_ADC,
    PICO_RTOS_IO_DEVICE_PWM,
    PICO_RTOS_IO_DEVICE_TIMER,
    PICO_RTOS_IO_DEVICE_FLASH,
    PICO_RTOS_IO_DEVICE_CUSTOM
} pico_rtos_io_device_type_t;

/**
 * @brief I/O operation modes
 */
typedef enum {
    PICO_RTOS_IO_MODE_BLOCKING = 0,
    PICO_RTOS_IO_MODE_NON_BLOCKING,
    PICO_RTOS_IO_MODE_TIMEOUT
} pico_rtos_io_mode_t;

/**
 * @brief I/O operation types
 */
typedef enum {
    PICO_RTOS_IO_OP_READ = 0,
    PICO_RTOS_IO_OP_WRITE,
    PICO_RTOS_IO_OP_CONTROL,
    PICO_RTOS_IO_OP_STATUS
} pico_rtos_io_operation_t;

// =============================================================================
// ERROR CODES
// =============================================================================

/**
 * @brief I/O specific error codes (700-799 range)
 */
typedef enum {
    PICO_RTOS_IO_ERROR_NONE = 0,
    PICO_RTOS_IO_ERROR_DEVICE_NOT_FOUND = 700,
    PICO_RTOS_IO_ERROR_DEVICE_BUSY = 701,
    PICO_RTOS_IO_ERROR_DEVICE_NOT_INITIALIZED = 702,
    PICO_RTOS_IO_ERROR_INVALID_HANDLE = 703,
    PICO_RTOS_IO_ERROR_INVALID_OPERATION = 704,
    PICO_RTOS_IO_ERROR_TIMEOUT = 705,
    PICO_RTOS_IO_ERROR_BUFFER_TOO_SMALL = 706,
    PICO_RTOS_IO_ERROR_DEVICE_ERROR = 707,
    PICO_RTOS_IO_ERROR_ACCESS_DENIED = 708,
    PICO_RTOS_IO_ERROR_RESOURCE_EXHAUSTED = 709,
    PICO_RTOS_IO_ERROR_INVALID_PARAMETER = 710,
    PICO_RTOS_IO_ERROR_NOT_SUPPORTED = 711
} pico_rtos_io_error_t;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

typedef struct pico_rtos_io_device pico_rtos_io_device_t;
typedef struct pico_rtos_io_handle pico_rtos_io_handle_t;

// =============================================================================
// DEVICE OPERATIONS INTERFACE
// =============================================================================

/**
 * @brief Device operation function types
 */
typedef struct {
    /**
     * @brief Initialize device
     * @param device Pointer to device structure
     * @param config Device-specific configuration
     * @return true if successful, false otherwise
     */
    bool (*init)(pico_rtos_io_device_t *device, const void *config);
    
    /**
     * @brief Deinitialize device
     * @param device Pointer to device structure
     */
    void (*deinit)(pico_rtos_io_device_t *device);
    
    /**
     * @brief Read data from device
     * @param device Pointer to device structure
     * @param buffer Buffer to store read data
     * @param size Number of bytes to read
     * @param bytes_read Pointer to store actual bytes read
     * @param timeout Timeout in milliseconds
     * @return I/O error code
     */
    pico_rtos_io_error_t (*read)(pico_rtos_io_device_t *device, 
                                void *buffer, 
                                size_t size, 
                                size_t *bytes_read, 
                                uint32_t timeout);
    
    /**
     * @brief Write data to device
     * @param device Pointer to device structure
     * @param buffer Buffer containing data to write
     * @param size Number of bytes to write
     * @param bytes_written Pointer to store actual bytes written
     * @param timeout Timeout in milliseconds
     * @return I/O error code
     */
    pico_rtos_io_error_t (*write)(pico_rtos_io_device_t *device, 
                                 const void *buffer, 
                                 size_t size, 
                                 size_t *bytes_written, 
                                 uint32_t timeout);
    
    /**
     * @brief Control device (device-specific operations)
     * @param device Pointer to device structure
     * @param command Control command
     * @param arg Command argument
     * @return I/O error code
     */
    pico_rtos_io_error_t (*control)(pico_rtos_io_device_t *device, 
                                   uint32_t command, 
                                   void *arg);
    
    /**
     * @brief Get device status
     * @param device Pointer to device structure
     * @param status Pointer to store device status
     * @return I/O error code
     */
    pico_rtos_io_error_t (*status)(pico_rtos_io_device_t *device, 
                                  uint32_t *status);
} pico_rtos_io_device_ops_t;

// =============================================================================
// DEVICE STRUCTURE
// =============================================================================

/**
 * @brief I/O device structure
 */
struct pico_rtos_io_device {
    const char *name;                           ///< Device name
    pico_rtos_io_device_type_t type;           ///< Device type
    uint32_t device_id;                        ///< Unique device identifier
    void *device_handle;                       ///< Device-specific handle
    const pico_rtos_io_device_ops_t *ops;      ///< Device operations
    pico_rtos_mutex_t access_mutex;            ///< Serialization mutex
    bool initialized;                          ///< Initialization flag
    uint32_t reference_count;                  ///< Number of open handles
    uint32_t flags;                            ///< Device flags
    void *private_data;                        ///< Device-specific private data
    
    // Statistics
    uint32_t read_operations;                  ///< Number of read operations
    uint32_t write_operations;                 ///< Number of write operations
    uint32_t control_operations;               ///< Number of control operations
    uint32_t error_count;                      ///< Number of errors
    uint32_t bytes_read;                       ///< Total bytes read
    uint32_t bytes_written;                    ///< Total bytes written
    uint32_t last_error;                       ///< Last error code
    uint32_t last_operation_time;              ///< Last operation timestamp
};

// =============================================================================
// HANDLE STRUCTURE
// =============================================================================

/**
 * @brief I/O handle structure
 */
struct pico_rtos_io_handle {
    pico_rtos_io_device_t *device;             ///< Associated device
    uint32_t handle_id;                        ///< Unique handle identifier
    pico_rtos_io_mode_t mode;                  ///< Operation mode
    uint32_t timeout;                          ///< Default timeout
    uint32_t flags;                            ///< Handle flags
    void *user_data;                           ///< User-defined data
    bool valid;                                ///< Handle validity flag
    
    // Handle-specific statistics
    uint32_t operations_count;                 ///< Number of operations
    uint32_t bytes_transferred;                ///< Total bytes transferred
    uint32_t errors;                           ///< Number of errors
    uint32_t created_time;                     ///< Handle creation time
    uint32_t last_access_time;                 ///< Last access time
};

// =============================================================================
// DEVICE MANAGEMENT API
// =============================================================================

/**
 * @brief Initialize the I/O subsystem
 * 
 * Must be called before any other I/O functions.
 * 
 * @return true if initialization successful, false otherwise
 */
bool pico_rtos_io_init(void);

/**
 * @brief Register an I/O device
 * 
 * @param device Pointer to device structure
 * @param name Device name (must be unique)
 * @param type Device type
 * @param ops Pointer to device operations structure
 * @param device_handle Device-specific handle
 * @return true if registration successful, false otherwise
 */
bool pico_rtos_io_register_device(pico_rtos_io_device_t *device,
                                 const char *name,
                                 pico_rtos_io_device_type_t type,
                                 const pico_rtos_io_device_ops_t *ops,
                                 void *device_handle);

/**
 * @brief Unregister an I/O device
 * 
 * @param device Pointer to device structure
 * @return true if unregistration successful, false otherwise
 */
bool pico_rtos_io_unregister_device(pico_rtos_io_device_t *device);

/**
 * @brief Find a device by name
 * 
 * @param name Device name to search for
 * @return Pointer to device structure, or NULL if not found
 */
pico_rtos_io_device_t *pico_rtos_io_find_device(const char *name);

/**
 * @brief Find a device by type and index
 * 
 * @param type Device type to search for
 * @param index Index of device of this type (0-based)
 * @return Pointer to device structure, or NULL if not found
 */
pico_rtos_io_device_t *pico_rtos_io_find_device_by_type(pico_rtos_io_device_type_t type, 
                                                        uint32_t index);

/**
 * @brief Get list of all registered devices
 * 
 * @param devices Array to store device pointers
 * @param max_devices Maximum number of devices to return
 * @param actual_count Pointer to store actual number of devices returned
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_get_device_list(pico_rtos_io_device_t **devices,
                                 uint32_t max_devices,
                                 uint32_t *actual_count);

// =============================================================================
// HANDLE MANAGEMENT API
// =============================================================================

/**
 * @brief Open a device handle
 * 
 * @param device_name Name of device to open
 * @param mode Operation mode
 * @param timeout Default timeout for operations
 * @return Pointer to handle structure, or NULL if failed
 */
pico_rtos_io_handle_t *pico_rtos_io_open(const char *device_name,
                                        pico_rtos_io_mode_t mode,
                                        uint32_t timeout);

/**
 * @brief Open a device handle by type
 * 
 * @param type Device type to open
 * @param index Index of device of this type
 * @param mode Operation mode
 * @param timeout Default timeout for operations
 * @return Pointer to handle structure, or NULL if failed
 */
pico_rtos_io_handle_t *pico_rtos_io_open_by_type(pico_rtos_io_device_type_t type,
                                                 uint32_t index,
                                                 pico_rtos_io_mode_t mode,
                                                 uint32_t timeout);

/**
 * @brief Close a device handle
 * 
 * @param handle Pointer to handle structure
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_close(pico_rtos_io_handle_t *handle);

/**
 * @brief Check if a handle is valid
 * 
 * @param handle Pointer to handle structure
 * @return true if handle is valid, false otherwise
 */
bool pico_rtos_io_is_handle_valid(pico_rtos_io_handle_t *handle);

// =============================================================================
// I/O OPERATIONS API
// =============================================================================

/**
 * @brief Read data from device
 * 
 * @param handle Pointer to handle structure
 * @param buffer Buffer to store read data
 * @param size Number of bytes to read
 * @param bytes_read Pointer to store actual bytes read (optional)
 * @return I/O error code
 */
pico_rtos_io_error_t pico_rtos_io_read(pico_rtos_io_handle_t *handle,
                                      void *buffer,
                                      size_t size,
                                      size_t *bytes_read);

/**
 * @brief Read data with custom timeout
 * 
 * @param handle Pointer to handle structure
 * @param buffer Buffer to store read data
 * @param size Number of bytes to read
 * @param bytes_read Pointer to store actual bytes read (optional)
 * @param timeout Timeout in milliseconds
 * @return I/O error code
 */
pico_rtos_io_error_t pico_rtos_io_read_timeout(pico_rtos_io_handle_t *handle,
                                              void *buffer,
                                              size_t size,
                                              size_t *bytes_read,
                                              uint32_t timeout);

/**
 * @brief Write data to device
 * 
 * @param handle Pointer to handle structure
 * @param buffer Buffer containing data to write
 * @param size Number of bytes to write
 * @param bytes_written Pointer to store actual bytes written (optional)
 * @return I/O error code
 */
pico_rtos_io_error_t pico_rtos_io_write(pico_rtos_io_handle_t *handle,
                                       const void *buffer,
                                       size_t size,
                                       size_t *bytes_written);

/**
 * @brief Write data with custom timeout
 * 
 * @param handle Pointer to handle structure
 * @param buffer Buffer containing data to write
 * @param size Number of bytes to write
 * @param bytes_written Pointer to store actual bytes written (optional)
 * @param timeout Timeout in milliseconds
 * @return I/O error code
 */
pico_rtos_io_error_t pico_rtos_io_write_timeout(pico_rtos_io_handle_t *handle,
                                               const void *buffer,
                                               size_t size,
                                               size_t *bytes_written,
                                               uint32_t timeout);

/**
 * @brief Control device (device-specific operations)
 * 
 * @param handle Pointer to handle structure
 * @param command Control command
 * @param arg Command argument
 * @return I/O error code
 */
pico_rtos_io_error_t pico_rtos_io_control(pico_rtos_io_handle_t *handle,
                                         uint32_t command,
                                         void *arg);

/**
 * @brief Get device status
 * 
 * @param handle Pointer to handle structure
 * @param status Pointer to store device status
 * @return I/O error code
 */
pico_rtos_io_error_t pico_rtos_io_get_status(pico_rtos_io_handle_t *handle,
                                            uint32_t *status);

/**
 * @brief Flush device buffers
 * 
 * @param handle Pointer to handle structure
 * @return I/O error code
 */
pico_rtos_io_error_t pico_rtos_io_flush(pico_rtos_io_handle_t *handle);

// =============================================================================
// CONFIGURATION API
// =============================================================================

/**
 * @brief Set handle operation mode
 * 
 * @param handle Pointer to handle structure
 * @param mode New operation mode
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_set_mode(pico_rtos_io_handle_t *handle,
                          pico_rtos_io_mode_t mode);

/**
 * @brief Set handle default timeout
 * 
 * @param handle Pointer to handle structure
 * @param timeout New default timeout in milliseconds
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_set_timeout(pico_rtos_io_handle_t *handle,
                             uint32_t timeout);

/**
 * @brief Set handle flags
 * 
 * @param handle Pointer to handle structure
 * @param flags New flags value
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_set_flags(pico_rtos_io_handle_t *handle,
                           uint32_t flags);

/**
 * @brief Get handle flags
 * 
 * @param handle Pointer to handle structure
 * @return Current flags value
 */
uint32_t pico_rtos_io_get_flags(pico_rtos_io_handle_t *handle);

// =============================================================================
// STATISTICS AND MONITORING API
// =============================================================================

/**
 * @brief Device statistics structure
 */
typedef struct {
    uint32_t read_operations;
    uint32_t write_operations;
    uint32_t control_operations;
    uint32_t error_count;
    uint32_t bytes_read;
    uint32_t bytes_written;
    uint32_t reference_count;
    uint32_t last_error;
    uint32_t last_operation_time;
    uint32_t uptime;
} pico_rtos_io_device_stats_t;

/**
 * @brief Handle statistics structure
 */
typedef struct {
    uint32_t operations_count;
    uint32_t bytes_transferred;
    uint32_t errors;
    uint32_t created_time;
    uint32_t last_access_time;
    uint32_t lifetime;
} pico_rtos_io_handle_stats_t;

/**
 * @brief Get device statistics
 * 
 * @param device Pointer to device structure
 * @param stats Pointer to store statistics
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_get_device_stats(pico_rtos_io_device_t *device,
                                  pico_rtos_io_device_stats_t *stats);

/**
 * @brief Get handle statistics
 * 
 * @param handle Pointer to handle structure
 * @param stats Pointer to store statistics
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_get_handle_stats(pico_rtos_io_handle_t *handle,
                                  pico_rtos_io_handle_stats_t *stats);

/**
 * @brief Reset device statistics
 * 
 * @param device Pointer to device structure
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_reset_device_stats(pico_rtos_io_device_t *device);

/**
 * @brief Reset handle statistics
 * 
 * @param handle Pointer to handle structure
 * @return true if successful, false otherwise
 */
bool pico_rtos_io_reset_handle_stats(pico_rtos_io_handle_t *handle);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get error description string
 * 
 * @param error I/O error code
 * @return Pointer to error description string
 */
const char *pico_rtos_io_get_error_string(pico_rtos_io_error_t error);

/**
 * @brief Get device type string
 * 
 * @param type Device type
 * @return Pointer to device type string
 */
const char *pico_rtos_io_get_device_type_string(pico_rtos_io_device_type_t type);

/**
 * @brief Get operation mode string
 * 
 * @param mode Operation mode
 * @return Pointer to mode string
 */
const char *pico_rtos_io_get_mode_string(pico_rtos_io_mode_t mode);

#endif // PICO_RTOS_IO_H