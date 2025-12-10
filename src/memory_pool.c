/**
 * @file memory_pool.c
 * @brief Memory Pool Implementation for Pico-RTOS v0.3.1
 * 
 * This module implements O(1) fixed-size block allocation and deallocation
 * using memory pools with comprehensive statistics and validation.
 */

#include "pico_rtos/memory_pool.h"
#include "pico_rtos/blocking.h"
#include "pico_rtos/task.h"
#include "pico_rtos/logging.h"
#include "pico/time.h"
#include <string.h>

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Align a size to the required alignment boundary
 * 
 * @param size Size to align
 * @return Aligned size
 */
static inline uint32_t align_size(uint32_t size) {
    return (size + PICO_RTOS_MEMORY_POOL_ALIGNMENT - 1) & ~(PICO_RTOS_MEMORY_POOL_ALIGNMENT - 1);
}

/**
 * @brief Check if a pointer is properly aligned
 * 
 * @param ptr Pointer to check
 * @return true if aligned, false otherwise
 */
static inline bool is_aligned(void *ptr) {
    return ((uintptr_t)ptr & (PICO_RTOS_MEMORY_POOL_ALIGNMENT - 1)) == 0;
}

/**
 * @brief Validate pool structure integrity
 * 
 * @param pool Pointer to memory pool
 * @return true if valid, false otherwise
 */
static bool validate_pool_structure(pico_rtos_memory_pool_t *pool) {
    if (!pool || !pool->initialized) {
        return false;
    }
    
    if (pool->magic != PICO_RTOS_MEMORY_POOL_MAGIC) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_CORRUPTION, (uint32_t)pool);
        return false;
    }
    
    if (!pool->pool_start || pool->block_size == 0 || pool->total_blocks == 0) {
        return false;
    }
    
    if (!is_aligned(pool->pool_start)) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_ALIGNMENT, (uint32_t)pool->pool_start);
        return false;
    }
    
    return true;
}

/**
 * @brief Initialize free list for the memory pool
 * 
 * @param pool Pointer to memory pool
 */
static void initialize_free_list(pico_rtos_memory_pool_t *pool) {
    uint8_t *current_block = (uint8_t *)pool->pool_start;
    pico_rtos_memory_block_t *prev_block = NULL;
    
    // Link all blocks in the free list
    for (uint32_t i = 0; i < pool->total_blocks; i++) {
        pico_rtos_memory_block_t *block = (pico_rtos_memory_block_t *)current_block;
        
        // Set magic number for corruption detection
        block->magic = PICO_RTOS_MEMORY_POOL_FREE_MAGIC;
        
        if (i == 0) {
            pool->free_list = block;
        } else {
            prev_block->next = block;
        }
        
        if (i == pool->total_blocks - 1) {
            block->next = NULL;  // Last block
        }
        
        prev_block = block;
        current_block += pool->block_size;
    }
    
    pool->free_blocks = pool->total_blocks;
}

/**
 * @brief Validate that a block belongs to the pool
 * 
 * @param pool Pointer to memory pool
 * @param block Pointer to block to validate
 * @return true if block is valid, false otherwise
 */
static bool validate_block_ownership(pico_rtos_memory_pool_t *pool, void *block) {
    if (!block || !pool->pool_start) {
        return false;
    }
    
    uint8_t *pool_start = (uint8_t *)pool->pool_start;
    uint8_t *pool_end = pool_start + (pool->total_blocks * pool->block_size);
    uint8_t *block_ptr = (uint8_t *)block;
    
    // Check if block is within pool bounds
    if (block_ptr < pool_start || block_ptr >= pool_end) {
        return false;
    }
    
    // Check if block is properly aligned within the pool
    uint32_t offset = block_ptr - pool_start;
    if (offset % pool->block_size != 0) {
        return false;
    }
    
    return true;
}

/**
 * @brief Check if a block is currently in the free list
 * 
 * @param pool Pointer to memory pool
 * @param block Pointer to block to check
 * @return true if block is free, false if allocated or invalid
 */
static bool is_block_in_free_list(pico_rtos_memory_pool_t *pool, void *block) {
    pico_rtos_memory_block_t *current = pool->free_list;
    
    while (current) {
        if (current == block) {
            // Validate magic number
            if (current->magic == PICO_RTOS_MEMORY_POOL_FREE_MAGIC) {
                return true;
            } else {
                // Corruption detected
                PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_CORRUPTION, (uint32_t)current);
                return false;
            }
        }
        current = current->next;
    }
    
    return false;
}

#if PICO_RTOS_ENABLE_MEMORY_TRACKING
/**
 * @brief Update allocation statistics
 * 
 * @param pool Pointer to memory pool
 * @param alloc_cycles Allocation time in CPU cycles
 */
static void update_alloc_stats(pico_rtos_memory_pool_t *pool, uint32_t alloc_cycles) {
    pool->stats.allocation_count++;
    pool->stats.allocated_blocks = pool->total_blocks - pool->free_blocks;
    
    if (pool->stats.allocated_blocks > pool->stats.peak_allocated) {
        pool->stats.peak_allocated = pool->stats.allocated_blocks;
    }
    
    // Update average allocation time
    if (pool->stats.allocation_count == 1) {
        pool->stats.avg_alloc_time_cycles = alloc_cycles;
    } else {
        pool->stats.avg_alloc_time_cycles = 
            (pool->stats.avg_alloc_time_cycles * (pool->stats.allocation_count - 1) + alloc_cycles) / 
            pool->stats.allocation_count;
    }
    
    pool->last_alloc_cycles = alloc_cycles;
}

/**
 * @brief Update deallocation statistics
 * 
 * @param pool Pointer to memory pool
 * @param free_cycles Deallocation time in CPU cycles
 */
static void update_free_stats(pico_rtos_memory_pool_t *pool, uint32_t free_cycles) {
    pool->stats.deallocation_count++;
    pool->stats.allocated_blocks = pool->total_blocks - pool->free_blocks;
    
    // Update average deallocation time
    if (pool->stats.deallocation_count == 1) {
        pool->stats.avg_free_time_cycles = free_cycles;
    } else {
        pool->stats.avg_free_time_cycles = 
            (pool->stats.avg_free_time_cycles * (pool->stats.deallocation_count - 1) + free_cycles) / 
            pool->stats.deallocation_count;
    }
    
    pool->last_free_cycles = free_cycles;
}
#endif // PICO_RTOS_ENABLE_MEMORY_TRACKING

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_memory_pool_init(pico_rtos_memory_pool_t *pool, 
                               void *memory, 
                               uint32_t block_size, 
                               uint32_t block_count) {
    if (!pool || !memory || block_size == 0 || block_count == 0) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_INVALID_PARAM, 0);
        return false;
    }
    
    if (!is_aligned(memory)) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_ALIGNMENT, (uint32_t)memory);
        return false;
    }
    
    // Clear the pool structure
    memset(pool, 0, sizeof(pico_rtos_memory_pool_t));
    
    // Initialize basic pool parameters
    pool->pool_start = memory;
    pool->block_size = align_size(block_size);
    pool->total_blocks = block_count;
    pool->free_blocks = 0;  // Will be set by initialize_free_list
    pool->free_list = NULL;
    pool->magic = PICO_RTOS_MEMORY_POOL_MAGIC;
    
    // Initialize critical section
    critical_section_init(&pool->cs);
    
    // Initialize statistics
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
    memset(&pool->stats, 0, sizeof(pool->stats));
    pool->stats.total_blocks = block_count;
    pool->stats.block_size = pool->block_size;
    pool->stats.pool_size = pool->block_size * block_count;
    pool->stats.free_blocks = block_count;
#endif
    
    // Create blocking object for timeout support
    pool->block_obj = pico_rtos_block_object_create(pool);
    if (!pool->block_obj) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_OUT_OF_MEMORY, 0);
        return false;
    }
    
    // Initialize the free list
    initialize_free_list(pool);
    
    pool->initialized = true;
    
    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory pool initialized: %u blocks of %u bytes each", 
                       block_count, pool->block_size);
    
    return true;
}

void *pico_rtos_memory_pool_alloc(pico_rtos_memory_pool_t *pool, uint32_t timeout) {
    if (!validate_pool_structure(pool)) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_NOT_INITIALIZED, (uint32_t)pool);
        return NULL;
    }
    
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
    uint32_t start_cycles = time_us_32();
#endif
    
    void *allocated_block = NULL;
    
    // Handle immediate allocation or timeout logic
    do {
        critical_section_enter_blocking(&pool->cs);
        
        if (pool->free_list != NULL) {
            // Get block from free list head (O(1) operation)
            pico_rtos_memory_block_t *block = pool->free_list;
            
            // Validate magic number
            if (block->magic != PICO_RTOS_MEMORY_POOL_FREE_MAGIC) {
                critical_section_exit(&pool->cs);
                PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_CORRUPTION, (uint32_t)block);
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
                pool->stats.corruption_detected++;
#endif
                return NULL;
            }
            
            // Remove from free list
            pool->free_list = block->next;
            pool->free_blocks--;
            
            // Clear the block header (overwrite magic and next pointer)
            memset(block, 0, sizeof(pico_rtos_memory_block_t));
            
            allocated_block = block;
            
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
            uint32_t end_cycles = time_us_32();
            update_alloc_stats(pool, end_cycles - start_cycles);
#endif
            
            critical_section_exit(&pool->cs);
            
            PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory pool alloc: block %p, %u free remaining", 
                               allocated_block, pool->free_blocks);
            
            return allocated_block;
        }
        
        critical_section_exit(&pool->cs);
        
        // No blocks available
        if (timeout == PICO_RTOS_NO_WAIT) {
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
            pool->stats.allocation_failures++;
#endif
            PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory pool allocation failed: no blocks available");
            return NULL;
        }
        
        // Block the current task until a block becomes available or timeout
        pico_rtos_task_t *current_task = pico_rtos_get_current_task();
        if (!pico_rtos_block_task(pool->block_obj, current_task, 
                                 PICO_RTOS_BLOCK_REASON_QUEUE_EMPTY, timeout)) {
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
            pool->stats.allocation_failures++;
#endif
            PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory pool allocation timeout");
            return NULL;
        }
        
        // Task was unblocked, try allocation again
        // (timeout is handled by the blocking system)
        
    } while (allocated_block == NULL);
    
    return allocated_block;
}

bool pico_rtos_memory_pool_free(pico_rtos_memory_pool_t *pool, void *block) {
    if (!validate_pool_structure(pool) || !block) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_INVALID_PARAM, (uint32_t)block);
        return false;
    }
    
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
    uint32_t start_cycles = time_us_32();
#endif
    
    critical_section_enter_blocking(&pool->cs);
    
    // Validate block ownership
    if (!validate_block_ownership(pool, block)) {
        critical_section_exit(&pool->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_INVALID_BLOCK, (uint32_t)block);
        return false;
    }
    
    // Check for double-free
    if (is_block_in_free_list(pool, block)) {
        critical_section_exit(&pool->cs);
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_DOUBLE_FREE, (uint32_t)block);
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
        pool->stats.double_free_detected++;
#endif
        return false;
    }
    
    // Add block back to free list head (O(1) operation)
    pico_rtos_memory_block_t *free_block = (pico_rtos_memory_block_t *)block;
    free_block->magic = PICO_RTOS_MEMORY_POOL_FREE_MAGIC;
    free_block->next = pool->free_list;
    pool->free_list = free_block;
    pool->free_blocks++;
    
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
    uint32_t end_cycles = time_us_32();
    update_free_stats(pool, end_cycles - start_cycles);
#endif
    
    critical_section_exit(&pool->cs);
    
    // Unblock any waiting tasks
    pico_rtos_task_t *unblocked_task = pico_rtos_unblock_highest_priority_task(pool->block_obj);
    if (unblocked_task) {
        PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory pool free: unblocked waiting task");
    }
    
    PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory pool free: block %p, %u free available", 
                       block, pool->free_blocks);
    
    return true;
}

void pico_rtos_memory_pool_destroy(pico_rtos_memory_pool_t *pool) {
    if (!pool || !pool->initialized) {
        return;
    }
    
    critical_section_enter_blocking(&pool->cs);
    
    // Unblock all waiting tasks
    if (pool->block_obj) {
        pico_rtos_block_object_delete(pool->block_obj);
        pool->block_obj = NULL;
    }
    
    // Clear pool structure
    pool->initialized = false;
    pool->magic = 0;
    
    critical_section_exit(&pool->cs);
    critical_section_deinit(&pool->cs);
    
    PICO_RTOS_LOG_INFO(PICO_RTOS_LOG_SUBSYSTEM_MEMORY, "Memory pool destroyed");
}

// =============================================================================
// INFORMATION API IMPLEMENTATION
// =============================================================================

uint32_t pico_rtos_memory_pool_get_free_count(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return 0;
    }
    
    critical_section_enter_blocking(&pool->cs);
    uint32_t free_count = pool->free_blocks;
    critical_section_exit(&pool->cs);
    
    return free_count;
}

uint32_t pico_rtos_memory_pool_get_allocated_count(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return 0;
    }
    
    critical_section_enter_blocking(&pool->cs);
    uint32_t allocated_count = pool->total_blocks - pool->free_blocks;
    critical_section_exit(&pool->cs);
    
    return allocated_count;
}

uint32_t pico_rtos_memory_pool_get_total_count(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return 0;
    }
    
    return pool->total_blocks;
}

uint32_t pico_rtos_memory_pool_get_block_size(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return 0;
    }
    
    return pool->block_size;
}

bool pico_rtos_memory_pool_is_empty(pico_rtos_memory_pool_t *pool) {
    return pico_rtos_memory_pool_get_free_count(pool) == 0;
}

bool pico_rtos_memory_pool_is_full(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return false;
    }
    
    return pico_rtos_memory_pool_get_free_count(pool) == pool->total_blocks;
}

// =============================================================================
// STATISTICS API IMPLEMENTATION
// =============================================================================

#if PICO_RTOS_ENABLE_MEMORY_TRACKING

bool pico_rtos_memory_pool_get_stats(pico_rtos_memory_pool_t *pool, 
                                    pico_rtos_memory_pool_stats_t *stats) {
    if (!validate_pool_structure(pool) || !stats) {
        return false;
    }
    
    critical_section_enter_blocking(&pool->cs);
    
    // Update current state
    pool->stats.free_blocks = pool->free_blocks;
    pool->stats.allocated_blocks = pool->total_blocks - pool->free_blocks;
    
    // Calculate fragmentation ratio (for memory pools, this is always 0 since blocks are fixed-size)
    pool->stats.fragmentation_ratio = 0.0f;
    
    // Copy statistics
    *stats = pool->stats;
    
    critical_section_exit(&pool->cs);
    
    return true;
}

void pico_rtos_memory_pool_reset_stats(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return;
    }
    
    critical_section_enter_blocking(&pool->cs);
    
    // Reset counters but preserve current state
    uint32_t current_free = pool->stats.free_blocks;
    uint32_t current_allocated = pool->stats.allocated_blocks;
    uint32_t total_blocks = pool->stats.total_blocks;
    uint32_t block_size = pool->stats.block_size;
    uint32_t pool_size = pool->stats.pool_size;
    
    memset(&pool->stats, 0, sizeof(pool->stats));
    
    pool->stats.total_blocks = total_blocks;
    pool->stats.free_blocks = current_free;
    pool->stats.allocated_blocks = current_allocated;
    pool->stats.block_size = block_size;
    pool->stats.pool_size = pool_size;
    
    critical_section_exit(&pool->cs);
}

uint32_t pico_rtos_memory_pool_get_peak_allocated(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return 0;
    }
    
    critical_section_enter_blocking(&pool->cs);
    uint32_t peak = pool->stats.peak_allocated;
    critical_section_exit(&pool->cs);
    
    return peak;
}

float pico_rtos_memory_pool_get_fragmentation_ratio(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return -1.0f;
    }
    
    // Memory pools have no fragmentation since all blocks are fixed-size
    return 0.0f;
}

#endif // PICO_RTOS_ENABLE_MEMORY_TRACKING

// =============================================================================
// VALIDATION API IMPLEMENTATION
// =============================================================================

bool pico_rtos_memory_pool_validate(pico_rtos_memory_pool_t *pool) {
    if (!validate_pool_structure(pool)) {
        return false;
    }
    
    critical_section_enter_blocking(&pool->cs);
    
    // Count free blocks by traversing the free list
    uint32_t free_count = 0;
    pico_rtos_memory_block_t *current = pool->free_list;
    
    while (current) {
        // Validate magic number
        if (current->magic != PICO_RTOS_MEMORY_POOL_FREE_MAGIC) {
            critical_section_exit(&pool->cs);
            PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_CORRUPTION, (uint32_t)current);
            return false;
        }
        
        // Validate block ownership
        if (!validate_block_ownership(pool, current)) {
            critical_section_exit(&pool->cs);
            return false;
        }
        
        free_count++;
        current = current->next;
        
        // Prevent infinite loops
        if (free_count > pool->total_blocks) {
            critical_section_exit(&pool->cs);
            PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_CORRUPTION, free_count);
            return false;
        }
    }
    
    // Verify free count matches
    bool valid = (free_count == pool->free_blocks);
    
    critical_section_exit(&pool->cs);
    
    if (!valid) {
        PICO_RTOS_REPORT_ERROR(PICO_RTOS_ERROR_MEMORY_POOL_CORRUPTION, 
                              (free_count << 16) | pool->free_blocks);
    }
    
    return valid;
}

bool pico_rtos_memory_pool_contains_pointer(pico_rtos_memory_pool_t *pool, void *ptr) {
    if (!validate_pool_structure(pool)) {
        return false;
    }
    
    return validate_block_ownership(pool, ptr);
}

bool pico_rtos_memory_pool_is_block_allocated(pico_rtos_memory_pool_t *pool, void *block) {
    if (!validate_pool_structure(pool) || !block) {
        return false;
    }
    
    if (!validate_block_ownership(pool, block)) {
        return false;
    }
    
    critical_section_enter_blocking(&pool->cs);
    bool is_free = is_block_in_free_list(pool, block);
    critical_section_exit(&pool->cs);
    
    return !is_free;  // Allocated if not in free list
}

// =============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// =============================================================================

uint32_t pico_rtos_memory_pool_calculate_size(uint32_t block_size, uint32_t block_count) {
    if (block_size == 0 || block_count == 0) {
        return 0;
    }
    
    uint32_t aligned_block_size = align_size(block_size);
    
    // Ensure minimum block size for free list linkage
    if (aligned_block_size < sizeof(pico_rtos_memory_block_t)) {
        aligned_block_size = sizeof(pico_rtos_memory_block_t);
    }
    
    return aligned_block_size * block_count;
}

uint32_t pico_rtos_memory_pool_get_aligned_block_size(uint32_t requested_size) {
    uint32_t aligned_size = align_size(requested_size);
    
    // Ensure minimum size for free list linkage
    if (aligned_size < sizeof(pico_rtos_memory_block_t)) {
        aligned_size = sizeof(pico_rtos_memory_block_t);
    }
    
    return aligned_size;
}

uint32_t pico_rtos_memory_pool_calculate_max_blocks(uint32_t memory_size, uint32_t block_size) {
    if (memory_size == 0 || block_size == 0) {
        return 0;
    }
    
    uint32_t aligned_block_size = pico_rtos_memory_pool_get_aligned_block_size(block_size);
    return memory_size / aligned_block_size;
}