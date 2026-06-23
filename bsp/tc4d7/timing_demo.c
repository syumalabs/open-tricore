/*
 * Timing demo. Publishes a millisecond clock to the heartbeat and paces the loop
 * with a precise 100 ms delay, both from the STM-based timing helper. The
 * heartbeat advancing by ~100 each loop shows tc_millis and tc_delay_ms agree.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/timing.c bsp/tc4d7/timing_demo.c \
 *     -I bsp/tc4d7 -o timing_demo.elf
 *   tricore-elf-objcopy -O binary timing_demo.elf timing_demo.bin
 *   tc-load run timing_demo.bin 0x70100000     # heartbeat = ms since boot
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "timing.h"

#define R(a) (*(volatile unsigned int *)(a))

int main(void)
{
    for (;;) {
        R(0x70000000u) = tc_millis();   /* milliseconds since the timer started */
        tc_delay_ms(100);
    }
}
