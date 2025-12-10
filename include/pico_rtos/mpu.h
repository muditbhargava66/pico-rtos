#ifndef PICO_RTOS_MPU_H
#define PICO_RTOS_MPU_H

#include "config.h"
#include "types.h"
#include "error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file mpu.h
 * @brief Memory Protection Unit (MPU) Support for Pico-RTOS v0.3.1
 * 
 * This module provides optional Memory Protection Unit support for
 * hardware-assisted memory protection on supported ARM Cortex-M variants.
 * The MPU provides hardware-level memory protection by defining regions
 * with specific access permissions.
 * 
 * Features:
 * - MPU region configuration for supported ARM variants
 * - Memory protection violation handlers with detailed diagnostics
 * - Conditional compilation support for MPU-enabled/disabled builds
 * - Integration with existing memory management system
 * 
 * Note: MPU availability depends on the specific ARM Cortex-M implementation.
 * The RP2040 (Cortex-M0+) has limited MPU support compared to higher-end
 * Cortex-M variants.
 */

// Only compile MPU support if enabled and on supported platforms
#if PICO_RTOS_ENABLE_MPU_SUPPORT

// =============================================================================
// CONFIGURATION AND CONSTANTS
// =============================================================================

#ifndef PICO_RTOS_MPU_REGIONS_MAX
#define PICO_RTOS_MPU_REGIONS_MAX 8  ///< Maximum number of MPU regions (hardware dependent)
#endif

// MPU region sizes (must be power of 2, minimum 32 bytes)
#define PICO_RTOS_MPU_SIZE_32B      0x04    ///< 32 bytes
#define PICO_RTOS_MPU_SIZE_64B      0x05    ///< 64 bytes
#define PICO_RTOS_MPU_SIZE_128B     0x06    ///< 128 bytes
#define PICO_RTOS_MPU_SIZE_256B     0x07    ///< 256 bytes
#define PICO_RTOS_MPU_SIZE_512B     0x08    ///< 512 bytes
#define PICO_RTOS_MPU_SIZE_1KB      0x09    ///< 1 KB
#define PICO_RTOS_MPU_SIZE_2KB      0x0A    ///< 2 KB
#define PICO_RTOS_MPU_SIZE_4KB      0x0B    ///< 4 KB
#define PICO_RTOS_MPU_SIZE_8KB      0x0C    ///< 8 KB
#define PICO_RTOS_MPU_SIZE_16KB     0x0D    ///< 16 KB
#define PICO_RTOS_MPU_SIZE_32KB     0x0E    ///< 32 KB
#define PICO_RTOS_MPU_SIZE_64KB     0x0F    ///< 64 KB
#define PICO_RTOS_MPU_SIZE_128KB    0x10    ///< 128 KB
#define PICO_RTOS_MPU_SIZE_256KB    0x11    ///< 256 KB
#define PICO_RTOS_MPU_SIZE_512KB    0x12    ///< 512 KB
#define PICO_RTOS_MPU_SIZE_1MB      0x13    ///< 1 MB
#define PICO_RTOS_MPU_SIZE_2MB      0x14    ///< 2 MB
#define PICO_RTOS_MPU_SIZE_4MB      0x15    ///< 4 MB
#define PICO_RTOS_MPU_SIZE_8MB      0x16    ///< 8 MB
#define PICO_RTOS_MPU_SIZE_16MB     0x17    ///< 16 MB
#define PICO_RTOS_MPU_SIZE_32MB     0x18    ///< 32 MB
#define PICO_RTOS_MPU_SIZE_64MB     0x19    ///< 64 MB
#define PICO_RTOS_MPU_SIZE_128MB    0x1A    ///< 128 MB
#define PICO_RTOS_MPU_SIZE_256MB    0x1B    ///< 256 MB
#define PICO_RTOS_MPU_SIZE_512MB    0x1C    ///< 512 MB
#define PICO_RTOS_MPU_SIZE_1GB      0x1D    ///< 1 GB
#define PICO_RTOS_MPU_SIZE_2GB      0x1E    ///< 2 GB
#define PICO_RTOS_MPU_SIZE_4GB      0x1F    ///< 4 GB

// MPU access permissions
#define PICO_RTOS_MPU_ACCESS_NONE           0x00    ///< No access
#define PICO_RTOS_MPU_ACCESS_PRIV_RW        0x01    ///< Privileged read/write
#define PICO_RTOS_MPU_ACCESS_PRIV_RW_USER_R 0x02    ///< Privileged RW, user read
#define PICO_RTOS_MPU_ACCESS_FULL_RW        0x03    ///< Full read/write access
#define PICO_RTOS_MPU_ACCESS_PRIV_R         0x05    ///< Privileged read-only
#define PICO_RTOS_MPU_ACCESS_FULL_R         0x06    ///< Full read-only access

// MPU memory attributes
#define PICO_RTOS_MPU_ATTR_STRONGLY_ORDERED    0x00    ///< Strongly ordered
#define PICO_RTOS_MPU_ATTR_DEVICE              0x01    ///< Device memory
#define PICO_RTOS_MPU_ATTR_NORMAL_WT_NWA       0x02    ///< Normal, write-through, no write allocate
#define PICO_RTOS_MPU_ATTR_NORMAL_WB_NWA       0x03    ///< Normal, write-back, no write allocate
#define PICO_RTOS_MPU_ATTR_NORMAL_WT_WA        0x06    ///< Normal, write-through, write allocate
#define PICO_RTOS_MPU_ATTR_NORMAL_WB_WA        0x07    ///< Normal, write-back, write allocate

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief MPU region configuration structure
 * 
 * Defines the configuration for a single MPU region including
 * base address, size, access permissions, and memory attributes.
 */
typedef struct {
    uint32_t base_address;          ///< Region base address (must be aligned to region size)
    uint32_t size_encoding;         ///< Region size encoding (use PICO_RTOS_MPU_SIZE_* constants)
    uint32_t access_permissions;    ///< Access permissions (use PICO_RTOS_MPU_ACCESS_* constants)
    uint32_t memory_attributes;     ///< Memory attributes (use PICO_RTOS_MPU_ATTR_* constants)
    bool execute_never;             ///< Execute Never (XN) bit
    bool shareable;                 ///< Shareable bit
    bool cacheable;                 ///< Cacheable bit
    bool bufferable;                ///< Bufferable bit
    bool enabled;                   ///< Region enabled flag
    uint8_t subregion_disable;      ///< Subregion disable mask (8 bits)
} pico_rtos_mpu_region_t;

/**
 * @brief MPU configuration structure
 * 
 * Contains the complete MPU configuration including all regions
 * and global MPU settings.
 */
typedef struct {
    pico_rtos_mpu_region_t regions[PICO_RTOS_MPU_REGIONS_MAX];  ///< MPU regions
    uint32_t active_regions;        ///< Number of active regions
    bool mpu_enabled;               ///< Global MPU enable flag
    bool background_region_enabled; ///< Background region enable
    bool hfnmi_privdef_enabled;     ///< HardFault/NMI/FAULTMASK privilege default
} pico_rtos_mpu_config_t;

/**
 * @brief MPU fault information structure
 * 
 * Contains detailed information about MPU violations for
 * debugging and fault handling.
 */
typedef struct {
    uint32_t fault_address;         ///< Address that caused the fault
    uint32_t fault_status;          ///< MPU fault status register value
    bool instruction_access;        ///< true if instruction access, false if data access
    bool write_access;              ///< true if write access, false if read access
    uint8_t fault_region;           ///< Region number that caused the fault (if applicable)
    uint32_t task_id;               ///< Task ID that caused the fault
    uint32_t timestamp;             ///< Timestamp when fault occurred
} pico_rtos_mpu_fault_info_t;

/**
 * @brief MPU statistics structure
 * 
 * Provides statistics about MPU usage and violations for
 * monitoring and debugging purposes.
 */
typedef struct {
    uint32_t total_violations;      ///< Total number of MPU violations
    uint32_t instruction_violations; ///< Number of instruction access violations
    uint32_t data_violations;       ///< Number of data access violations
    uint32_t write_violations;      ///< Number of write access violations
    uint32_t read_violations;       ///< Number of read access violations
    uint32_t regions_configured;    ///< Number of regions currently configured
    bool mpu_active;                ///< MPU currently active
    pico_rtos_mpu_fault_info_t last_fault; ///< Information about last fault
} pico_rtos_mpu_stats_t;

// =============================================================================
// CALLBACK TYPES
// =============================================================================

/**
 * @brief MPU fault handler callback type
 * 
 * User-defined callback function that gets called when an MPU
 * violation occurs. This allows applications to implement custom
 * fault handling behavior.
 * 
 * @param fault_info Pointer to fault information structure
 */
typedef void (*pico_rtos_mpu_fault_handler_t)(const pico_rtos_mpu_fault_info_t *fault_info);

// =============================================================================
// CORE API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize the MPU system
 * 
 * Initializes the MPU hardware and internal data structures.
 * Must be called before any other MPU functions.
 * 
 * @return true if initialization successful, false otherwise
 * 
 * @note This function checks for MPU hardware availability
 */
bool pico_rtos_mpu_init(void);

/**
 * @brief Enable or disable the MPU
 * 
 * Enables or disables the MPU globally. When disabled, no memory
 * protection is enforced.
 * 
 * @param enable true to enable MPU, false to disable
 * @return true if operation successful, false otherwise
 */
bool pico_rtos_mpu_enable(bool enable);

/**
 * @brief Check if MPU is available on this hardware
 * 
 * @return true if MPU is available, false otherwise
 */
bool pico_rtos_mpu_is_available(void);

/**
 * @brief Get the number of MPU regions supported by hardware
 * 
 * @return Number of MPU regions supported, or 0 if MPU not available
 */
uint32_t pico_rtos_mpu_get_region_count(void);

// =============================================================================
// REGION CONFIGURATION API
// =============================================================================

/**
 * @brief Configure an MPU region
 * 
 * Configures a specific MPU region with the provided settings.
 * The region will be enabled if the configuration is valid.
 * 
 * @param region_number Region number (0 to PICO_RTOS_MPU_REGIONS_MAX-1)
 * @param region Pointer to region configuration
 * @return true if configuration successful, false otherwise
 * 
 * @note Base address must be aligned to region size
 * @note Region size must be a power of 2, minimum 32 bytes
 */
bool pico_rtos_mpu_configure_region(uint8_t region_number, const pico_rtos_mpu_region_t *region);

/**
 * @brief Disable an MPU region
 * 
 * Disables a specific MPU region, removing its memory protection.
 * 
 * @param region_number Region number to disable
 * @return true if operation successful, false otherwise
 */
bool pico_rtos_mpu_disable_region(uint8_t region_number);

/**
 * @brief Get configuration of an MPU region
 * 
 * Retrieves the current configuration of a specific MPU region.
 * 
 * @param region_number Region number to query
 * @param region Pointer to structure to fill with region configuration
 * @return true if operation successful, false otherwise
 */
bool pico_rtos_mpu_get_region_config(uint8_t region_number, pico_rtos_mpu_region_t *region);

/**
 * @brief Validate MPU region configuration
 * 
 * Validates that a region configuration is valid without actually
 * configuring the hardware. Useful for testing configurations.
 * 
 * @param region Pointer to region configuration to validate
 * @return true if configuration is valid, false otherwise
 */
bool pico_rtos_mpu_validate_region_config(const pico_rtos_mpu_region_t *region);

// =============================================================================
// CONVENIENCE FUNCTIONS
// =============================================================================

/**
 * @brief Configure a simple memory region with basic protection
 * 
 * Convenience function to configure a region with common settings.
 * 
 * @param region_number Region number to configure
 * @param base_address Base address of the region
 * @param size_bytes Size of the region in bytes (must be power of 2)
 * @param read_access Allow read access
 * @param write_access Allow write access
 * @param execute_access Allow execute access
 * @return true if configuration successful, false otherwise
 */
bool pico_rtos_mpu_configure_simple_region(uint8_t region_number,
                                          uint32_t base_address,
                                          uint32_t size_bytes,
                                          bool read_access,
                                          bool write_access,
                                          bool execute_access);

/**
 * @brief Configure stack protection for a task
 * 
 * Convenience function to set up stack overflow protection for a task.
 * Creates a guard region at the bottom of the stack.
 * 
 * @param region_number Region number to use for protection
 * @param stack_base Base address of the stack
 * @param stack_size Size of the stack in bytes
 * @param guard_size Size of the guard region in bytes
 * @return true if configuration successful, false otherwise
 */
bool pico_rtos_mpu_configure_stack_protection(uint8_t region_number,
                                             uint32_t stack_base,
                                             uint32_t stack_size,
                                             uint32_t guard_size);

/**
 * @brief Configure code region protection
 * 
 * Convenience function to protect code regions from modification.
 * 
 * @param region_number Region number to configure
 * @param code_base Base address of the code region
 * @param code_size Size of the code region in bytes
 * @return true if configuration successful, false otherwise
 */
bool pico_rtos_mpu_configure_code_protection(uint8_t region_number,
                                            uint32_t code_base,
                                            uint32_t code_size);

// =============================================================================
// FAULT HANDLING API
// =============================================================================

/**
 * @brief Set MPU fault handler callback
 * 
 * Registers a callback function to be called when MPU violations occur.
 * Only one callback can be registered at a time.
 * 
 * @param handler Pointer to callback function, or NULL to disable callbacks
 */
void pico_rtos_mpu_set_fault_handler(pico_rtos_mpu_fault_handler_t handler);

/**
 * @brief Get current MPU fault handler
 * 
 * @return Pointer to current callback function, or NULL if none set
 */
pico_rtos_mpu_fault_handler_t pico_rtos_mpu_get_fault_handler(void);

/**
 * @brief Get information about the last MPU fault
 * 
 * Retrieves detailed information about the most recent MPU violation.
 * 
 * @param fault_info Pointer to structure to fill with fault information
 * @return true if fault information available, false if no recent faults
 */
bool pico_rtos_mpu_get_last_fault_info(pico_rtos_mpu_fault_info_t *fault_info);

/**
 * @brief Clear MPU fault information
 * 
 * Clears the stored fault information and resets fault counters.
 */
void pico_rtos_mpu_clear_fault_info(void);

// =============================================================================
// STATISTICS AND MONITORING API
// =============================================================================

/**
 * @brief Get MPU statistics
 * 
 * Retrieves comprehensive statistics about MPU usage and violations.
 * 
 * @param stats Pointer to structure to fill with statistics
 * @return true if statistics retrieved successfully, false otherwise
 */
bool pico_rtos_mpu_get_stats(pico_rtos_mpu_stats_t *stats);

/**
 * @brief Reset MPU statistics
 * 
 * Resets all MPU statistics counters to zero.
 */
void pico_rtos_mpu_reset_stats(void);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Calculate size encoding for a given size in bytes
 * 
 * Converts a size in bytes to the appropriate MPU size encoding.
 * The size must be a power of 2 and at least 32 bytes.
 * 
 * @param size_bytes Size in bytes
 * @return Size encoding value, or 0 if invalid size
 */
uint32_t pico_rtos_mpu_calculate_size_encoding(uint32_t size_bytes);

/**
 * @brief Calculate size in bytes from size encoding
 * 
 * Converts an MPU size encoding back to size in bytes.
 * 
 * @param size_encoding Size encoding value
 * @return Size in bytes, or 0 if invalid encoding
 */
uint32_t pico_rtos_mpu_calculate_size_bytes(uint32_t size_encoding);

/**
 * @brief Check if an address is aligned for a given region size
 * 
 * Verifies that a base address is properly aligned for an MPU region
 * of the specified size.
 * 
 * @param address Address to check
 * @param size_bytes Region size in bytes
 * @return true if address is properly aligned, false otherwise
 */
bool pico_rtos_mpu_is_address_aligned(uint32_t address, uint32_t size_bytes);

/**
 * @brief Get the minimum alignment for a given region size
 * 
 * Returns the required alignment for an MPU region of the specified size.
 * 
 * @param size_bytes Region size in bytes
 * @return Required alignment in bytes, or 0 if invalid size
 */
uint32_t pico_rtos_mpu_get_required_alignment(uint32_t size_bytes);

// =============================================================================
// ERROR CODES
// =============================================================================

// MPU specific error codes (extending the existing error system)
#define PICO_RTOS_ERROR_MPU_NOT_AVAILABLE       (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 10)
#define PICO_RTOS_ERROR_MPU_INVALID_REGION       (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 11)
#define PICO_RTOS_ERROR_MPU_INVALID_SIZE         (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 12)
#define PICO_RTOS_ERROR_MPU_INVALID_ALIGNMENT    (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 13)
#define PICO_RTOS_ERROR_MPU_INVALID_PERMISSIONS  (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 14)
#define PICO_RTOS_ERROR_MPU_REGION_OVERLAP       (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 15)
#define PICO_RTOS_ERROR_MPU_HARDWARE_FAULT       (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 16)

#else // !PICO_RTOS_ENABLE_MPU_SUPPORT

// When MPU support is disabled, provide stub definitions
#define pico_rtos_mpu_init() (false)
#define pico_rtos_mpu_enable(enable) (false)
#define pico_rtos_mpu_is_available() (false)
#define pico_rtos_mpu_get_region_count() (0)

#endif // PICO_RTOS_ENABLE_MPU_SUPPORT

#endif // PICO_RTOS_MPU_H