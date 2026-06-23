/*
 * TC4x interrupt vector table. On a service request of priority N the CPU jumps
 * to BIV + N*32 (BIV.VSS = 0, 32-byte slots). Only the STM timer-tick priority
 * is populated here, its slot saves the lower context, calls the C handler,
 * restores, and returns from the interrupt. timer_tick_init in irq.c points BIV
 * at __ivt and uses the matching priority.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

    .equ STM_TICK_PRIO, 10

    .section .text.ivt,"ax",@progbits
    .balign 1024                 /* BIV must be aligned to cover the table */
    .global __ivt
__ivt:
    .space STM_TICK_PRIO * 0x20  /* priorities below the tick are unused */

    .global __ivt_stm
__ivt_stm:                       /* slot for STM_TICK_PRIO */
    svlcx                        /* save lower context (caller-saved regs) */
    call stm_isr                 /* call the C handler (call saves upper context to match its ret) */
    rslcx                        /* restore lower context */
    rfe                          /* return from interrupt, restores upper context */
