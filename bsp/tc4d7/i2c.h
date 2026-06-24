/*
 * Minimal I2C master driver for the TC4x.
 *
 * Brings up I2C0 as a 7-bit master on the TC4D7 Lite Kit's onboard I2C bus
 * (SCL P13.1, SDA P13.2, ~100 kHz standard mode), which carries the board's
 * 24AA02E48 EEPROM at address 0x50 with onboard pull-ups, so transfers are
 * verifiable with no external wiring.
 *
 * The I2C module is gated by TWO clocks. The kernel block (CLC1 at the module
 * base) sits behind a wrapper/bridge block whose CLC (at base + 0x10000) is the
 * real module clock gate and resets to OFF, so the wrapper CLC must be opened
 * before the kernel CLC1, otherwise every register access is a bus error. The
 * kernel clock fI2C (which generates SCL) comes from the PLL, so call
 * clock_init_pll() then clock_enable_i2c() before i2c_init(), or the engine makes
 * no clock and transfers hang. Verified on real silicon (EEPROM ACK).
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/* Bring up I2C0 as a master on P13.1 (SCL) / P13.2 (SDA). Call clock_init_pll()
   and clock_enable_i2c() first. Returns 0. */
int i2c_init(void);

/* Master write: START, the 7-bit address with R/W=0, n data bytes, STOP.
   Returns 0 if the device acknowledged, 1 on NACK (no device), -1 on error or
   timeout. data may be NULL when n is 0 (an address-only probe). */
int i2c_write(uint8_t addr7, const uint8_t *data, unsigned n);

/* Probe a 7-bit address (address-only write). Returns 0 if a device ACKs, 1 if
   not, -1 on error. */
int i2c_probe(uint8_t addr7);

#endif
