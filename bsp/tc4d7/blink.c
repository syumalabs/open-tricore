/*
 * Minimal TC4D7 RAM blink, built with the open-tricore toolchain.
 *
 * _start is a leaf with no calls, so it needs no stack and no context save
 * area. It configures P03.9 as an output and drives it low, which turns LED1
 * on, then parks in an infinite loop. The loader sets the program counter to
 * _start and runs the core, then we read P03_IN back to prove our compiled
 * code did it.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#define P03_DRVCFG9 (*(volatile unsigned int *)0xF003AF94u) /* DIR bit0, OD bit1 */
#define P03_OMR     (*(volatile unsigned int *)0xF003AC3Cu) /* PCL9 bit25 drives low */

void _start(void)
{
    P03_DRVCFG9 = 0x1u;       /* P03.9 push-pull output */
    P03_OMR     = (1u << 25); /* PCL9, drive P03.9 low, LED1 ON */
    for (;;) {
    }
}
