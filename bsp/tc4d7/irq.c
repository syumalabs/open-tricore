/*
 * TC4x interrupts and a periodic timer tick.
 *
 * The per-CPU system timer STM is a free-running 64-bit counter (read at ABS).
 * We arm compare register CMP0 against the low 32 bits, route its service
 * request through the SRC node to CPU0 at a chosen priority, point BIV at the
 * vector table in ivt.S, and enable interrupts. The handler reloads CMP0 one
 * interval ahead and bumps g_ticks. Register map verified against the iLLD SFR,
 * see docs/hardware-notes.md.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "irq.h"

#define R(a) (*(volatile unsigned int *)(a))

/* CPU0 system timer (STM) registers. */
#define STM_ABS   0xF8800020u   /* absolute timer value, low 32 bits here */
#define STM_CMP0  0xF8800120u   /* compare register 0 */
#define STM_CMCON 0xF8800128u   /* compare match control */
#define STM_ICR   0xF880012Cu   /* compare interrupt control */
#define STM_ISCR  0xF8800130u   /* compare interrupt set/clear */

/* Service request node the STM CPU0 compare 0 drives, verified on silicon to be
   SR2 of the STMCPU0 group (SR0 at 0x...20, this is +8). */
#define SRC_STM0  0xF4432028u

/* TriCore core special-function registers (mtcr ids). */
#define CSFR_BIV  0xFE20u       /* base interrupt vector */
#define CSFR_ISP  0xFE28u       /* interrupt stack pointer */

#define STM_TICK_PRIO 10u       /* must match ivt.S */

volatile unsigned int g_ticks;
static unsigned int g_interval;

/* Dedicated interrupt stack, the CPU switches to ISP on the first interrupt. */
static unsigned int istack[512] __attribute__((aligned(8)));

extern unsigned int __ivt[];    /* vector table base, from ivt.S */

static inline void mtcr(unsigned int id, unsigned int v)
{
    __asm__ volatile("mtcr %0,%1\n\tisync" :: "i"(id), "d"(v) : "memory");
}

/* Called from the vector slot in ivt.S, which has saved the lower context. */
void stm_isr(void)
{
    R(STM_CMP0) = R(STM_CMP0) + g_interval; /* advance the compare past the match first */
    R(STM_ISCR) = 1u;                       /* then clear CMP0IR cleanly (no immediate re-match) */
    g_ticks++;
}

void timer_tick_init(unsigned int interval)
{
    g_interval = interval;

    mtcr(CSFR_ISP, (unsigned int)&istack[512]);  /* top of the interrupt stack */
    mtcr(CSFR_BIV, (unsigned int)__ivt);         /* vector base, VSS = 0 */

    R(STM_CMCON) = 31u;                          /* MSIZE0 = 31, compare low 32 bits */
    R(STM_CMP0)  = R(STM_ABS) + interval;
    R(STM_ISCR)  = 1u;                           /* clear any pending flag */
    R(STM_ICR)   = 1u;                           /* CMP0EN, output to SR0 */
    R(SRC_STM0)  = STM_TICK_PRIO | (1u << 23);   /* SRPN, SRE, TOS = 0 (CPU0) */

    __asm__ volatile("enable");                  /* global interrupt enable */
}
