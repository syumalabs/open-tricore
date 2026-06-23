/*
 * Minimal PWM driver for the TC4x, built on the eGTM TOM (Timer Output Module).
 *
 * Brings up eGTM cluster 0 and generates an edge-aligned PWM on a TOM channel.
 * The eGTM runs on the PLL clock, so bring that up first: clock_init_pll() then
 * clock_enable_egtm(). Period and duty are in ticks of the TOM channel clock
 * (FXCLK0, derived from the eGTM clock), 16-bit each, with duty <= period. The
 * output is high for the first duty ticks of each period.
 *
 * Register layout verified against the iLLD TC4Dx headers and the TC4xx user
 * manual (eGTM chapter), exercised on real silicon: the generated duty was
 * confirmed by sampling the live TOM output (50 percent and 25 percent both
 * tracked the configured value).
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef PWM_H
#define PWM_H

#include <stdint.h>

/* Bring up eGTM cluster 0 for PWM, the module clock, the CMU and its fixed clock,
   and the cluster. Returns 0. Call clock_init_pll() and clock_enable_egtm() first. */
int pwm_init(void);

/* Configure TOM channel ch (0..7) as an edge-aligned PWM with the given period
   and duty in TOM-clock ticks (duty <= period), and start it. Output is high for
   the first duty ticks of each period. */
void pwm_set(unsigned ch, uint16_t period, uint16_t duty);

/* Update the duty of a running channel. The change is glitch-free and takes
   effect at the start of the next period. */
void pwm_set_duty(unsigned ch, uint16_t duty);

#endif
