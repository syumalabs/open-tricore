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

/* ---- Full C runtime for a secondary core ---------------------------------- */

extern unsigned int _SMALL_DATA_[], _SMALL_DATA2_[];

/* Entry per core, set by core_start_c before the core is released, read by the
   trampoline on the target core. Lives in CPU0 data, which secondary cores read
   coherently. */
static void (*volatile g_core_entry[6])(void);

/* Stack and CSA both live in the started core's own local data scratchpad,
   always at 0xD0000000 from that core's view, so every core uses the same
   layout without colliding. Keep well within a small DSPR. */
#define SEC_STACK_TOP 0xD0003F00u
#define SEC_CSA_BEGIN 0xD0000100u
#define SEC_CSA_END   0xD0000D00u   /* 48 context save areas */

/* Trampoline that runs first on the target core. Pure asm so no prologue runs
   before the stack exists: set the stack and small-data bases like crt0.S, then
   jump to the C startup. */
__asm__(
"    .section .text._core_tramp,\"ax\",@progbits\n"
"    .global _core_tramp\n"
"    .type _core_tramp,@function\n"
"_core_tramp:\n"
"    movh.a %a10, 0xd000\n"
"    lea    %a10, [%a10] 0x3f00\n"
"    movh.a %a0, hi:_SMALL_DATA_\n"
"    lea    %a0, [%a0] lo:_SMALL_DATA_\n"
"    movh.a %a1, hi:_SMALL_DATA2_\n"
"    lea    %a1, [%a1] lo:_SMALL_DATA2_\n"
"    dsync\n"
"    isync\n"
"    j      core_cstart\n"
);

extern void _core_tramp(void);

static inline void mtcr_isync(unsigned int id, unsigned int v)
{
    __asm__ volatile ("mtcr %0,%1\n\tisync" :: "i"(id), "d"(v) : "memory");
}

/* Reached by a jump from _core_tramp (stack set, no context saved yet). Build
   the CSA free list inline, exactly like _cstart, then make the first call into
   the user entry. Mirrors the iLLD initCSA algorithm. */
__attribute__((used)) void core_cstart(void)
{
    unsigned int *b = (unsigned int *)SEC_CSA_BEGIN;
    unsigned int *e = (unsigned int *)SEC_CSA_END;
    unsigned int num = ((unsigned int)e - (unsigned int)b) / 64u;
    unsigned int *prv = 0, *nxt = b, lv = 0;
    for (unsigned int k = 0; k < num; k++) {
        lv = (((unsigned int)nxt & (0xFu << 28)) >> 12) |
             (((unsigned int)nxt & (0xFFFFu << 6)) >> 6);
        if (k == 0) mtcr_isync(0xFE38u, lv); else *prv = lv;  /* FCX / link */
        if (k == num - 3) mtcr_isync(0xFE3Cu, lv);            /* LCX */
        prv = nxt; nxt += 16;
    }
    *prv = 0;
    __asm__ volatile ("dsync\n\tisync" ::: "memory");

    unsigned int id;
    __asm__ volatile ("mfcr %0, 0xFE1C" : "=d"(id));
    id &= 0x7u;
    if (g_core_entry[id])
        g_core_entry[id]();   /* first call, saves context to the new CSA list */
    for (;;) { }
}

int core_start_c(unsigned core, void (*entry)(void))
{
    if (core < 1u || core > 5u)
        return -1;
    g_core_entry[core] = entry;
    return core_start(core, _core_tramp);
}
