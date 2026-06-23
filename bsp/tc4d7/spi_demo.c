/*
 * QSPI master demo using the BSP clock and SPI APIs. Brings up the peripheral
 * PLL (the QSPI shift engine needs it), enables internal loopback, and exchanges
 * a set of bytes so each one reads back identically with no external wiring.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/clock.c bsp/tc4d7/spi.c \
 *     bsp/tc4d7/spi_demo.c -I bsp/tc4d7 -o spi_demo.elf
 *   tricore-elf-objcopy -O binary spi_demo.elf spi_demo.bin
 *   tc-load run spi_demo.bin 0x70100000     # heartbeat = bytes that round-tripped
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "clock.h"
#include "spi.h"

#define R(a) (*(volatile unsigned int *)(a))

int main(void)
{
    int pll_fail = clock_init_pll();      /* 0 if the PLL locked */
    clock_qspi_select_pll(1);             /* fQSPI = fPLL1 / 1 (must be non-zero) */

    spi_init(SPI_QSPI0, 1);               /* master, internal loopback */

    static const unsigned char tests[] = { 0xA5, 0x3C, 0xFF, 0x01, 0x80, 0x7E };
    const unsigned n = sizeof(tests);
    unsigned pass = 0, last = 0;
    for (unsigned i = 0; i < n; i++) {
        last = spi_transfer(SPI_QSPI0, tests[i]);
        if (last == tests[i])
            pass++;
    }

    R(0x70000004u) = last;                 /* last byte received (0x7E if all good) */
    R(0x7000000Cu) = (unsigned)pll_fail;   /* 0 = PLL locked */
    for (;;) {
        R(0x70000000u) = pass;             /* heartbeat = round-trips passed (6 = all) */
    }
}
