/*
 * Timer-tick demo. The STM compare interrupt increments g_ticks on its own;
 * main just publishes the count to the selftest heartbeat so it can be read back
 * over the debugger, proving a periodic interrupt is running.
 *
 * Build and run (the interrupt vector table and handler come from ivt.S/irq.c):
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/ivt.S bsp/tc4d7/irq.c \
 *     bsp/tc4d7/timer_demo.c -I bsp/tc4d7 -o timer_demo.elf
 *   tricore-elf-objcopy -O binary timer_demo.elf timer_demo.bin
 *   tc-load run timer_demo.bin 0x70100000      # heartbeat moving => ticks advancing
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "irq.h"

#define HEARTBEAT (*(volatile unsigned int *)0x70000000u)

int main(void)
{
    timer_tick_init(100000);          /* periodic STM tick */
    for (;;) {
        HEARTBEAT = g_ticks;          /* only the timer interrupt changes g_ticks */
    }
}
