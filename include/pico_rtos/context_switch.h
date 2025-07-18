#ifndef PICO_RTOS_CONTEXT_SWITCH_H
#define PICO_RTOS_CONTEXT_SWITCH_H

#include <stdint.h>
#include "task.h"

// Stack frame structure for ARM Cortex-M0+
// This represents the CPU state saved on the stack during context switch
typedef struct {
    // Registers saved by software (in PendSV handler)
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    
    // Registers saved by hardware (exception entry)
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;    // Link register (R14)
    uint32_t pc;    // Program counter (R15)
    uint32_t psr;   // Program status register
} pico_rtos_stack_frame_t;

// Global variables for context switching (defined in context_switch.c)
extern uint32_t *current_task_stack_ptr;
extern uint32_t *next_task_stack_ptr;

/**
 * @brief Initialize a task's stack with proper initial context
 * 
 * @param stack_base Base address of the task's stack
 * @param stack_size Size of the stack in bytes
 * @param task_function Entry point of the task
 * @param param Parameter to pass to the task
 * @return uint32_t* Initial stack pointer for the task
 */
uint32_t *pico_rtos_init_task_stack(uint32_t *stack_base, uint32_t stack_size, 
                                   pico_rtos_task_function_t task_function, void *param);

/**
 * @brief Trigger a context switch
 * 
 * This function sets the PendSV interrupt to trigger a context switch.
 * The actual switch happens in the PendSV handler.
 */
void pico_rtos_context_switch(void);

/**
 * @brief Start the first task
 * 
 * This function starts the scheduler by jumping to the first task.
 * It should only be called once when starting the RTOS.
 */
void pico_rtos_start_first_task(void);

/**
 * @brief Save current task context (C-callable)
 */
void pico_rtos_save_context(void);

/**
 * @brief Restore next task context (C-callable)
 */
void pico_rtos_restore_context(void);

/**
 * @brief Set up the PendSV interrupt priority
 * 
 * PendSV should have the lowest priority to ensure context switches
 * don't interfere with other interrupts.
 */
void pico_rtos_setup_pendsv(void);

/**
 * @brief Task wrapper function that handles task termination
 * 
 * This function is called when a task function returns.
 * It handles cleanup and scheduling of the next task.
 */
void pico_rtos_task_exit_handler(void);

/**
 * @brief Prepare for context switch by setting up global pointers
 */
void pico_rtos_prepare_context_switch(pico_rtos_task_t *current, pico_rtos_task_t *next);

/**
 * @brief Perform the actual context switch
 */
void pico_rtos_perform_context_switch(pico_rtos_task_t *current, pico_rtos_task_t *next);

/**
 * @brief Initialize context switching system
 */
void pico_rtos_context_switch_init(void);

#endif // PICO_RTOS_CONTEXT_SWITCH_H