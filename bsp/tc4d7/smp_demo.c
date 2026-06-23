/*
 * Multicore demo. CPU0 starts CPU1 on a worker loop, then runs a request and
 * response handshake with it through the shared LMU: CPU0 posts a value, CPU1
 * (running concurrently) doubles it and posts the result back, CPU0 checks it.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/smp.c bsp/tc4d7/smp_demo.c \
 *     -I bsp/tc4d7 -o smp_demo.elf
 *   tricore-elf-objcopy -O binary smp_demo.elf smp_demo.bin
 *   tc-load run smp_demo.bin 0x70100000      # heartbeat = handshakes passed (8)
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "smp.h"

#define R(a) (*(volatile unsigned int *)(a))

/* Shared LMU mailbox. CPU0 writes REQ then bumps REQ_SEQ; CPU1 answers in RESP
   and copies the sequence into RESP_SEQ. */
#define REQ      0xB0400000u
#define REQ_SEQ  0xB0400004u
#define RESP     0xB0400008u
#define RESP_SEQ 0xB040000Cu
#define ROUNDS   8u

/* Runs on CPU1. Leaf loop, no stack or CSA needed: wait for a new request
   sequence, double the request, publish the answer. */
__attribute__((noreturn)) static void cpu1_worker(void)
{
    unsigned last = 0;
    for (;;) {
        unsigned s = R(REQ_SEQ);
        if (s != last) {
            R(RESP) = R(REQ) * 2u;
            R(RESP_SEQ) = s;
            last = s;
        }
    }
}

/* Open the shared LMU region to all master tags so CPU1's stores are visible,
   and disable the ECC-on-uninitialized fault. Same recipe as the DMA driver. */
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

    core_start(1, cpu1_worker);

    unsigned ok = 0;
    for (unsigned i = 1; i <= ROUNDS; i++) {
        unsigned val = 0x1000u * i + 7u;
        R(REQ) = val;
        R(REQ_SEQ) = i;                          /* hand the request to CPU1 */
        int got = 0;
        for (int t = 0; t < 2000000; t++)
            if (R(RESP_SEQ) == i) { got = 1; break; }
        if (got && R(RESP) == val * 2u) ok++;
    }

    R(0x70000004u) = R(RESP);                     /* last answer */
    R(0x7000000Cu) = R(RESP_SEQ);                 /* last sequence (want 8) */
    for (;;)
        R(0x70000000u) = ok;                      /* heartbeat = handshakes passed (want 8) */
}
