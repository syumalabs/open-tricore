/*
 * DMA demo using the BSP DMA API. Brings up DMA0, fills a source buffer in the
 * DMA-accessible shared LMU, copies it to a destination buffer with the DMA, and
 * verifies the copy word by word. Memory-to-memory needs no external wiring.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/dma.c bsp/tc4d7/dma_demo.c \
 *     -I bsp/tc4d7 -o dma_demo.elf
 *   tricore-elf-objcopy -O binary dma_demo.elf dma_demo.bin
 *   tc-load run dma_demo.bin 0x70100000     # heartbeat = words copied correctly
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "dma.h"

#define R(a) (*(volatile unsigned int *)(a))
#define N 64u
#define SRC DMA_LMU
#define DST (DMA_LMU + 0x1000u)

int main(void)
{
    dma_init();

    for (unsigned i = 0; i < N; i++) {
        R(SRC + i * 4) = 0xA5000000u | (i * 0x111u) | 0x33u;
        R(DST + i * 4) = 0u;
    }

    int err = dma_copy(DST, SRC, N);

    unsigned matches = 0;
    for (unsigned i = 0; i < N; i++) {
        if (R(DST + i * 4) == R(SRC + i * 4))
            matches++;
    }

    R(0x70000004u) = R(DST);            /* first copied word (want 0xA5000033) */
    R(0x7000000Cu) = (unsigned)err;     /* 0 = DMA reported success */
    for (;;) {
        R(0x70000000u) = matches;       /* heartbeat = words copied correctly (want 64) */
    }
}
