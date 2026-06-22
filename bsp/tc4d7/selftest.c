/*
 * TC4D7 RAM self-test, built with the open-tricore toolchain.
 *
 * _start is a leaf with no calls, so it needs no stack and no context save
 * area. It proves real on-chip execution in four device-reported ways that do
 * not depend on the LED.
 *
 *   SCRATCH[0]  heartbeat counter, incremented forever      (liveness)
 *   SCRATCH[1]  challenge response, seed*7+3                 (real computation)
 *   SCRATCH[2]  challenge seed, written by the host first    (input)
 *   SCRATCH[3]  done marker 0xC0DECAFE                       (reached the end)
 *
 * It also drives LED1 on as a bonus visual. SCRATCH lives in DSPR0.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#define SCRATCH     ((volatile unsigned int *)0x70000000u)
#define P03_DRVCFG9 (*(volatile unsigned int *)0xF003AF94u)
#define P03_OMR     (*(volatile unsigned int *)0xF003AC3Cu)

void _start(void)
{
    unsigned int seed = SCRATCH[2];
    SCRATCH[1] = seed * 7u + 3u;  /* challenge response */
    SCRATCH[3] = 0xC0DECAFEu;     /* done marker */

    P03_DRVCFG9 = 0x1u;           /* P03.9 push-pull output */
    P03_OMR     = (1u << 25);     /* LED1 on, bonus visual */

    unsigned int c = 0;
    for (;;) {
        SCRATCH[0] = ++c;         /* heartbeat */
    }
}
