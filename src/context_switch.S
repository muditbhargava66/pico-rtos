.syntax unified
.cpu cortex-m0plus
.thumb

.section .text

// Context switch assembly functions for ARM Cortex-M0+
// These functions handle saving and restoring CPU state during task switches

.global pico_rtos_context_switch
.global pico_rtos_start_first_task
.global pico_rtos_save_context
.global pico_rtos_restore_context

// External symbols
.extern current_task_stack_ptr
.extern next_task_stack_ptr

// ARM Cortex-M0+ register definitions
.equ NVIC_INT_CTRL,     0xE000ED04  // Interrupt control state register
.equ NVIC_PENDSVSET,    0x10000000  // Value to trigger PendSV exception

//-----------------------------------------------------------------------------
// pico_rtos_context_switch
// Triggers a context switch by setting PendSV interrupt
//-----------------------------------------------------------------------------
.type pico_rtos_context_switch, %function
pico_rtos_context_switch:
    // Load the address of the interrupt control register
    ldr r0, =NVIC_INT_CTRL
    ldr r1, =NVIC_PENDSVSET
    str r1, [r0]
    
    // Data synchronization barrier
    dsb
    isb
    
    bx lr
.size pico_rtos_context_switch, .-pico_rtos_context_switch

//-----------------------------------------------------------------------------
// pico_rtos_start_first_task
// Starts the first task by restoring its context
//-----------------------------------------------------------------------------
.type pico_rtos_start_first_task, %function
pico_rtos_start_first_task:
    // Disable interrupts
    cpsid i
    
    // Load the stack pointer of the first task
    ldr r0, =current_task_stack_ptr
    ldr r1, [r0]
    ldr r2, [r1]
    mov sp, r2
    
    // Pop registers r4-r11 from stack
    pop {r4-r7}
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    pop {r4-r7}
    
    // Pop remaining registers and return address
    pop {r0-r3}
    pop {r4-r6}  // r4=r12, r5=lr, r6=pc
    mov r12, r4
    
    // Skip PSR (it will be restored by hardware)
    add sp, sp, #4
    
    // Enable interrupts
    cpsie i
    
    // Jump to task (PC is in r6)
    bx r6
.size pico_rtos_start_first_task, .-pico_rtos_start_first_task

//-----------------------------------------------------------------------------
// PendSV_Handler
// The actual context switch handler - called when PendSV interrupt occurs
//-----------------------------------------------------------------------------
.type PendSV_Handler, %function
.global PendSV_Handler
PendSV_Handler:
    // Disable interrupts
    cpsid i
    
    // Check if we have a current task to save
    ldr r0, =current_task_stack_ptr
    ldr r1, [r0]
    cmp r1, #0
    beq restore_context
    
save_context:
    // Save r4-r7 to stack
    push {r4-r7}
    
    // Save r8-r11 to stack (need to move to low registers first)
    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    push {r4-r7}
    
    // Save current stack pointer to task control block
    ldr r0, =current_task_stack_ptr
    ldr r1, [r0]
    mov r2, sp
    str r2, [r1]

restore_context:
    // Load next task's stack pointer
    ldr r0, =next_task_stack_ptr
    ldr r1, [r0]
    ldr r2, [r1]
    mov sp, r2
    
    // Update current task pointer
    ldr r0, =current_task_stack_ptr
    str r1, [r0]
    
    // Restore r8-r11 from stack
    pop {r4-r7}
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    
    // Restore r4-r7 from stack
    pop {r4-r7}
    
    // Enable interrupts
    cpsie i
    
    // Return from exception (hardware will restore r0-r3, r12, lr, pc, psr)
    bx lr
.size PendSV_Handler, .-PendSV_Handler

//-----------------------------------------------------------------------------
// pico_rtos_save_context
// C-callable function to save current context
//-----------------------------------------------------------------------------
.type pico_rtos_save_context, %function
pico_rtos_save_context:
    // This is called from C, so we need to save the context manually
    // Save all registers to stack
    push {r0-r7}
    mov r0, r8
    mov r1, r9
    mov r2, r10
    mov r3, r11
    push {r0-r3}
    push {lr}
    
    // Save stack pointer to current task
    ldr r0, =current_task_stack_ptr
    ldr r1, [r0]
    mov r2, sp
    str r2, [r1]
    
    bx lr
.size pico_rtos_save_context, .-pico_rtos_save_context

//-----------------------------------------------------------------------------
// pico_rtos_restore_context  
// C-callable function to restore context
//-----------------------------------------------------------------------------
.type pico_rtos_restore_context, %function
pico_rtos_restore_context:
    // Load stack pointer from next task
    ldr r0, =next_task_stack_ptr
    ldr r1, [r0]
    ldr r2, [r1]
    mov sp, r2
    
    // Update current task pointer
    ldr r0, =current_task_stack_ptr
    str r1, [r0]
    
    // Restore all registers from stack
    pop {r0}
    mov lr, r0
    pop {r0-r3}
    mov r8, r0
    mov r9, r1
    mov r10, r2
    mov r11, r3
    pop {r0-r7}
    
    bx lr
.size pico_rtos_restore_context, .-pico_rtos_restore_context

.end