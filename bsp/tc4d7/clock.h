/*
 * Clock-tree bring-up for the TC4x.
 *
 * After a bare reset (no Infineon SSW) the system runs on the always-on backup
 * clock and both PLLs and the external oscillator are off. Peripherals that take
 * their functional clock from the backup clock (STM, ports, ASCLIN) work as-is,
 * but anything whose kernel clock comes from a PLL does not. The QSPI is the
 * notable case: its shift engine runs on fQSPI = fPER from the peripheral PLL,
 * so without a PLL it never shifts.
 *
 * clock_init_pll brings up the peripheral PLL from the backup clock (no external
 * crystal needed) and routes the peripheral clock tree to it. The CCU/PLL
 * registers are protected by the access-protection unit (PROTE), which this
 * unlocks and re-locks around the writes. Register layout verified against the
 * iLLD TC4Dx headers and the TC4xx user manual (CLOCK chapter).
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef CLOCK_H
#define CLOCK_H

/* Bring up the peripheral PLL from the backup clock and select it as the
   peripheral clock source. Returns 0 if the PLL locked, 1 on timeout. Safe to
   call once at startup before configuring PLL-clocked peripherals. */
int clock_init_pll(void);

/* Enable the ADC kernel clock fADC from the peripheral PLL (PERCCUCON1.ADCPERON).
   Without this fADC is off and the ADC shift logic never runs. Call after
   clock_init_pll(). */
void clock_enable_adc(void);

/* Enable the eGTM kernel clock fGTM from the peripheral PLL (SYSCCUCON1.EGTMDIV).
   Without it the eGTM timers do not run. Call after clock_init_pll(). */
void clock_enable_egtm(void);

/* Select the peripheral PLL as the QSPI kernel clock (fQSPI) with divider divsel.
   divsel is the PERCCUCON0.QSPIDIV field: 1 divides by 1, up to 15. IMPORTANT:
   divsel 0 switches fQSPI OFF entirely, so always pass at least 1. Call after
   clock_init_pll(). */
void clock_qspi_select_pll(unsigned divsel);

#endif
