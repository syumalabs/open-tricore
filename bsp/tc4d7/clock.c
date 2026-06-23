/*
 * Peripheral PLL bring-up for the TC4x. See clock.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "clock.h"

#define R(a) (*(volatile unsigned int *)(a))

/* SCU clock-control registers. */
#define PROTE      0xF0064020u   /* access-protection for the CCU/PLL registers */
#define OSCCON     0xF0064100u   /* INSEL[25:24] picks the PLL input clock */
#define PERPLLCON0 0xF0064380u   /* PLLPWR[0], NDIV[14:8], PDIV[18:16] */
#define PERPLLCON1 0xF0064384u   /* K dividers */
#define PERPLLSTAT 0xF006438Cu   /* PWRSTAT[0], PLLLOCK[1] */
#define CCUCON     0xF0064400u   /* CLKSELS[1:0], CLKSELP[16] (0 = pll) */
#define CCUSTAT    0xF0064404u   /* LCK[31] busy flag */
#define PERCCUCON0 0xF0064420u   /* QSPIDIV[19:16], CLKSELQSPI[21:20] */

/* PROTE fields: STATE[2:0], SWEN[3], ODEF[30], OWEN[31]; STATE values run=4, config=1. */
static void prot_unlock(void)
{
    R(PROTE) = (1u << 31) | (1u << 30); /* define owner (OWEN, ODEF) */
    R(PROTE) = (1u << 3) | 4u;          /* -> run */
    R(PROTE) = (1u << 3) | 1u;          /* -> config (writes allowed) */
}

static void prot_relock(void)
{
    R(PROTE) = (1u << 3) | 4u;          /* -> run */
}

/* The CCU latches one change at a time; wait for the lock bit to clear before
   the next write. Bounded so a stuck CCU cannot hang startup. */
static void wait_lck(void)
{
    for (int t = 0; t < 2000000; t++) {
        if (!(R(CCUSTAT) & (1u << 31)))
            return;
    }
}

int clock_init_pll(void)
{
    prot_unlock();
    wait_lck();

    /* PLL input = backup clock (INSEL 0), so no external crystal is required. */
    R(OSCCON) = R(OSCCON) & ~(3u << 24);
    wait_lck();

    /* K-divider setup: prescalers /2, KxDIV = 1 (gives a live fPLL1 output). */
    R(PERPLLCON1) = 0x0010A010u;
    wait_lck();
    R(PERPLLCON1) = R(PERPLLCON1) | 0x00010101u;
    wait_lck();

    /* Power up with NDIV = 3, PDIV = 0: VCO = backup x 4, inside the lock range. */
    R(PERPLLCON0) = (3u << 8) | 1u;
    wait_lck();

    int locked = 1;
    for (int t = 0; t < 3000000; t++) {
        if (R(PERPLLSTAT) & 1u) break;            /* PWRSTAT */
    }
    for (int t = 0; t < 8000000; t++) {
        if (R(PERPLLSTAT) & 2u) { locked = 0; break; } /* PLLLOCK */
    }

    /* Route the peripheral clock tree to the PLL (CLKSELP 0), keep CLKSELS. */
    R(CCUCON) = R(CCUCON) & ~(1u << 16);
    wait_lck();

    prot_relock();
    return locked;
}

void clock_qspi_select_pll(unsigned divsel)
{
    /* PERCCUCON0 is access-protected once an owner is defined. The owner is
       already set by clock_init_pll, so just step run -> config for the write and
       back. CLKSELQSPI = 1 (fPLL1), QSPIDIV = divsel. divsel 0 gates fQSPI off. */
    R(PROTE) = (1u << 3) | 1u;          /* run -> config */
    wait_lck();
    R(PERCCUCON0) = (R(PERCCUCON0) & ~(0xFu << 16) & ~(3u << 20))
                    | ((divsel & 0xFu) << 16) | (1u << 20);
    wait_lck();
    R(PROTE) = (1u << 3) | 4u;          /* config -> run */
}
