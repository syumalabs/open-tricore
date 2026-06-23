/*
 * PWM demo using the BSP clock and PWM APIs. Brings up the PLL and the eGTM, then
 * generates a 50 percent PWM on TOM channel 0 and self-tests it with no external
 * wiring: it statistically samples the live TOM output level and confirms the
 * measured duty matches the configured value, first at 50 percent then after a
 * glitch-free update to 25 percent.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/clock.c bsp/tc4d7/pwm.c \
 *     bsp/tc4d7/pwm_demo.c -I bsp/tc4d7 -o pwm_demo.elf
 *   tricore-elf-objcopy -O binary pwm_demo.elf pwm_demo.bin
 *   tc-load run pwm_demo.bin 0x70100000     # heartbeat 0x600D = duty self-test passed
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "clock.h"
#include "pwm.h"

#define R(a) (*(volatile unsigned int *)(a))
#define CCM_TOM_OUT 0xF90841E8u   /* read-only live TOM output levels, bit n = channel n */

/* Per-mille of samples where TOM channel 0's output is high (measured duty). */
static unsigned measure_duty(void)
{
    unsigned hi = 0;
    const unsigned n = 200000u;
    for (unsigned i = 0; i < n; i++) {
        if (R(CCM_TOM_OUT) & 1u)
            hi++;
    }
    return (unsigned)(((unsigned long long)hi * 1000u) / n);
}

int main(void)
{
    clock_init_pll();
    clock_enable_egtm();
    pwm_init();

    pwm_set(0, 1000, 500);               /* 50 percent duty */
    unsigned d50 = measure_duty();       /* expect ~500 per-mille */

    pwm_set_duty(0, 250);                /* update to 25 percent */
    unsigned d25 = measure_duty();       /* expect ~250 per-mille */

    int pass = (d50 > 450u && d50 < 550u) && (d25 > 200u && d25 < 300u);

    R(0x70000004u) = (d50 << 16) | d25;  /* measured duties in per-mille */
    R(0x7000000Cu) = 0;
    for (;;) {
        R(0x70000000u) = pass ? 0x600Du : 0xFA11u;  /* heartbeat, 0x600D = passed */
    }
}
