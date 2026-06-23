/*
 * Minimal ADC (TMADC) driver for the TC4x.
 *
 * Brings up time-multiplexed ADC module 0 (12-bit SAR) and does blocking
 * conversions. The converter runs on the PLL clock fADC and requires a start-up
 * calibration pass before it produces valid results, both of which adc_init
 * handles. Bring the clock up first: clock_init_pll() then clock_enable_adc().
 *
 * Conversions return a 12-bit code, 0 at analog ground (VAGND) up to 0xFFF at the
 * analog reference (VAREF). adc_read_monitor samples on-chip signals (supplies,
 * ground), which is how the driver self-tests with no external wiring: VSSM reads
 * about 0 and a supply reads a large value.
 *
 * Register layout verified against the iLLD TC4Dx headers and the TC4xx user
 * manual (ADC chapter), exercised on real silicon.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef ADC_H
#define ADC_H

#include <stdint.h>

/* Internal monitor-channel signals for adc_read_monitor (TMADC0 MCH0). */
#define ADC_MON_VDDK1 0u   /* core supply voltage, reads non-zero */
#define ADC_MON_VSSM  2u   /* analog ground, reads about 0 */

/* Bring up TMADC0: enable the module, run the mandatory start-up calibration,
   and enter RUN state. Returns 0 once calibrated and running, 1 on timeout.
   Call clock_init_pll() and clock_enable_adc() first. */
int adc_init(void);

/* Convert an input channel (0..15) and return the 12-bit result. The channel's
   result goes to result register ch. */
uint16_t adc_read_channel(unsigned ch);

/* Convert an internal monitor signal (ADC_MON_*) and return the 12-bit result. */
uint16_t adc_read_monitor(unsigned sel);

#endif
