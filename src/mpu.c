/**
 * @file mpu.c
 * @brief Memory Protection Unit Implementation for Pico-RTOS v0.3.1
 * 
 * This module implements MPU support for ARM Cortex-M processors.
 * Note: The RP2040 (Cortex-M0+) has limited MPU support compared to
 * higher-end Cortex-M variants.
 */

#include "pico_rtos/mpu.h"
#include "pico_rtos/logging.h"
#include "pico_rtos/task.h"
#include "pico/time.h"
#include <string.h>

// Only compile MPU support if enabled
#if PICO_RTOS_ENABLE_MPU_SUPPORT

// =============================================================================
// HARDWARE ABSTRACTION
// =============================================================================

// ARM Cortex-M MPU register definitions
#define MPU_TYPE_REG        (*((volatile uint32_t*)0xE000ED90))
#define MPU_CTRL_REG        (*((volatile uint32_t*)0xE000ED94))
#define MPU_RNR_REG         (*((volatile uint32_t*)0xE000ED98))
#define MPU_RBAR_REG        (*((volatile uint32_t*)0xE000ED9C))
#define MPU_RASR_REG        (*((volatile uint32_t*)0xE000EDA0))

// MPU Control Register bits
#define MPU_CTRL_ENABLE     (1 << 0)
#define MPU_CTRL_HFNMIENA   (1 << 1)
#define MPU_CTRL_PRIVDEFENA (1 << 2)

// MPU Region Attribute and Size Register bits
#define MPU_RASR_ENABLE     (1 << 0)
#define MPU_RASR_SIZE_SHIFT 1
#define MPU_RASR_SRD_SHIFT  8
#define MPU_RASR_B_SHIFT    16
#define MPU_RASR_C_SHIFT    17
#define MPU_RASR_S_SHIFT    18
#define MPU_RASR_TEX_SHIFT  19
#define MPU_RASR_AP_SHIFT   24
#define MPU_RASR_XN_SHIFT   28

// Fault status register (for fault handling)
#define SCB_CFSR_REG        (*((volatile uint32_t*)0xE000ED28))
#define SCB_MMFAR_REG       (*((volatile uint32_t*)0xE000ED34))

// Memory Management Fault Status Register bits
#define SCB_CFSR_IACCVIOL   (1 << 0)
#define SCB_CFSR_DACCVIOL   (1 << 1)
#define SCB_CFSR_MUNSTKERR  (1 << 3)
#define SCB_CFSR_MSTKERR    (1 << 4)
#define SCB_CFSR_MMARVALID  (1 << 7)

// =============================================================================
// INTERNAL DATA STRUCTURES
// =============================================================================

/**
 * @brief Internal MPU state structure
 */
typedef struct {
    pico_rtos_mpu_config_t config;         ///< Current MPU configuration
    pico_rtos_mpu_stats_t stats;           ///< MPU statistics
    pico_rtos_mpu_fault_handler_t fault_handler; ///< Fault handler callback
    bool initialized;                      ///< Initialization flag
    uint32_t hardware_regions;             ///< Number of hardware regions available
} pico_rtos_mpu_state_t;

// Global MPU state
static pico_rtos_mpu_state_t mpu_state = {0};

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Check if MPU hardware is available
 * 
 * @return true if MPU is available, false otherwise
 */
static bool check_mpu_availability(void) {
    uint32_t mpu_type = MPU_TYPE_REG;
    return (mpu_type & 0xFF) > 0;  // DREGION field indicates number of regions
}

/**
 * @brief Get number of MPU regions from hardware
 * 
 * @return Number of MPU regions supported by hardware
 */
static uint32_t get_hardware_region_count(void) {
    uint32_t mpu_type = MPU_TYPE_REG;
    return (mpu_type >> 8) & 0xFF;  // DREGION field
}

/**
 * @brief Validate region number
 * 
 * @param region_number Region number to validate
 * @return true if valid, false otherwise
 */
static bool validate_region_number(uint8_t region_number) {
    return region_number < mpu_state.hardware_regions && 
           region_number < PICO_RTOS_MPU_REGIONS_MAX;
}

/**
 * @brief Convert size in bytes to MPU size encoding
 * 
 * @param size_bytes Size in bytes
 * @return Size encoding, or 0 if invalid
 */
static uint32_t size_to_encoding(uint32_t size_bytes) {
    if (size_bytes < 32) {
        return 0;  // Invalid size
    }
    
    // Find the power of 2
    uint32_t encoding = 0;
    uint32_t test_size = 32;
    
    while (test_size < size_bytes && encoding < 31) {
        test_size <<= 1;
        encoding++;
    }
    
    if (test_size != size_bytes) {
        return 0;  // Size is not a power of 2
    }
    
    return encoding + 4;  // MPU encoding starts at 4 for 32 bytes
}

/**
 * @brief Convert MPU size encoding to bytes
 * 
 * @param encoding Size encoding
 * @return Size in bytes, or 0 if invalid
 */
static uint32_t encoding_to_size(uint32_t encoding) {
    if (encoding < 4 || encoding > 31) {
        return 0;  // Invalid encoding
    }
    
    return 1U << (encoding + 1);
}

/**
 * @brief Check if address is aligned for given size
 * 
 * @param address Address to check
 * @param size_bytes Size in bytes
 * @return true if aligned, false otherwise
 */
static bool is_address_aligned(uint32_t address, uint32_t size_bytes) {
    return (address & (size_bytes - 1)) == 0;
}

/**
 * @brief Configure hardware MPU region
 * 
 * @param region_number Region number
 * @param region Region configuration
 * @return true if successful, false otherwise
 */
static bool configure_hardware_region(uint8_t region_number, const pico_rtos_mpu_region_t *region) {
    if (!validate_region_number(region_number)) {
        return false;
    }
    
    // Select region
    MPU_RNR_REG = region_number;
    
    if (!region->enabled) {
        // Disable region
        MPU_RASR_REG = 0;
        return true;
    }
    
    // Set base address
    MPU_RBAR_REG = region->base_address & 0xFFFFFFE0;  // Clear lower 5 bits
    
    // Build region attribute and size register value
    uint32_t rasr = 0;
    
    if (region->enabled) {
        rasr |= MPU_RASR_ENABLE;
    }
    
    rasr |= (region->size_encoding & 0x1F) << MPU_RASR_SIZE_SHIFT;
    rasr |= (region->subregion_disable & 0xFF) << MPU_RASR_SRD_SHIFT;
    rasr |= (region->access_permissions & 0x7) << MPU_RASR_AP_SHIFT;
    
    if (region->bufferable) {
        rasr |= (1 << MPU_RASR_B_SHIFT);
    }
    
    if (region->cacheable) {
        rasr |= (1 << MPU_RASR_C_SHIFT);
    }
    
    if (region->shareable) {
        rasr |= (1 << MPU_RASR_S_SHIFT);
    }
    
    if (region->execute_never) {
        rasr |= (1 << MPU_RASR_XN_SHIFT);
    }
    
    // Set memory attributes (TEX field)
    rasr |= (region->memory_attributes & 0x7) << MPU_RASR_TEX_SHIFT;
    
    // Configure region
    MPU_RASR_REG = rasr;
    
    return true;
}

/**
 * @brief Handle MPU fault
 * 
 * This function is called from the MemManage_Handler interrupt
 */
static void handle_mpu_fault(void) {
    pico_rtos_mpu_fault_info_t fault_info = {0};
    
    // Read fault status
    uint32_t cfsr = SCB_CFSR_REG;
    fault_info.fault_status = cfsr;
    
    // Check if fault address is valid
    if (cfsr & SCB_CFSR_MMARVALID) {
        fault_info.fault_address = SCB_MMFAR_REG;
    }
    
    // Determine fault type
    fault_info.instruction_access = (cfsr & SCB_CFSR_IACCVIOL) != 0;
    fault_info.write_access = false;  // Cannot determine from CFSR on Cortex-M0+
    
    // Get current task information
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    fault_info.task_id = current_task ? (uint32_t)current_task : 0;
    fault_info.timestamp = time_us_32();
    
    // Update statistics
    mpu_state.stats.total_violations++;
    if (fault_info.instruction_access) {
        mpu_state.stats.instruction_violations++;
    } else {
        mpu_state.stats.data_violations++;
    }
    
    // Store fault information
    mpu_state.stats.last_fault = fault_info;
    
    // Call user fault handler if registered
    if (mpu_state.fault_handler) {
        mpu_state.fault_handler(&fault_info);
    }
    
    // Clear fault status
    SCB_CFSR_REG = cfsr & 0xFF;  // Clear MemManage fault bits
    
    PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, 
                       "MPU violation: addr=0x%08X, status=0x%08X, task=%u",
                       fault_info.fault_address, fault_info.fault_status, fault_info.task_id);
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_mpu_init(void) {
    if (mpu_state.initialized) {
        return true;
    }
    
    // Check if MPU is available
    if (!check_mpu_availability()) {
        PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "MPU not available on this hardware");
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_NOT_AVAILABLE, 0);
        return false;
    }
    
    // Initialize state
    memset(&mpu_state, 0, sizeof(mpu_state));
    mpu_state.hardware_regions = get_hardware_region_count();
    
    // Disable MPU initially
    MPU_CTRL_REG = 0;
    
    // Initialize configuration
    mpu_state.config.mpu_enabled = false;
    mpu_state.config.background_region_enabled = false;
    mpu_state.config.hfnmi_privdef_enabled = false;
    mpu_state.config.active_regions = 0;
    
    // Initialize statistics
    memset(&mpu_state.stats, 0, sizeof(mpu_state.stats));
    mpu_state.stats.regions_configured = 0;
    mpu_state.stats.mpu_active = false;
    
    mpu_state.initialized = true;
    
    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, 
                      "MPU initialized: %u regions available", mpu_state.hardware_regions);
    
    return true;
}

bool pico_rtos_mpu_enable(bool enable) {
    if (!mpu_state.initialized) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_NOT_AVAILABLE, 0);
        return false;
    }
    
    uint32_t ctrl_value = 0;
    
    if (enable) {
        ctrl_value |= MPU_CTRL_ENABLE;
        
        if (mpu_state.config.hfnmi_privdef_enabled) {
            ctrl_value |= MPU_CTRL_HFNMIENA;
        }
        
        if (mpu_state.config.background_region_enabled) {
            ctrl_value |= MPU_CTRL_PRIVDEFENA;
        }
    }
    
    MPU_CTRL_REG = ctrl_value;
    
    mpu_state.config.mpu_enabled = enable;
    mpu_state.stats.mpu_active = enable;
    
    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, 
                      "MPU %s", enable ? "enabled" : "disabled");
    
    return true;
}

bool pico_rtos_mpu_is_available(void) {
    return check_mpu_availability();
}

uint32_t pico_rtos_mpu_get_region_count(void) {
    if (!mpu_state.initialized) {
        return 0;
    }
    return mpu_state.hardware_regions;
}

bool pico_rtos_mpu_configure_region(uint8_t region_number, const pico_rtos_mpu_region_t *region) {
    if (!mpu_state.initialized || !region) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_TASK_INVALID_PARAMETER, region_number);
        return false;
    }
    
    if (!validate_region_number(region_number)) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_INVALID_REGION, region_number);
        return false;
    }
    
    // Validate region configuration
    if (!pico_rtos_mpu_validate_region_config(region)) {
        return false;
    }
    
    // Configure hardware
    if (!configure_hardware_region(region_number, region)) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_HARDWARE_FAULT, region_number);
        return false;
    }
    
    // Update internal state
    mpu_state.config.regions[region_number] = *region;
    
    if (region->enabled) {
        mpu_state.config.active_regions++;
        mpu_state.stats.regions_configured++;
    }
    
    PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, 
                       "MPU region %u configured: base=0x%08X, size=%u, enabled=%d",
                       region_number, region->base_address, 
                       encoding_to_size(region->size_encoding), region->enabled);
    
    return true;
}

bool pico_rtos_mpu_disable_region(uint8_t region_number) {
    if (!mpu_state.initialized) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_NOT_AVAILABLE, 0);
        return false;
    }
    
    if (!validate_region_number(region_number)) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_INVALID_REGION, region_number);
        return false;
    }
    
    // Select region and disable
    MPU_RNR_REG = region_number;
    MPU_RASR_REG = 0;
    
    // Update internal state
    if (mpu_state.config.regions[region_number].enabled) {
        mpu_state.config.active_regions--;
    }
    
    mpu_state.config.regions[region_number].enabled = false;
    
    PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, 
                       "MPU region %u disabled", region_number);
    
    return true;
}

bool pico_rtos_mpu_get_region_config(uint8_t region_number, pico_rtos_mpu_region_t *region) {
    if (!mpu_state.initialized || !region) {
        return false;
    }
    
    if (!validate_region_number(region_number)) {
        return false;
    }
    
    *region = mpu_state.config.regions[region_number];
    return true;
}

bool pico_rtos_mpu_validate_region_config(const pico_rtos_mpu_region_t *region) {
    if (!region) {
        return false;
    }
    
    // Check size encoding
    uint32_t size_bytes = encoding_to_size(region->size_encoding);
    if (size_bytes == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_INVALID_SIZE, region->size_encoding);
        return false;
    }
    
    // Check address alignment
    if (!is_address_aligned(region->base_address, size_bytes)) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_INVALID_ALIGNMENT, region->base_address);
        return false;
    }
    
    // Check access permissions
    if (region->access_permissions > 7) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MPU_INVALID_PERMISSIONS, region->access_permissions);
        return false;
    }
    
    return true;
}

// =============================================================================
// CONVENIENCE FUNCTIONS IMPLEMENTATION
// =============================================================================

bool pico_rtos_mpu_configure_simple_region(uint8_t region_number,
                                          uint32_t base_address,
                                          uint32_t size_bytes,
                                          bool read_access,
                                          bool write_access,
                                          bool execute_access) {
    pico_rtos_mpu_region_t region = {0};
    
    region.base_address = base_address;
    region.size_encoding = size_to_encoding(size_bytes);
    region.enabled = true;
    region.execute_never = !execute_access;
    
    // Set access permissions based on read/write flags
    if (read_access && write_access) {
        region.access_permissions = PICO_RTOS_MPU_ACCESS_FULL_RW;
    } else if (read_access) {
        region.access_permissions = PICO_RTOS_MPU_ACCESS_FULL_R;
    } else {
        region.access_permissions = PICO_RTOS_MPU_ACCESS_NONE;
    }
    
    // Set reasonable defaults for memory attributes
    region.memory_attributes = PICO_RTOS_MPU_ATTR_NORMAL_WB_WA;
    region.cacheable = true;
    region.bufferable = true;
    region.shareable = false;
    
    return pico_rtos_mpu_configure_region(region_number, &region);
}

bool pico_rtos_mpu_configure_stack_protection(uint8_t region_number,
                                             uint32_t stack_base,
                                             uint32_t stack_size,
                                             uint32_t guard_size) {
    // Configure guard region at bottom of stack with no access
    uint32_t guard_base = stack_base - guard_size;
    
    return pico_rtos_mpu_configure_simple_region(region_number, guard_base, guard_size,
                                               false, false, false);
}

bool pico_rtos_mpu_configure_code_protection(uint8_t region_number,
                                            uint32_t code_base,
                                            uint32_t code_size) {
    // Configure code region as read-only and executable
    return pico_rtos_mpu_configure_simple_region(region_number, code_base, code_size,
                                               true, false, true);
}

// =============================================================================
// FAULT HANDLING IMPLEMENTATION
// =============================================================================

void pico_rtos_mpu_set_fault_handler(pico_rtos_mpu_fault_handler_t handler) {
    mpu_state.fault_handler = handler;
}

pico_rtos_mpu_fault_handler_t pico_rtos_mpu_get_fault_handler(void) {
    return mpu_state.fault_handler;
}

bool pico_rtos_mpu_get_last_fault_info(pico_rtos_mpu_fault_info_t *fault_info) {
    if (!fault_info || mpu_state.stats.total_violations == 0) {
        return false;
    }
    
    *fault_info = mpu_state.stats.last_fault;
    return true;
}

void pico_rtos_mpu_clear_fault_info(void) {
    memset(&mpu_state.stats.last_fault, 0, sizeof(mpu_state.stats.last_fault));
    mpu_state.stats.total_violations = 0;
    mpu_state.stats.instruction_violations = 0;
    mpu_state.stats.data_violations = 0;
    mpu_state.stats.write_violations = 0;
    mpu_state.stats.read_violations = 0;
}

// =============================================================================
// STATISTICS IMPLEMENTATION
// =============================================================================

bool pico_rtos_mpu_get_stats(pico_rtos_mpu_stats_t *stats) {
    if (!stats || !mpu_state.initialized) {
        return false;
    }
    
    *stats = mpu_state.stats;
    return true;
}

void pico_rtos_mpu_reset_stats(void) {
    memset(&mpu_state.stats, 0, sizeof(mpu_state.stats));
    mpu_state.stats.regions_configured = mpu_state.config.active_regions;
    mpu_state.stats.mpu_active = mpu_state.config.mpu_enabled;
}

// =============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// =============================================================================

uint32_t pico_rtos_mpu_calculate_size_encoding(uint32_t size_bytes) {
    return size_to_encoding(size_bytes);
}

uint32_t pico_rtos_mpu_calculate_size_bytes(uint32_t size_encoding) {
    return encoding_to_size(size_encoding);
}

bool pico_rtos_mpu_is_address_aligned(uint32_t address, uint32_t size_bytes) {
    return is_address_aligned(address, size_bytes);
}

uint32_t pico_rtos_mpu_get_required_alignment(uint32_t size_bytes) {
    // For MPU, alignment requirement equals the region size
    return size_bytes;
}

// =============================================================================
// INTERRUPT HANDLER
// =============================================================================

/**
 * @brief MemManage fault handler
 * 
 * This is the actual interrupt handler that gets called on MPU violations.
 * It should be linked to the MemManage_Handler vector.
 */
void MemManage_Handler(void) {
    handle_mpu_fault();
}

#endif // PICO_RTOS_ENABLE_MPU_SUPPORT