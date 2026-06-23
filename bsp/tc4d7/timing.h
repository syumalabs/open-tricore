/*
 * Busy-wait delays and a monotonic time source for the TC4x, built on the
 * free-running CPU system timer (STM). No interrupts needed.
 *
 * The STM on the TC4D7 runs at about 50.3 MHz (the ~100.7 MHz backup clock
 * divided by two), measured against a host wall clock. Override STM_HZ if your
 * clock setup differs. tc_delay_ticks is exact regardless of the frequency; the
 * microsecond and millisecond helpers use STM_HZ to convert.
 *
 * The 32-bit tick counter wraps about every 85 seconds, so tc_micros and
 * tc_millis wrap too. Delays handle the wrap correctly via unsigned arithmetic.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>

#ifndef STM_HZ
#define STM_HZ 50300000u   /* measured STM frequency, ~50.3 MHz */
#endif

/* Free-running STM counter, low 32 bits, increments at STM_HZ. */
uint32_t tc_ticks(void);

/* Busy-wait. tc_delay_ticks counts raw STM cycles and is exact. */
void tc_delay_ticks(uint32_t ticks);
void tc_delay_us(uint32_t us);
void tc_delay_ms(uint32_t ms);

/* Time since the counter started, microseconds and milliseconds. */
uint32_t tc_micros(void);
uint32_t tc_millis(void);

#endif
