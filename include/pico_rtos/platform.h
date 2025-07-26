#ifndef PICO_RTOS_PLATFORM_H
#define PICO_RTOS_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// PLATFORM-SPECIFIC INCLUDES AND DEFINITIONS
// =============================================================================

// Platform-specific definitions
#ifdef PICO_PLATFORM
// When building for Pico platform, use SDK types and functions directly
// No need to redefine anything - just include the headers
#include "pico/critical_section.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

// Define Pico-RTOS specific critical section type to avoid conflicts
typedef critical_section_t pico_rtos_critical_section_t;

// Inline wrappers for consistency
static inline void pico_rtos_critical_section_init(pico_rtos_critical_section_t *cs) {
    critical_section_init(cs);
}

static inline void pico_rtos_critical_section_enter_blocking(pico_rtos_critical_section_t *cs) {
    critical_section_enter_blocking(cs);
}

static inline void pico_rtos_critical_section_exit(pico_rtos_critical_section_t *cs) {
    critical_section_exit(cs);
}

#else
// Mock implementations for testing and non-Pico platforms

// Mock critical section for non-Pico builds
typedef struct {
    int dummy;
} pico_rtos_critical_section_t;

static inline void pico_rtos_critical_section_init(pico_rtos_critical_section_t *cs) { 
    (void)cs; 
}

static inline void pico_rtos_critical_section_enter_blocking(pico_rtos_critical_section_t *cs) { 
    (void)cs; 
}

static inline void pico_rtos_critical_section_exit(pico_rtos_critical_section_t *cs) { 
    (void)cs; 
}

// Mock Pico SDK functions for non-Pico builds - only define if not using Pico SDK
// Note: When building for Pico platform, these functions are provided by the SDK
#if !defined(PICO_SDK_VERSION_MAJOR) && !defined(PICO_PLATFORM) && !defined(PICO_BOARD)
static inline uint32_t get_core_num(void) { 
    return 0; 
}

static inline void multicore_launch_core1(void (*entry)(void)) { 
    (void)entry; 
}

static inline void multicore_fifo_push_blocking(uint32_t data) { 
    (void)data; 
}

static inline void sleep_ms(uint32_t ms) { 
    (void)ms; 
}

static inline void __wfi(void) { 
    // Wait for interrupt - no-op in test environment
}
#endif // Mock functions guard

#endif // PICO_PLATFORM

#ifdef __cplusplus
}
#endif

#endif // PICO_RTOS_PLATFORM_H