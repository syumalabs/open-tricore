/*
 * All-cores demo. CPU0 starts every secondary TriCore core (CPU1..CPU5) with a
 * full C runtime and runs the same worker on each. The worker reads its own core
 * id and computes a per-core Fibonacci number recursively, so each of the six
 * cores on the TC4D7 is running our C concurrently. CPU0 checks that all five
 * secondaries returned the right value.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/smp.c bsp/tc4d7/smp_all_demo.c \
 *     -I bsp/tc4d7 -o smp_all_demo.elf
 *   tricore-elf-objcopy -O binary smp_all_demo.elf smp_all_demo.bin
 *   tc-load run smp_all_demo.bin 0x70100000    # heartbeat = cores reporting (5)
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "smp.h"

#define R(a) (*(volatile unsigned int *)(a))

#define OUT       0xB0400000u   /* OUT[id] = result from core id, in shared LMU */
#define SECONDARY 5u            /* CPU1..CPU5 */

static unsigned fib(unsigned n)
{
    return n < 2u ? n : fib(n - 1u) + fib(n - 2u);
}

/* Runs on every secondary core. Each finds its own id and writes a distinct
   recursive result to its slot. */
__attribute__((noreturn)) static void worker(void)
{
    unsigned id = core_id();
    R(OUT + id * 4u) = fib(10u + id);
    for (;;) { }
}

static void lmu_grant(void)
{
    R(0xFB000060u) = 0x300u;
    R(0xFB000070u) = (1u << 31) | (1u << 30);
    R(0xFB000070u) = (1u << 3) | 4u;
    R(0xFB000070u) = (1u << 3) | 1u;
    R(0xFB000070u) = 0u;
    R(0xFB000300u) = 0xFFFFFFFFu; R(0xFB000304u) = 0xFFFFFFFFu;
    R(0xFB000308u) = 0xFFFFFFFFu; R(0xFB00030Cu) = 0xFFFFFFFFu;
    R(0xFB000310u) = 0xFFFFFFFFu; R(0xFB000314u) = 0xFFFFFFFFu;
    R(0xFB000318u) = 0x90400000u; R(0xFB00031Cu) = 0x90480000u;
    R(0xFB000070u) = (1u << 3) | 4u;
}

int main(void)
{
    lmu_grant();
    for (unsigned c = 0; c <= SECONDARY; c++)
        R(OUT + c * 4u) = 0;

    for (unsigned c = 1; c <= SECONDARY; c++)
        core_start_c(c, worker);                 /* light up CPU1..CPU5 */

    unsigned ok = 0;
    for (int t = 0; t < 40000000 && ok < SECONDARY; t++) {
        ok = 0;
        for (unsigned c = 1; c <= SECONDARY; c++)
            if (R(OUT + c * 4u) == fib(10u + c)) ok++;
    }

    R(0x70000004u) = R(OUT + 1u * 4u);           /* CPU1: fib(11) = 89 */
    R(0x7000000Cu) = R(OUT + 5u * 4u);           /* CPU5: fib(15) = 610 */
    for (;;)
        R(0x70000000u) = ok;                     /* heartbeat = secondary cores reporting (want 5) */
}
