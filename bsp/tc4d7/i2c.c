/*
 * Minimal I2C master driver for the TC4x (I2C0). See i2c.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "i2c.h"

#define R(a) (*(volatile unsigned int *)(a))

/* I2C0 kernel block at 0xF44C0000, wrapper/bridge block at 0xF44D0000. */
#define I2C_CLC1     0xF44C0000u   /* kernel: RMC[15:8], DISR[0], DISS[1] */
#define I2C_RUNCTRL  0xF44C0010u   /* RUN[0] */
#define I2C_ENDDCTRL 0xF44C0014u   /* SETEND[1] */
#define I2C_FDIVCFG  0xF44C0018u   /* INC[?], DEC[?] baud divider */
#define I2C_FDIVHIGH 0xF44C001Cu   /* 0 selects standard/fast mode */
#define I2C_ADDRCFG  0xF44C0020u   /* MNS[19], SONA[20], SOPE[21] */
#define I2C_BUSSTAT  0xF44C0024u   /* BS[1:0] */
#define I2C_FIFOCFG  0xF44C0028u   /* RXFC[16], TXFC[17] */
#define I2C_TPSCTRL  0xF44C0034u   /* TPS[13:0] transmit packet size */
#define I2C_TIMCFG   0xF44C0040u
#define I2C_ERRIRQSC 0xF44C0068u   /* error status clear (FIFO over/underflow) */
#define I2C_PIRQSS   0xF44C0074u   /* NACK[4], TX_END[5] */
#define I2C_PIRQSC   0xF44C0078u   /* protocol status clear */
#define I2C_RIS      0xF44C0080u   /* raw interrupt status (FIFO requests low nibble) */
#define I2C_ICR      0xF44C008Cu   /* interrupt clear */
#define I2C_TXD      0xF44C8000u   /* transmit FIFO */
#define I2C_WCLC     0xF44D0000u   /* wrapper module clock gate: DISR[0], DISS[1] */
#define I2C_GPCTL    0xF44D0060u   /* PISEL[2:0] input select */

/* Wait (best effort) for a FIFO data request, then return regardless so the byte
   is written: the 8-stage FIFO buffers it and the engine drains it as it shifts.
   Aborting here when no request bit shows wedges multi-byte transfers, since the
   request for a later byte does not always appear in RIS. */
static void fifo_ready(void)
{
    for (int t = 0; t < 500000; t++)
        if (R(I2C_RIS) & 0xFu) return;
}

int i2c_init(void)
{
    /* Pins: P13.1 SCL, P13.2 SDA = alternate-6 output, open-drain (DIR|OD|MODE6). */
    R(0xF003D714u) = 0x63u;
    R(0xF003D724u) = 0x63u;

    /* Open the wrapper clock gate FIRST, then route the inputs, then the kernel. */
    R(I2C_WCLC) = 0u;
    for (int t = 0; t < 400000; t++) if (!(R(I2C_WCLC) & 2u)) break;   /* DISS clear */
    R(I2C_GPCTL) = 1u;                                                 /* PISEL = P13 inputs */
    R(I2C_CLC1) = 0x100u;
    for (int t = 0; t < 400000; t++) if (!(R(I2C_CLC1) & 2u)) break;

    /* Master config (must be done with RUN=0). */
    R(I2C_RUNCTRL) = 0u;
    R(I2C_ADDRCFG) = (1u << 19) | (1u << 20) | (1u << 21); /* master, stop-on-NACK, stop-on-packet-end */
    R(I2C_FIFOCFG) = (1u << 16) | (1u << 17);              /* RX/TX FIFO flow control */
    R(I2C_FDIVHIGH) = 0u;
    R(I2C_FDIVCFG) = 0x0001003Du;                          /* INC=1, DEC=61 -> ~100 kHz */
    R(I2C_TIMCFG) = 0x2000C03Fu;
    R(I2C_RUNCTRL) = 1u;                                   /* run, enter listening */
    return 0;
}

int i2c_write(uint8_t addr7, const uint8_t *data, unsigned n)
{
    R(I2C_PIRQSC) = 0x7Fu;                 /* clear stale protocol status */
    R(I2C_ERRIRQSC) = 0xFu;                /* clear stale error status */

    R(I2C_TPSCTRL) = (n + 1u) & 0x3FFFu;   /* address byte + n data bytes; arms the transfer */

    fifo_ready();
    R(I2C_TXD) = (unsigned)addr7 << 1;     /* address with R/W = 0 (write); first write emits START */
    R(I2C_ICR) = 0xFu;

    for (unsigned i = 0; i < n; i++) {
        fifo_ready();
        R(I2C_TXD) = data[i];
        R(I2C_ICR) = 0xFu;
    }

    /* SONA/SOPE make the hardware auto-STOP, so a no-slave NACK cannot hang. The
       transmission-end (or NACK) flag bounds the wait. */
    for (int t = 0; t < 6000000; t++) {
        unsigned p = R(I2C_PIRQSS);
        if (p & ((1u << 4) | (1u << 5))) {          /* NACK or TX_END */
            R(I2C_PIRQSC) = (1u << 4) | (1u << 5);
            return (p & (1u << 4)) ? 1 : 0;         /* NACK -> 1, clean end -> 0 */
        }
    }
    R(I2C_ENDDCTRL) = (1u << 1);           /* force STOP on timeout */
    R(I2C_PIRQSC) = 0x7Fu;
    return -1;
}

int i2c_probe(uint8_t addr7)
{
    return i2c_write(addr7, 0, 0);
}
