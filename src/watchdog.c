/**
 * @file watchdog.c
 * @brief Hardware Watchdog Integration Implementation
 * 
 * This module implements comprehensive hardware watchdog timer integration
 * for the RP2040, providing automatic feeding, timeout handling, and
 * system recovery capabilities.
 */

#include "pico_rtos/watchdog.h"
#include "pico_rtos/logging.h"
#include "pico_rtos.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "hardware/regs/watchdog.h"
#include "pico/bootrom.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

static pico_rtos_watchdog_system_t g_watchdog_system = {0};
static pico_rtos_watchdog_recovery_callback_t g_recovery_callback = NULL;
static void *g_recovery_callback_data = NULL;

// Recovery data storage (in RAM that survives watchdog reset)
#define RECOVERY_DATA_SIZE 256
static uint8_t g_recovery_data[RECOVERY_DATA_SIZE] __attribute__((section(".uninitialized_data")));
static size_t g_recovery_data_size __attribute__((section(".uninitialized_data")));
static uint32_t g_recovery_magic __attribute__((section(".uninitialized_data")));
#define RECOVERY_MAGIC 0xDEADBEEF

// =============================================================================
// STRING CONSTANTS
// =============================================================================

static const char *reset_reason_strings[] = {
    "None",
    "Timeout",
    "Software",
    "Power On",
    "External",
    "Unknown"
};

static const char *watchdog_state_strings[] = {
    "Disabled",
    "Enabled",
    "Feeding",
    "Warning",
    "Timeout"
};

static const char *watchdog_action_strings[] = {
    "Reset",
    "Callback",
    "Interrupt",
    "Debug Halt"
};

// =============================================================================
// INTERNAL HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Get current time in milliseconds
 * @return Current time in milliseconds
 */
static inline uint32_t get_current_time_ms(void)
{
    return pico_rtos_get_tick_count();
}

/**
 * @brief Find handler by ID
 * @param handler_id Handler ID to find
 * @return Pointer to handler, or NULL if not found
 */
static pico_rtos_watchdog_handler_t *find_handler_by_id(uint32_t handler_id)
{
    if (handler_id == 0 || handler_id > g_watchdog_system.handler_count) {
        return NULL;
    }
    
    return &g_watchdog_system.handlers[handler_id - 1];
}

/**
 * @brief Call timeout handlers
 * @param remaining_ms Remaining time before timeout
 * @return Action to take
 */
static pico_rtos_watchdog_action_t call_timeout_handlers(uint32_t remaining_ms)
{
    pico_rtos_watchdog_action_t action = g_watchdog_system.config.default_action;
    
    // Sort handlers by priority (higher priority first)
    for (uint32_t i = 0; i < g_watchdog_system.handler_count; i++) {
        pico_rtos_watchdog_handler_t *handler = &g_watchdog_system.handlers[i];
        
        if (handler->enabled && handler->timeout_callback != NULL) {
            pico_rtos_watchdog_action_t handler_action = 
                handler->timeout_callback(remaining_ms, handler->user_data);
            
            handler->timeout_count++;
            
            // Higher priority handlers can override the action
            if (handler->priority > 0) {
                action = handler_action;
            }
        }
    }
    
    return action;
}

/**
 * @brief Call feed handlers
 * @return true if feeding should proceed, false otherwise
 */
static bool call_feed_handlers(void)
{
    bool allow_feed = true;
    
    for (uint32_t i = 0; i < g_watchdog_system.handler_count; i++) {
        pico_rtos_watchdog_handler_t *handler = &g_watchdog_system.handlers[i];
        
        if (handler->enabled && handler->feed_callback != NULL) {
            bool handler_result = handler->feed_callback(handler->user_data);
            handler->feed_count++;
            
            // Any handler can prevent feeding
            if (!handler_result) {
                allow_feed = false;
            }
        }
    }
    
    return allow_feed;
}

/**
 * @brief Update watchdog statistics
 */
static void update_statistics(void)
{
    uint32_t current_time = get_current_time_ms();
    
    if (g_watchdog_system.last_feed_time_ms > 0) {
        uint32_t feed_interval = current_time - g_watchdog_system.last_feed_time_ms;
        
        if (g_watchdog_system.stats.total_feeds == 0) {
            g_watchdog_system.stats.min_feed_interval_ms = feed_interval;
            g_watchdog_system.stats.max_feed_interval_ms = feed_interval;
            g_watchdog_system.stats.avg_feed_interval_ms = feed_interval;
        } else {
            if (feed_interval < g_watchdog_system.stats.min_feed_interval_ms) {
                g_watchdog_system.stats.min_feed_interval_ms = feed_interval;
            }
            if (feed_interval > g_watchdog_system.stats.max_feed_interval_ms) {
                g_watchdog_system.stats.max_feed_interval_ms = feed_interval;
            }
            
            // Update running average
            g_watchdog_system.stats.avg_feed_interval_ms = 
                ((g_watchdog_system.stats.avg_feed_interval_ms * g_watchdog_system.stats.total_feeds) + 
                 feed_interval) / (g_watchdog_system.stats.total_feeds + 1);
        }
    }
    
    g_watchdog_system.stats.total_feeds++;
    g_watchdog_system.last_feed_time_ms = current_time;
}

/**
 * @brief Determine reset reason from hardware
 * @return Reset reason
 */
static pico_rtos_watchdog_reset_reason_t determine_reset_reason(void)
{
    // Check if we have recovery magic (indicates watchdog reset)
    if (g_recovery_magic == RECOVERY_MAGIC) {
        return PICO_RTOS_WATCHDOG_RESET_TIMEOUT;
    }
    
    // Check hardware reset status register
    uint32_t reset_status = watchdog_hw->reason;
    
    if (reset_status & WATCHDOG_REASON_TIMER_BITS) {
        return PICO_RTOS_WATCHDOG_RESET_TIMEOUT;
    }
    
    // For RP2040, we can't easily distinguish between other reset types
    // so we'll return power-on as default
    return PICO_RTOS_WATCHDOG_RESET_POWER_ON;
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

bool pico_rtos_watchdog_init(const pico_rtos_watchdog_config_t *config)
{
    if (g_watchdog_system.initialized) {
        return true;
    }
    
    // Initialize critical section
    critical_section_init(&g_watchdog_system.cs);
    
    // Initialize system state
    memset(&g_watchdog_system, 0, sizeof(g_watchdog_system));
    
    // Set default configuration
    if (config != NULL) {
        g_watchdog_system.config = *config;
    } else {
        g_watchdog_system.config.timeout_ms = PICO_RTOS_WATCHDOG_DEFAULT_TIMEOUT_MS;
        g_watchdog_system.config.feed_interval_ms = PICO_RTOS_WATCHDOG_FEED_INTERVAL_MS;
        g_watchdog_system.config.auto_feed_enabled = true;
        g_watchdog_system.config.pause_on_debug = true;
        g_watchdog_system.config.default_action = PICO_RTOS_WATCHDOG_ACTION_RESET;
        g_watchdog_system.config.warning_threshold_ms = 1000;
        g_watchdog_system.config.enable_early_warning = true;
    }
    
    g_watchdog_system.state = PICO_RTOS_WATCHDOG_STATE_DISABLED;
    g_watchdog_system.stats.last_reset_reason = determine_reset_reason();
    g_watchdog_system.stats.last_reset_timestamp = get_current_time_ms();
    
    g_watchdog_system.initialized = true;
    
    PICO_RTOS_LOG_INFO("Watchdog system initialized (reset reason: %s)",
                       pico_rtos_watchdog_get_reset_reason_string(g_watchdog_system.stats.last_reset_reason));
    
    return true;
}

bool pico_rtos_watchdog_enable(uint32_t timeout_ms)
{
    if (!g_watchdog_system.initialized) {
        PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_WATCHDOG, "Watchdog system not initialized");
        return false;
    }
    
    if (timeout_ms < 1000 || timeout_ms > 0x7FFFFF) { // RP2040 limits
        PICO_RTOS_LOG_ERROR("Invalid watchdog timeout: %u ms", timeout_ms);
        return false;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    
    // Configure hardware watchdog
    watchdog_enable(timeout_ms, g_watchdog_system.config.pause_on_debug);
    
    g_watchdog_system.config.timeout_ms = timeout_ms;
    g_watchdog_system.hardware_enabled = true;
    g_watchdog_system.hardware_timeout_ms = timeout_ms;
    g_watchdog_system.state = PICO_RTOS_WATCHDOG_STATE_ENABLED;
    g_watchdog_system.last_feed_time_ms = get_current_time_ms();
    g_watchdog_system.next_feed_time_ms = g_watchdog_system.last_feed_time_ms + 
                                         g_watchdog_system.config.feed_interval_ms;
    g_watchdog_system.timeout_time_ms = g_watchdog_system.last_feed_time_ms + timeout_ms;
    g_watchdog_system.warning_issued = false;
    
    critical_section_exit(&g_watchdog_system.cs);
    
    PICO_RTOS_LOG_INFO("Watchdog enabled with %u ms timeout", timeout_ms);
    return true;
}

bool pico_rtos_watchdog_disable(void)
{
    if (!g_watchdog_system.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    
    // Note: RP2040 watchdog cannot be disabled once enabled
    // We can only stop feeding it, which will cause a reset
    g_watchdog_system.state = PICO_RTOS_WATCHDOG_STATE_DISABLED;
    g_watchdog_system.config.auto_feed_enabled = false;
    
    critical_section_exit(&g_watchdog_system.cs);
    
    PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_WATCHDOG, "Watchdog disabled (will cause reset if not fed manually)");
    return true;
}

bool pico_rtos_watchdog_is_enabled(void)
{
    if (!g_watchdog_system.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    bool enabled = (g_watchdog_system.state != PICO_RTOS_WATCHDOG_STATE_DISABLED);
    critical_section_exit(&g_watchdog_system.cs);
    
    return enabled;
}

bool pico_rtos_watchdog_feed(void)
{
    if (!g_watchdog_system.initialized || !g_watchdog_system.hardware_enabled) {
        return false;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    
    // Call feed handlers to check if feeding should proceed
    if (!call_feed_handlers()) {
        g_watchdog_system.stats.missed_feeds++;
        critical_section_exit(&g_watchdog_system.cs);
        PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_WATCHDOG, "Watchdog feed blocked by handler");
        return false;
    }
    
    // Feed the hardware watchdog
    watchdog_update();
    
    // Update state and statistics
    update_statistics();
    g_watchdog_system.state = PICO_RTOS_WATCHDOG_STATE_FEEDING;
    g_watchdog_system.timeout_time_ms = get_current_time_ms() + g_watchdog_system.config.timeout_ms;
    g_watchdog_system.warning_issued = false;
    
    critical_section_exit(&g_watchdog_system.cs);
    
    PICO_RTOS_LOG_DEBUG(PICO_RTOS_LOG_SUBSYSTEM_WATCHDOG, "Watchdog fed");
    return true;
}

void pico_rtos_watchdog_force_reset(void)
{
    PICO_RTOS_LOG_WARN(PICO_RTOS_LOG_SUBSYSTEM_WATCHDOG, "Forcing watchdog reset");
    
    // Save recovery magic
    g_recovery_magic = RECOVERY_MAGIC;
    
    // Force immediate watchdog reset
    watchdog_reboot(0, 0, 0);
    
    // This should not return
    while (1) {
        __asm volatile ("nop");
    }
}

bool pico_rtos_watchdog_set_timeout(uint32_t timeout_ms)
{
    if (!g_watchdog_system.initialized) {
        return false;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    g_watchdog_system.config.timeout_ms = timeout_ms;
    critical_section_exit(&g_watchdog_system.cs);
    
    // If watchdog is enabled, we need to restart it with new timeout
    if (g_watchdog_system.hardware_enabled) {
        return pico_rtos_watchdog_enable(timeout_ms);
    }
    
    return true;
}

uint32_t pico_rtos_watchdog_get_timeout(void)
{
    if (!g_watchdog_system.initialized) {
        return 0;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    uint32_t timeout = g_watchdog_system.config.timeout_ms;
    critical_section_exit(&g_watchdog_system.cs);
    
    return timeout;
}

uint32_t pico_rtos_watchdog_register_handler(const char *name,
                                            pico_rtos_watchdog_timeout_callback_t timeout_callback,
                                            pico_rtos_watchdog_feed_callback_t feed_callback,
                                            void *user_data,
                                            uint32_t priority)
{
    if (!g_watchdog_system.initialized) {
        return 0;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    
    if (g_watchdog_system.handler_count >= PICO_RTOS_WATCHDOG_MAX_HANDLERS) {
        critical_section_exit(&g_watchdog_system.cs);
        PICO_RTOS_LOG_ERROR(PICO_RTOS_LOG_SUBSYSTEM_WATCHDOG, "Maximum number of watchdog handlers exceeded");
        return 0;
    }
    
    pico_rtos_watchdog_handler_t *handler = &g_watchdog_system.handlers[g_watchdog_system.handler_count];
    
    handler->name = name;
    handler->timeout_callback = timeout_callback;
    handler->feed_callback = feed_callback;
    handler->user_data = user_data;
    handler->priority = priority;
    handler->enabled = true;
    handler->timeout_count = 0;
    handler->feed_count = 0;
    
    uint32_t handler_id = ++g_watchdog_system.handler_count;
    
    critical_section_exit(&g_watchdog_system.cs);
    
    PICO_RTOS_LOG_DEBUG("Registered watchdog handler %u: %s", handler_id, name ? name : "unnamed");
    return handler_id;
}

pico_rtos_watchdog_state_t pico_rtos_watchdog_get_state(void)
{
    if (!g_watchdog_system.initialized) {
        return PICO_RTOS_WATCHDOG_STATE_DISABLED;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    pico_rtos_watchdog_state_t state = g_watchdog_system.state;
    critical_section_exit(&g_watchdog_system.cs);
    
    return state;
}

uint32_t pico_rtos_watchdog_get_remaining_time(void)
{
    if (!g_watchdog_system.initialized || !g_watchdog_system.hardware_enabled) {
        return 0;
    }
    
    uint32_t current_time = get_current_time_ms();
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    
    if (current_time >= g_watchdog_system.timeout_time_ms) {
        critical_section_exit(&g_watchdog_system.cs);
        return 0;
    }
    
    uint32_t remaining = g_watchdog_system.timeout_time_ms - current_time;
    critical_section_exit(&g_watchdog_system.cs);
    
    return remaining;
}

uint32_t pico_rtos_watchdog_get_time_since_feed(void)
{
    if (!g_watchdog_system.initialized) {
        return 0;
    }
    
    uint32_t current_time = get_current_time_ms();
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    uint32_t time_since_feed = current_time - g_watchdog_system.last_feed_time_ms;
    critical_section_exit(&g_watchdog_system.cs);
    
    return time_since_feed;
}

bool pico_rtos_watchdog_get_statistics(pico_rtos_watchdog_statistics_t *stats)
{
    if (!g_watchdog_system.initialized || stats == NULL) {
        return false;
    }
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    *stats = g_watchdog_system.stats;
    stats->uptime_since_last_reset_ms = get_current_time_ms() - g_watchdog_system.stats.last_reset_timestamp;
    critical_section_exit(&g_watchdog_system.cs);
    
    return true;
}

pico_rtos_watchdog_reset_reason_t pico_rtos_watchdog_get_reset_reason(void)
{
    if (!g_watchdog_system.initialized) {
        return PICO_RTOS_WATCHDOG_RESET_UNKNOWN;
    }
    
    return g_watchdog_system.stats.last_reset_reason;
}

bool pico_rtos_watchdog_save_recovery_data(const void *data, size_t size)
{
    if (data == NULL || size == 0 || size > RECOVERY_DATA_SIZE) {
        return false;
    }
    
    memcpy(g_recovery_data, data, size);
    g_recovery_data_size = size;
    g_recovery_magic = RECOVERY_MAGIC;
    
    return true;
}

bool pico_rtos_watchdog_restore_recovery_data(void *data, size_t size, size_t *actual_size)
{
    if (data == NULL || size == 0 || g_recovery_magic != RECOVERY_MAGIC) {
        return false;
    }
    
    size_t copy_size = (size < g_recovery_data_size) ? size : g_recovery_data_size;
    memcpy(data, g_recovery_data, copy_size);
    
    if (actual_size != NULL) {
        *actual_size = copy_size;
    }
    
    return true;
}

// Utility functions
const char *pico_rtos_watchdog_get_reset_reason_string(pico_rtos_watchdog_reset_reason_t reason)
{
    if (reason < sizeof(reset_reason_strings) / sizeof(reset_reason_strings[0])) {
        return reset_reason_strings[reason];
    }
    return "Unknown";
}

const char *pico_rtos_watchdog_get_state_string(pico_rtos_watchdog_state_t state)
{
    if (state < sizeof(watchdog_state_strings) / sizeof(watchdog_state_strings[0])) {
        return watchdog_state_strings[state];
    }
    return "Unknown";
}

const char *pico_rtos_watchdog_get_action_string(pico_rtos_watchdog_action_t action)
{
    if (action < sizeof(watchdog_action_strings) / sizeof(watchdog_action_strings[0])) {
        return watchdog_action_strings[action];
    }
    return "Unknown";
}

void pico_rtos_watchdog_print_status(void)
{
    if (!g_watchdog_system.initialized) {
        printf("Watchdog system not initialized\n");
        return;
    }
    
    printf("=== Watchdog Status ===\n");
    printf("State: %s\n", pico_rtos_watchdog_get_state_string(g_watchdog_system.state));
    printf("Enabled: %s\n", g_watchdog_system.hardware_enabled ? "Yes" : "No");
    printf("Timeout: %lu ms\n", g_watchdog_system.config.timeout_ms);
    printf("Remaining: %lu ms\n", pico_rtos_watchdog_get_remaining_time());
    printf("Time since feed: %lu ms\n", pico_rtos_watchdog_get_time_since_feed());
    printf("Auto-feed: %s\n", g_watchdog_system.config.auto_feed_enabled ? "Enabled" : "Disabled");
    printf("Handlers: %lu registered\n", g_watchdog_system.handler_count);
    printf("Last reset: %s\n", pico_rtos_watchdog_get_reset_reason_string(g_watchdog_system.stats.last_reset_reason));
    printf("=======================\n");
}

// Internal system functions
void pico_rtos_watchdog_periodic_update(void)
{
    if (!g_watchdog_system.initialized || !g_watchdog_system.hardware_enabled) {
        return;
    }
    
    uint32_t current_time = get_current_time_ms();
    
    critical_section_enter_blocking(&g_watchdog_system.cs);
    
    // Check for early warning
    if (g_watchdog_system.config.enable_early_warning && !g_watchdog_system.warning_issued) {
        uint32_t remaining = (current_time < g_watchdog_system.timeout_time_ms) ?
                           g_watchdog_system.timeout_time_ms - current_time : 0;
        
        if (remaining <= g_watchdog_system.config.warning_threshold_ms) {
            g_watchdog_system.warning_issued = true;
            g_watchdog_system.stats.early_warnings++;
            g_watchdog_system.state = PICO_RTOS_WATCHDOG_STATE_WARNING;
            
            PICO_RTOS_LOG_WARN("Watchdog warning: %u ms remaining", remaining);
            
            // Call timeout handlers with warning
            pico_rtos_watchdog_action_t action = call_timeout_handlers(remaining);
            
            if (action == PICO_RTOS_WATCHDOG_ACTION_RESET) {
                critical_section_exit(&g_watchdog_system.cs);
                pico_rtos_watchdog_force_reset();
                return; // Should not reach here
            }
        }
    }
    
    // Check for automatic feeding
    if (g_watchdog_system.config.auto_feed_enabled && 
        current_time >= g_watchdog_system.next_feed_time_ms) {
        
        critical_section_exit(&g_watchdog_system.cs);
        
        if (pico_rtos_watchdog_feed()) {
            critical_section_enter_blocking(&g_watchdog_system.cs);
            g_watchdog_system.next_feed_time_ms = current_time + g_watchdog_system.config.feed_interval_ms;
            critical_section_exit(&g_watchdog_system.cs);
        } else {
            critical_section_enter_blocking(&g_watchdog_system.cs);
        }
    }
    
    critical_section_exit(&g_watchdog_system.cs);
}