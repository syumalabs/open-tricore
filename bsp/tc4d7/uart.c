/*
 * Minimal ASCLIN0 UART transmit driver for the TC4D7 Lite Kit console.
 * Console is ASCLIN0, TX on P14.0 (alternate function alt2, push-pull).
 * The boot ROM already clocks fASCLINF, so we select it as the baud clock.
 * Baud is set for roughly 115200 assuming fASCLINF near 100 MHz, the exact
 * rate is calibrated on the host.
 *
 * Register and field values taken from the iLLD TC4Dx headers.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include <stdint.h>

#define ASCLIN0_BASE 0xF46C0000u
#define R(off) (*(volatile uint32_t *)(ASCLIN0_BASE + (off)))
#define A_CLC       R(0x000)
#define A_TXFIFOCON R(0x104)
#define A_BITCON    R(0x10C)
#define A_FRAMECON  R(0x110)
#define A_DATCON    R(0x114)
#define A_BRG       R(0x118)
#define A_CSR       R(0x13C)
#define A_TXDATA    R(0x140)

#define P14_0_DRVCFG (*(volatile uint32_t *)0xF003DB04u)

void uart_init(void)
{
    A_CLC = 0u;            /* clear DISR, enable the module kernel clock */
    (void)A_CLC;
    A_CSR = 0u;            /* baud clock off while we configure */

    A_FRAMECON = 0u;       /* initialise mode first */
    /* baud = fASCLINF / (PRESCALER+1) / (OVERSAMPLING+1) * (NUM/DEN).
       fASCLINF = 100 MHz. Prescaler 54, oversampling 16, NUM/DEN neutral (1/1):
       100e6 / 54 / 16 = 115740, within 0.5 percent of 115200. */
    /* baud = fASCLINF / ((PRESCALER+1) * (OVERSAMPLING+1)) = 100e6 / (54*16) = 115740,
       within 0.5 percent of 115200. fASCLINF is the 100 MHz backup clock. */
    A_BITCON = (53u) | (15u << 16) | (8u << 24); /* PRESCALER 54, oversampling 16, sample 8 */
    A_BRG = (85u << 16) | 86u;                     /* NUM/DEN 85/86 trims measured 116550 to 115200 */
    A_DATCON = 7u;                       /* 8 data bits */
    A_FRAMECON = (1u << 9) | (1u << 16); /* 1 stop bit, ASC mode */
    A_TXFIFOCON = (1u << 0) | (1u << 1) | (1u << 6); /* FLUSH, ENO, inlet width 1 byte */

    A_CSR = 2u;                          /* CLKSEL = fASCLINF, start baud clock */
    while (!(A_CSR & 0x80000000u)) { }   /* wait for clock on */

    P14_0_DRVCFG = 0x21u;               /* P14.0 = ASCLIN0 TXD, alt2, push-pull */
}

void uart_putc(char c)
{
    while (((A_TXFIFOCON >> 16) & 0x1Fu) >= 15u) { } /* wait for TX FIFO space */
    A_TXDATA = (uint8_t)c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}
