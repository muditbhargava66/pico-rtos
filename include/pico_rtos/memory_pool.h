#ifndef PICO_RTOS_MEMORY_POOL_H
#define PICO_RTOS_MEMORY_POOL_H

#include "config.h"
#include "types.h"
#include "error.h"
#include "pico/critical_section.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file memory_pool.h
 * @brief Memory Pool Management for Pico-RTOS v0.3.0
 * 
 * This module provides O(1) fixed-size block allocation and deallocation
 * using memory pools. Memory pools reduce fragmentation and provide
 * deterministic allocation performance for real-time applications.
 * 
 * Features:
 * - O(1) allocation and deallocation performance
 * - Thread-safe operations with critical sections
 * - Block validation and corruption detection
 * - Comprehensive statistics and monitoring
 * - Integration with existing blocking system for timeout support
 */

// =============================================================================
// CONFIGURATION AND CONSTANTS
// =============================================================================

#ifndef PICO_RTOS_MEMORY_POOL_ALIGNMENT
#define PICO_RTOS_MEMORY_POOL_ALIGNMENT 4  ///< Memory alignment requirement (bytes)
#endif

#ifndef PICO_RTOS_MEMORY_POOL_MAGIC
#define PICO_RTOS_MEMORY_POOL_MAGIC 0xDEADBEEF  ///< Magic number for corruption detection
#endif

#ifndef PICO_RTOS_MEMORY_POOL_FREE_MAGIC
#define PICO_RTOS_MEMORY_POOL_FREE_MAGIC 0xFEEDFACE  ///< Magic number for free blocks
#endif

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Memory block header for free list linkage
 * 
 * This structure is placed at the beginning of each free block
 * to maintain the free list. When a block is allocated, this
 * header is overwritten by user data.
 */
typedef struct pico_rtos_memory_block {
    struct pico_rtos_memory_block *next;  ///< Next free block in list
    uint32_t magic;                       ///< Magic number for corruption detection
} pico_rtos_memory_block_t;

/**
 * @brief Memory pool statistics structure
 * 
 * Provides detailed information about memory pool usage,
 * performance, and health for debugging and monitoring.
 */
typedef struct {
    uint32_t total_blocks;          ///< Total number of blocks in pool
    uint32_t free_blocks;           ///< Currently available blocks
    uint32_t allocated_blocks;      ///< Currently allocated blocks
    uint32_t peak_allocated;        ///< Peak number of allocated blocks
    uint32_t allocation_count;      ///< Total allocations performed
    uint32_t deallocation_count;    ///< Total deallocations performed
    uint32_t allocation_failures;   ///< Number of failed allocations
    uint32_t corruption_detected;   ///< Number of corruption events detected
    uint32_t double_free_detected;  ///< Number of double-free attempts detected
    uint32_t block_size;            ///< Size of each block in bytes
    uint32_t pool_size;             ///< Total pool size in bytes
    float fragmentation_ratio;      ///< Fragmentation ratio (0.0 = no fragmentation)
    uint32_t avg_alloc_time_cycles; ///< Average allocation time in CPU cycles
    uint32_t avg_free_time_cycles;  ///< Average deallocation time in CPU cycles
} pico_rtos_memory_pool_stats_t;

/**
 * @brief Memory pool control structure
 * 
 * Main structure that manages a memory pool. Contains all necessary
 * information for pool management, statistics, and thread safety.
 */
typedef struct {
    void *pool_start;                    ///< Start address of memory pool
    uint32_t block_size;                 ///< Size of each block (aligned)
    uint32_t total_blocks;               ///< Total number of blocks
    uint32_t free_blocks;                ///< Currently free blocks
    pico_rtos_memory_block_t *free_list; ///< Head of free list
    critical_section_t cs;               ///< Critical section for thread safety
    uint32_t magic;                      ///< Pool magic number for validation
    bool initialized;                    ///< Initialization flag
    
    // Statistics (conditionally compiled)
#if PICO_RTOS_ENABLE_MEMORY_TRACKING
    pico_rtos_memory_pool_stats_t stats; ///< Pool statistics
    uint32_t last_alloc_cycles;          ///< Last allocation timing
    uint32_t last_free_cycles;           ///< Last deallocation timing
#endif
    
    // Optional blocking support for allocation timeouts
    struct pico_rtos_block_object *block_obj; ///< Blocking object for waiting tasks
} pico_rtos_memory_pool_t;

// =============================================================================
// CORE API FUNCTIONS
// =============================================================================

/**
 * @brief Initialize a memory pool
 * 
 * Initializes a memory pool with the provided memory region. The memory
 * region must be large enough to hold the specified number of blocks
 * plus any alignment overhead.
 * 
 * @param pool Pointer to memory pool structure to initialize
 * @param memory Pointer to memory region for the pool
 * @param block_size Size of each block in bytes (will be aligned)
 * @param block_count Number of blocks to create in the pool
 * @return true if initialization successful, false otherwise
 * 
 * @note The actual block size may be larger than requested due to alignment
 * @note The memory region must remain valid for the lifetime of the pool
 */
bool pico_rtos_memory_pool_init(pico_rtos_memory_pool_t *pool, 
                               void *memory, 
                               uint32_t block_size, 
                               uint32_t block_count);

/**
 * @brief Allocate a block from the memory pool
 * 
 * Allocates a fixed-size block from the memory pool. This operation
 * is O(1) and thread-safe. If no blocks are available and timeout
 * is specified, the calling task will block until a block becomes
 * available or the timeout expires.
 * 
 * @param pool Pointer to the memory pool
 * @param timeout Timeout in milliseconds (PICO_RTOS_WAIT_FOREVER for no timeout,
 *                PICO_RTOS_NO_WAIT for immediate return)
 * @return Pointer to allocated block, or NULL if allocation failed
 * 
 * @note The returned block is guaranteed to be properly aligned
 * @note The block contents are undefined after allocation
 */
void *pico_rtos_memory_pool_alloc(pico_rtos_memory_pool_t *pool, uint32_t timeout);

/**
 * @brief Free a block back to the memory pool
 * 
 * Returns a previously allocated block back to the memory pool.
 * This operation is O(1) and thread-safe. The block is validated
 * to ensure it belongs to the pool and detect corruption.
 * 
 * @param pool Pointer to the memory pool
 * @param block Pointer to the block to free
 * @return true if deallocation successful, false if invalid block or corruption detected
 * 
 * @note The block contents become undefined after freeing
 * @note Double-free attempts are detected and reported as errors
 */
bool pico_rtos_memory_pool_free(pico_rtos_memory_pool_t *pool, void *block);

/**
 * @brief Destroy a memory pool
 * 
 * Destroys a memory pool and releases all associated resources.
 * Any tasks blocked waiting for allocations will be unblocked
 * with allocation failure.
 * 
 * @param pool Pointer to the memory pool to destroy
 * 
 * @note The underlying memory region is not freed - it must be
 *       managed by the caller
 */
void pico_rtos_memory_pool_destroy(pico_rtos_memory_pool_t *pool);

// =============================================================================
// INFORMATION AND STATISTICS API
// =============================================================================

/**
 * @brief Get the number of free blocks in the pool
 * 
 * @param pool Pointer to the memory pool
 * @return Number of currently available blocks, or 0 if pool is invalid
 */
uint32_t pico_rtos_memory_pool_get_free_count(pico_rtos_memory_pool_t *pool);

/**
 * @brief Get the number of allocated blocks in the pool
 * 
 * @param pool Pointer to the memory pool
 * @return Number of currently allocated blocks, or 0 if pool is invalid
 */
uint32_t pico_rtos_memory_pool_get_allocated_count(pico_rtos_memory_pool_t *pool);

/**
 * @brief Get the total number of blocks in the pool
 * 
 * @param pool Pointer to the memory pool
 * @return Total number of blocks in the pool, or 0 if pool is invalid
 */
uint32_t pico_rtos_memory_pool_get_total_count(pico_rtos_memory_pool_t *pool);

/**
 * @brief Get the block size for the pool
 * 
 * @param pool Pointer to the memory pool
 * @return Size of each block in bytes (aligned), or 0 if pool is invalid
 */
uint32_t pico_rtos_memory_pool_get_block_size(pico_rtos_memory_pool_t *pool);

/**
 * @brief Check if the pool is empty (no free blocks)
 * 
 * @param pool Pointer to the memory pool
 * @return true if pool has no free blocks, false otherwise
 */
bool pico_rtos_memory_pool_is_empty(pico_rtos_memory_pool_t *pool);

/**
 * @brief Check if the pool is full (no allocated blocks)
 * 
 * @param pool Pointer to the memory pool
 * @return true if all blocks are free, false otherwise
 */
bool pico_rtos_memory_pool_is_full(pico_rtos_memory_pool_t *pool);

// =============================================================================
// STATISTICS AND MONITORING API (if enabled)
// =============================================================================

#if PICO_RTOS_ENABLE_MEMORY_TRACKING

/**
 * @brief Get comprehensive statistics for a memory pool
 * 
 * Retrieves detailed statistics about pool usage, performance,
 * and health. This information is useful for debugging,
 * optimization, and system monitoring.
 * 
 * @param pool Pointer to the memory pool
 * @param stats Pointer to structure to fill with statistics
 * @return true if statistics retrieved successfully, false if pool invalid
 */
bool pico_rtos_memory_pool_get_stats(pico_rtos_memory_pool_t *pool, 
                                    pico_rtos_memory_pool_stats_t *stats);

/**
 * @brief Reset statistics for a memory pool
 * 
 * Resets all counters and statistics to zero while preserving
 * current allocation state. Useful for performance measurement
 * over specific time periods.
 * 
 * @param pool Pointer to the memory pool
 */
void pico_rtos_memory_pool_reset_stats(pico_rtos_memory_pool_t *pool);

/**
 * @brief Get peak allocation count since initialization or last reset
 * 
 * @param pool Pointer to the memory pool
 * @return Peak number of simultaneously allocated blocks
 */
uint32_t pico_rtos_memory_pool_get_peak_allocated(pico_rtos_memory_pool_t *pool);

/**
 * @brief Get fragmentation ratio for the pool
 * 
 * Calculates the fragmentation ratio based on free block distribution.
 * A value of 0.0 indicates no fragmentation (all free blocks are
 * contiguous), while higher values indicate more fragmentation.
 * 
 * @param pool Pointer to the memory pool
 * @return Fragmentation ratio (0.0 to 1.0), or -1.0 if pool invalid
 */
float pico_rtos_memory_pool_get_fragmentation_ratio(pico_rtos_memory_pool_t *pool);

#endif // PICO_RTOS_ENABLE_MEMORY_TRACKING

// =============================================================================
// VALIDATION AND DEBUG API
// =============================================================================

/**
 * @brief Validate memory pool integrity
 * 
 * Performs comprehensive validation of the memory pool structure,
 * free list integrity, and block corruption detection. This is
 * useful for debugging memory corruption issues.
 * 
 * @param pool Pointer to the memory pool
 * @return true if pool is valid and uncorrupted, false otherwise
 */
bool pico_rtos_memory_pool_validate(pico_rtos_memory_pool_t *pool);

/**
 * @brief Check if a pointer belongs to the memory pool
 * 
 * Determines whether a given pointer points to a valid block
 * within the memory pool's address range.
 * 
 * @param pool Pointer to the memory pool
 * @param ptr Pointer to check
 * @return true if pointer is within pool bounds, false otherwise
 */
bool pico_rtos_memory_pool_contains_pointer(pico_rtos_memory_pool_t *pool, void *ptr);

/**
 * @brief Check if a block is currently allocated
 * 
 * Determines whether a specific block is currently allocated
 * or free. This is useful for debugging double-free issues.
 * 
 * @param pool Pointer to the memory pool
 * @param block Pointer to the block to check
 * @return true if block is allocated, false if free or invalid
 */
bool pico_rtos_memory_pool_is_block_allocated(pico_rtos_memory_pool_t *pool, void *block);

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Calculate required memory size for a pool
 * 
 * Calculates the total memory size required to create a memory pool
 * with the specified parameters, including alignment overhead.
 * 
 * @param block_size Desired block size in bytes
 * @param block_count Number of blocks
 * @return Total memory size required in bytes, or 0 if parameters invalid
 */
uint32_t pico_rtos_memory_pool_calculate_size(uint32_t block_size, uint32_t block_count);

/**
 * @brief Get aligned block size
 * 
 * Returns the actual block size that will be used after alignment.
 * This may be larger than the requested size due to alignment requirements.
 * 
 * @param requested_size Requested block size in bytes
 * @return Aligned block size in bytes
 */
uint32_t pico_rtos_memory_pool_get_aligned_block_size(uint32_t requested_size);

/**
 * @brief Calculate maximum blocks for given memory size
 * 
 * Calculates the maximum number of blocks that can fit in a given
 * memory region with the specified block size.
 * 
 * @param memory_size Total available memory size in bytes
 * @param block_size Block size in bytes
 * @return Maximum number of blocks that can fit
 */
uint32_t pico_rtos_memory_pool_calculate_max_blocks(uint32_t memory_size, uint32_t block_size);

// =============================================================================
// ERROR CODES
// =============================================================================

// Memory pool specific error codes (extending the existing error system)
#define PICO_RTOS_ERROR_MEMORY_POOL_INVALID_PARAM    (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 1)
#define PICO_RTOS_ERROR_MEMORY_POOL_NOT_INITIALIZED  (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 2)
#define PICO_RTOS_ERROR_MEMORY_POOL_INVALID_BLOCK    (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 3)
#define PICO_RTOS_ERROR_MEMORY_POOL_CORRUPTION       (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 4)
#define PICO_RTOS_ERROR_MEMORY_POOL_DOUBLE_FREE      (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 5)
#define PICO_RTOS_ERROR_MEMORY_POOL_TIMEOUT          (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 6)
#define PICO_RTOS_ERROR_MEMORY_POOL_ALIGNMENT        (PICO_RTOS_ERROR_MEMORY_POOL_EXHAUSTED + 7)

#endif // PICO_RTOS_MEMORY_POOL_H