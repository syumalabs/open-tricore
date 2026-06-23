/*
 * Minimal multicore (SMP) support for the TC4x. See smp.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "smp.h"

#define R(a) (*(volatile unsigned int *)(a))

/* Per-core CSFR blocks start at CPU0 and step by 0x40000. Within a block the
   program counter is at +0x1FE08 and BOOTCON (boot-halt) at +0x1FE60. */
#define CPU0_BASE   0xF8800000u
#define CPU_STRIDE  0x40000u
#define OFF_PC      0x1FE08u
#define OFF_BOOTCON 0x1FE60u

int core_start(unsigned core, void (*entry)(void))
{
    if (core < 1u || core > 5u)
        return -1;

    unsigned base = CPU0_BASE + core * CPU_STRIDE;
    R(base + OFF_PC) = (unsigned)entry;          /* set the start PC */
    if (R(base + OFF_BOOTCON) & 1u)
        R(base + OFF_BOOTCON) = 0u;              /* release boot halt (BHALT) */
    return 0;
}

unsigned core_id(void)
{
    unsigned v;
    __asm__ volatile ("mfcr %0, 0xFE1C" : "=d"(v));   /* CORE_ID */
    return v & 0x7u;
}
