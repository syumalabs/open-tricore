/*
 * STM-based delays and time source for the TC4x. See timing.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "timing.h"

#define STM_ABS 0xF8800020u   /* CPU0 system timer, low 32 bits of the count */

uint32_t tc_ticks(void)
{
    return *(volatile uint32_t *)STM_ABS;
}

void tc_delay_ticks(uint32_t ticks)
{
    uint32_t start = tc_ticks();
    while ((tc_ticks() - start) < ticks) {
        /* unsigned subtraction handles the 32-bit wrap */
    }
}

void tc_delay_us(uint32_t us)
{
    tc_delay_ticks((uint32_t)((uint64_t)us * STM_HZ / 1000000u));
}

void tc_delay_ms(uint32_t ms)
{
    while (ms--) {
        tc_delay_ticks(STM_HZ / 1000u);
    }
}

uint32_t tc_micros(void)
{
    return (uint32_t)((uint64_t)tc_ticks() * 1000000u / STM_HZ);
}

uint32_t tc_millis(void)
{
    return (uint32_t)((uint64_t)tc_ticks() * 1000u / STM_HZ);
}
