/*
 * Multicore C-runtime demo. CPU0 starts CPU1 with a full C runtime (its own
 * stack and context save area) and runs a worker on it that answers each request
 * by computing a Fibonacci number recursively. Recursion exercises real function
 * calls and stack frames on the secondary core, which only work once it has a
 * stack and CSA. CPU0 checks every answer against its own fib().
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/smp.c bsp/tc4d7/smp_c_demo.c \
 *     -I bsp/tc4d7 -o smp_c_demo.elf
 *   tricore-elf-objcopy -O binary smp_c_demo.elf smp_c_demo.bin
 *   tc-load run smp_c_demo.bin 0x70100000    # heartbeat = rounds passed (16)
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "smp.h"

#define R(a) (*(volatile unsigned int *)(a))

#define REQ      0xB0400000u
#define REQ_SEQ  0xB0400004u
#define RESP     0xB0400008u
#define RESP_SEQ 0xB040000Cu
#define ROUNDS   16u

/* Recursive, so it needs a stack and CSA. Run on both cores. */
static unsigned fib(unsigned n)
{
    return n < 2u ? n : fib(n - 1u) + fib(n - 2u);
}

/* Runs on CPU1 as full C: per request, recurse and answer. */
__attribute__((noreturn)) static void cpu1_c_worker(void)
{
    unsigned last = 0;
    for (;;) {
        unsigned s = R(REQ_SEQ);
        if (s != last) {
            R(RESP) = fib(R(REQ));
            R(RESP_SEQ) = s;
            last = s;
        }
    }
}

/* Open the shared LMU to all master tags so CPU1's stores are visible. */
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
    R(REQ) = 0; R(REQ_SEQ) = 0; R(RESP) = 0; R(RESP_SEQ) = 0;

    core_start_c(1, cpu1_c_worker);

    unsigned ok = 0;
    for (unsigned i = 1; i <= ROUNDS; i++) {
        R(REQ) = i;
        R(REQ_SEQ) = i;
        int got = 0;
        for (int t = 0; t < 8000000; t++)
            if (R(RESP_SEQ) == i) { got = 1; break; }
        if (got && R(RESP) == fib(i)) ok++;     /* CPU1's fib vs CPU0's fib */
    }

    R(0x70000004u) = R(RESP);                    /* last answer (fib(16) = 987) */
    R(0x7000000Cu) = R(RESP_SEQ);                /* last sequence (want 16) */
    for (;;)
        R(0x70000000u) = ok;                     /* heartbeat = rounds passed (want 16) */
}
