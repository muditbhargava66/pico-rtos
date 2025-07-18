#include "pico_rtos/context_switch.h"
#include "pico_rtos/task.h"
#include "pico_rtos.h"
#include "pico/critical_section.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

// Global variables for context switching
uint32_t *current_task_stack_ptr = NULL;
uint32_t *next_task_stack_ptr = NULL;

// ARM Cortex-M0+ specific definitions
#define NVIC_SHPR3_REG          (*(volatile uint32_t *)0xE000ED20)
#define PENDSV_PRIORITY_MASK    0x00FF0000
#define PENDSV_PRIORITY_SHIFT   16
#define LOWEST_INTERRUPT_PRIORITY 0xFF

// Initial PSR value for new tasks (Thumb mode enabled)
#define INITIAL_PSR             0x01000000

uint32_t *pico_rtos_init_task_stack(uint32_t *stack_base, uint32_t stack_size,
                                   pico_rtos_task_function_t task_function, void *param) {
    if (stack_base == NULL || stack_size == 0 || task_function == NULL) {
        return NULL;
    }
    
    // Calculate the top of the stack (stacks grow downward on ARM)
    uint32_t *stack_top = (uint32_t *)((uint8_t *)stack_base + stack_size);
    
    // Initialize stack pointer to top of stack minus space for stack frame
    uint32_t *stack_ptr = stack_top - sizeof(pico_rtos_stack_frame_t) / sizeof(uint32_t);
    
    // Get pointer to the stack frame
    pico_rtos_stack_frame_t *frame = (pico_rtos_stack_frame_t *)stack_ptr;
    
    // Clear the entire stack frame
    memset(frame, 0, sizeof(pico_rtos_stack_frame_t));
    
    // Set up the stack frame for initial task execution
    frame->r0 = (uint32_t)param;           // First parameter to task function
    frame->r1 = 0;                         // Clear other parameter registers
    frame->r2 = 0;
    frame->r3 = 0;
    frame->r12 = 0;                        // Clear r12
    frame->lr = (uint32_t)pico_rtos_task_exit_handler;  // Return address when task exits
    frame->pc = (uint32_t)task_function;   // Program counter - where to start execution
    frame->psr = INITIAL_PSR;              // Program status register (Thumb mode)
    
    // Clear the software-saved registers (r4-r11)
    frame->r4 = 0;
    frame->r5 = 0;
    frame->r6 = 0;
    frame->r7 = 0;
    frame->r8 = 0;
    frame->r9 = 0;
    frame->r10 = 0;
    frame->r11 = 0;
    
    return stack_ptr;
}

void pico_rtos_setup_pendsv(void) {
    // Set PendSV interrupt to lowest priority
    // This ensures context switches don't interfere with other interrupts
    uint32_t reg_value = NVIC_SHPR3_REG;
    reg_value &= ~PENDSV_PRIORITY_MASK;
    reg_value |= (LOWEST_INTERRUPT_PRIORITY << PENDSV_PRIORITY_SHIFT);
    NVIC_SHPR3_REG = reg_value;
}

void pico_rtos_task_exit_handler(void) {
    // This function is called when a task function returns
    // We need to mark the task as terminated and schedule the next task
    
    pico_rtos_enter_critical();
    
    // Get the current task and mark it as terminated
    pico_rtos_task_t *current_task = pico_rtos_get_current_task();
    if (current_task != NULL) {
        current_task->state = PICO_RTOS_TASK_STATE_TERMINATED;
        
        // Free the task's stack memory if it was dynamically allocated
        if (current_task->auto_delete && current_task->stack_base != NULL) {
            pico_rtos_free(current_task->stack_base, current_task->stack_size);
            current_task->stack_base = NULL;
            current_task->stack_ptr = NULL;
        }
    }
    
    pico_rtos_exit_critical();
    
    // Schedule the next task - this should not return
    pico_rtos_schedule_next_task();
    
    // If we somehow get here, enter an infinite loop
    while (1) {
        __wfi(); // Wait for interrupt
    }
}

// Helper function to prepare for context switch
void pico_rtos_prepare_context_switch(pico_rtos_task_t *current, pico_rtos_task_t *next) {
    if (current != NULL) {
        current_task_stack_ptr = current->stack_ptr;
    } else {
        current_task_stack_ptr = NULL;
    }
    
    if (next != NULL) {
        next_task_stack_ptr = next->stack_ptr;
    } else {
        next_task_stack_ptr = NULL;
    }
}

// Function to perform the actual context switch
void pico_rtos_perform_context_switch(pico_rtos_task_t *current, pico_rtos_task_t *next) {
    if (next == NULL) {
        return;
    }
    
    // Prepare the global variables for the assembly context switch routine
    pico_rtos_prepare_context_switch(current, next);
    
    // If this is the first task to run, use the special startup function
    if (current == NULL) {
        pico_rtos_start_first_task();
    } else {
        // Trigger PendSV interrupt for context switch
        pico_rtos_context_switch();
    }
}

// Initialize context switching system
void pico_rtos_context_switch_init(void) {
    // Set up PendSV interrupt priority
    pico_rtos_setup_pendsv();
    
    // Initialize global pointers
    current_task_stack_ptr = NULL;
    next_task_stack_ptr = NULL;
}