/*
 * Minimal CAN (MCMCAN / Bosch M_CAN) driver for the TC4x (CAN0 node 0). See can.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "can.h"

#define R(a) (*(volatile unsigned int *)(a))

/* CAN0: module registers at 0xF4710000, message RAM at 0xF4700000 (= base-0x10000). */
#define CAN_RAM   0xF4700000u
#define CAN_CLC   0xF4710000u   /* DISR[0], DISS[1] */
#define CAN_MCR   0xF4710070u   /* CLKSEL0[1:0], RBUSY[28], RINIT[29], CI[30], CCCE[31] */
#define CAN_TEST  0xF4710210u   /* node 0 test register: LBCK[4] */
#define CAN_CCCR  0xF4710218u   /* INIT[0], CCE[1], CSA[3], CSR[4], MON[5], DAR[6], TEST[7] */
#define CAN_NBTP  0xF471021Cu   /* nominal bit timing */
#define CAN_PSR   0xF4710244u   /* LEC[2:0], ACT[4:3] */
#define CAN_IR    0xF4710250u   /* interrupt raw status */
#define CAN_GFC   0xF4710280u   /* global filter config */
#define CAN_RXF0C 0xF47102A0u   /* RX FIFO 0 config: F0SA[15:2], F0S[22:16] */
#define CAN_RXF0S 0xF47102A4u   /* RX FIFO 0 status: F0FL[6:0], F0GI[13:8] */
#define CAN_RXF0A 0xF47102A8u   /* RX FIFO 0 acknowledge */
#define CAN_RXESC 0xF47102BCu   /* RX element size (0 = 8-byte) */
#define CAN_TXBC  0xF47102C0u   /* TX buffer config: TBSA[15:2], NDTB[21:16] */
#define CAN_TXESC 0xF47102C8u   /* TX element size (0 = 8-byte) */
#define CAN_TXBRP 0xF47102CCu   /* TX buffer request pending */
#define CAN_TXBAR 0xF47102D0u   /* TX buffer add request */

/* Message-RAM layout (byte offsets): RX FIFO 0 = two 16-byte elements at 0x00,
   dedicated TX buffer 0 (16 bytes) at 0x20. */
#define RXF0_OFF  0x00u
#define TXB0_OFF  0x20u
#define ELEM_SZ   16u           /* 8-byte header + 8-byte data */

#define SPIN 2000000

int can_init(int internal_loopback)
{
    unsigned m;

    /* Module clock gate. */
    R(CAN_CLC) = 0u;
    for (int t = 0; t < SPIN; t++) if (!(R(CAN_CLC) & 2u)) break;

    /* Initialise the message-RAM ECC (MCR.RINIT, a 0->1 transition). The CCCE/CI
       change-enable bits do not read back, so carry the whole value in one local
       and never re-read MCR between writes. */
    m = R(CAN_MCR); m |= 0xC0000000u; R(CAN_MCR) = m;        /* CCCE | CI */
    m |= 0x20000000u; R(CAN_MCR) = m; (void)R(CAN_MCR);      /* RINIT = 1 */
    for (int t = 0; t < SPIN; t++) if (!(R(CAN_MCR) & 0x10000000u)) break; /* wait RBUSY */
    m &= ~0x20000000u; R(CAN_MCR) = m;                       /* RINIT = 0 */
    m &= ~0xC0000000u; R(CAN_MCR) = m;                       /* clear CCCE | CI */

    /* Select the node clock source (CLKSEL0 = both), same single-local rule. */
    m = R(CAN_MCR); m |= 0xC0000000u; R(CAN_MCR) = m;
    m = (m & ~0x3u) | 0x3u; R(CAN_MCR) = m;
    m &= ~0xC0000000u; R(CAN_MCR) = m;

    /* Enter configuration: set INIT, then INIT|CCE (CCE only latches once fMCAN is
       actually reaching the node). */
    R(CAN_CCCR) |= 0x1u;
    for (int t = 0; t < SPIN; t++) if (R(CAN_CCCR) & 1u) break;
    { unsigned c = R(CAN_CCCR); c |= 0x3u; R(CAN_CCCR) = c; }

    if (internal_loopback) {
        /* Classic Bosch internal loopback: enable test mode, the loopback bit in
           the TEST register, then bus-monitoring for the self-generated ACK. */
        R(CAN_CCCR) |= 0x80u;       /* TEST = 1 */
        R(CAN_TEST) |= 0x10u;       /* TEST.LBCK = 1 */
        R(CAN_CCCR) |= 0x20u;       /* MON = 1 */
    }

    /* Bit timing (sample point ~80%; exact rate is loopback-internal). */
    R(CAN_NBTP) = 0x06000E03u;

    /* Sections: 8-byte elements, RX FIFO 0 of 2 at offset 0, one TX buffer at 0x20,
       accept all frames into RX FIFO 0. */
    R(CAN_RXESC) = 0u;
    R(CAN_TXESC) = 0u;
    R(CAN_RXF0C) = (2u << 16) | RXF0_OFF;       /* F0S = 2, F0SA = 0 */
    R(CAN_TXBC)  = (1u << 16) | TXB0_OFF;       /* NDTB = 1, TBSA = 0x20 */
    R(CAN_GFC)   = 0u;
    R(CAN_IR)    = 0x1FCFFFFFu;                  /* clear pending interrupt flags */

    /* Leave configuration: clear CCE, then INIT, to start the node. */
    R(CAN_CCCR) &= ~0x2u;
    for (int t = 0; t < SPIN; t++) if (!(R(CAN_CCCR) & 2u)) break;
    R(CAN_CCCR) &= ~0x1u;
    for (int t = 0; t < SPIN; t++) if (!(R(CAN_CCCR) & 1u)) return 0;
    return -1;                                   /* INIT never cleared */
}

int can_send(uint32_t id, const uint8_t *data, unsigned len)
{
    if (len > 8u) len = 8u;

    unsigned w0 = 0u, w1 = 0u;
    for (unsigned i = 0; i < len; i++) {
        unsigned b = data[i];
        if (i < 4u) w0 |= b << (8u * i);
        else        w1 |= b << (8u * (i - 4u));
    }

    /* Build TX buffer 0: T0 = standard id in bits [28:18], T1 = DLC in [19:16]. */
    R(CAN_RAM + TXB0_OFF + 0x0u) = (id & 0x7FFu) << 18;
    R(CAN_RAM + TXB0_OFF + 0x4u) = (len & 0xFu) << 16;
    R(CAN_RAM + TXB0_OFF + 0x8u) = w0;
    R(CAN_RAM + TXB0_OFF + 0xCu) = w1;

    R(CAN_TXBAR) = 1u;                           /* request transmit of buffer 0 */
    for (int t = 0; t < SPIN; t++)
        if (!(R(CAN_TXBRP) & 1u)) return 0;      /* pending cleared = sent */
    return -1;
}

int can_recv(uint32_t *id, uint8_t *data, unsigned *len)
{
    unsigned s = R(CAN_RXF0S);
    if ((s & 0x7Fu) == 0u) return 1;             /* FIFO empty */

    unsigned gi = (s >> 8) & 0x3Fu;              /* get index */
    unsigned e = CAN_RAM + RXF0_OFF + gi * ELEM_SZ;

    unsigned r0 = R(e + 0x0u);
    unsigned r1 = R(e + 0x4u);
    unsigned dlc = (r1 >> 16) & 0xFu;
    if (dlc > 8u) dlc = 8u;

    if (id)  *id = (r0 >> 18) & 0x7FFu;
    if (len) *len = dlc;
    if (data) {
        unsigned w0 = R(e + 0x8u), w1 = R(e + 0xCu);
        for (unsigned i = 0; i < dlc; i++)
            data[i] = (i < 4u) ? (w0 >> (8u * i)) : (w1 >> (8u * (i - 4u)));
    }

    R(CAN_RXF0A) = gi;                           /* free the FIFO element */
    return 0;
}
