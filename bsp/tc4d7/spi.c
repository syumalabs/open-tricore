/*
 * Minimal QSPI master for the TC4x. See spi.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "spi.h"

#define R(a) (*(volatile unsigned int *)(a))

/* Register offsets from a QSPI module base. */
#define CLC        0x000u
#define GLOBALCON  0x110u   /* TQ[7:0], EXPECT[13:10], LB[14], EN[24], MS[26:25], CLKSEL[29], RESETS[31:30] */
#define GLOBALCON1 0x114u   /* TXFM[29:28], RXFM[31:30] FIFO modes */
#define ECON0      0x120u   /* Q[5:0], A[7:6], B[9:8], C[11:10], CPH[12], CPOL[13] */
#define STATUS     0x140u   /* RXFIFOLEVEL[23:20], PHASE[31:28], flags */
#define SSOC       0x148u   /* AOL[15:0], OEN[31:16] slave-select output enables */
#define FLAGSCLEAR 0x150u
#define BACONENTRY 0x15Cu   /* per-frame config: LAST[0], MSB[21], DL[27:23], CS[31:28] */
#define DATAENTRY0 0x160u   /* TX FIFO entry for channel 0 */
#define RXEXIT0    0x180u   /* RX FIFO read for channel 0 */

static void short_delay(void)
{
    for (volatile int i = 0; i < 8000; i++) { }
}

void spi_init(uint32_t q, int loopback)
{
    R(q + CLC) = 0;                                  /* enable the module clock */
    short_delay();

    R(q + GLOBALCON) = (1u << 30);                   /* reset sub-modules */
    short_delay();

    /* Baud timing: ECON with a 50% duty bit (A = B + C) and clock mode 0. */
    R(q + ECON0) = 15u | (1u << 6) | (2u << 8) | (0u << 10);
    R(q + GLOBALCON1) = (1u << 28) | (1u << 30);     /* single-move TX and RX */

    unsigned gc = 50u | (15u << 10) | (1u << 24) | (1u << 29); /* TQ, EXPECT, EN, CLKSEL, master */
    if (loopback)
        gc |= (1u << 14);                            /* internal loopback */
    R(q + GLOBALCON) = gc;

    R(q + SSOC) = (1u << 16);                         /* enable SLSO0, active low */
    short_delay();
    R(q + FLAGSCLEAR) = 0xFFFFFFFFu;
}

uint8_t spi_transfer(uint32_t q, uint8_t tx)
{
    R(q + FLAGSCLEAR) = 0xFFFFFFFFu;
    /* One 8-bit frame, MSB first, channel 0, last word in the frame. */
    R(q + BACONENTRY) = 1u | (1u << 21) | (7u << 23);
    R(q + DATAENTRY0) = tx;

    /* Wait for the received word, bounded so a missing clock cannot hang. */
    for (int t = 0; t < 1000000; t++) {
        if (((R(q + STATUS) >> 20) & 0xFu) != 0)
            break;
    }
    return (uint8_t)(R(q + RXEXIT0) & 0xFFu);
}
