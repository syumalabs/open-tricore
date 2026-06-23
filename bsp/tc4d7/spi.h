/*
 * Minimal QSPI master for the TC4x.
 *
 * Configures a QSPI module as an 8-bit, MSB-first, mode-0 master and does
 * blocking full-duplex byte transfers. The QSPI shift engine runs on the PLL
 * clock fQSPI, so bring that up first: clock_init_pll() then
 * clock_qspi_select_pll(1). With loopback enabled, the module ties its own
 * transmit to its receive internally, so a transfer can be verified with no
 * external wiring.
 *
 * Register layout verified against the iLLD TC4Dx headers and the TC4xx user
 * manual (QSPI chapter), all exercised on real silicon over internal loopback.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef SPI_H
#define SPI_H

#include <stdint.h>

/* QSPI module base addresses (modules are spaced 0x200 apart). */
#define SPI_QSPI0 0xF4401000u
#define SPI_QSPI1 0xF4401200u
#define SPI_QSPI2 0xF4401400u
#define SPI_QSPI3 0xF4401600u

/* Initialize a QSPI as an 8-bit mode-0 master on channel 0. If loopback is
   nonzero the module's output is tied internally to its input for self-test.
   Requires the QSPI kernel clock to be running (see clock.h). */
void spi_init(uint32_t qspi, int loopback);

/* Blocking full-duplex exchange of one byte: shifts tx out and returns the byte
   shifted in. In loopback mode the return equals tx. */
uint8_t spi_transfer(uint32_t qspi, uint8_t tx);

#endif
