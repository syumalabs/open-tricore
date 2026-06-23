/*
 * Minimal SDMA (DMA0) memory-to-memory driver for the TC4x. See dma.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "dma.h"

#define R(a) (*(volatile unsigned int *)(a))

/* DMA0 module + resource partition 0 + channel 0. */
#define DMA0_CLC   0xF4410000u
#define DMA0_ERRSR 0xF4410084u
#define RP0_PROT   0xF4411020u   /* IfxApProt: STATE[2:0], SWEN[3], ODEF[30], OWEN[31] */
#define RP0_MODE   0xF4411030u   /* VALID[31] */
#define RP0_ERRSR  0xF4411048u
#define HRR0       0xF4411800u   /* HRP[3:0], HRPV[4] */
#define CH0_SADR   0xF4412008u
#define CH0_DADR   0xF441200Cu
#define CH0_ADICR  0xF4412010u   /* INCS[3], INCD[7] */
#define CH0_CHCFGR 0xF4412014u   /* TREL[13:0], RROAT[19], CHDW[23:21] */
#define CH0_CHCSR  0xF441201Cu   /* SCH[31] trigger, ICH[18] done, TCOUNT[13:0] */

/* LMU0 access-protection: open its SRAM region to all master tags (incl. DMA). */
#define LMU0_MEMCON  0xFB000060u
#define LMU0_PROTRGN 0xFB000070u
#define LMU0_CFG     0xFB000300u   /* shadow ACCENCFG: WRA,WRB,RDA,RDB,VM,PRS,RGNLA,RGNUA */

int dma_init(void)
{
    /* Disable the ECC-on-uninitialized-read fault, then open the shared LMU
       region to the DMA's master tag. Both must happen before any code fills the
       LMU buffers, the region commit does not preserve previously written data.
       The region select is written separately with SWEN=0 so it does not disturb
       the PROT state. */
    R(LMU0_MEMCON) = 0x300u;                     /* ERRDISWE | ERRDIS */
    R(LMU0_PROTRGN) = (1u << 31) | (1u << 30);   /* define owner */
    R(LMU0_PROTRGN) = (1u << 3) | 4u;            /* -> run */
    R(LMU0_PROTRGN) = (1u << 3) | 1u;            /* -> config */
    R(LMU0_PROTRGN) = (0u << 8);                 /* region select 0 */
    R(LMU0_CFG + 0x00) = 0xFFFFFFFFu;            /* WRA */
    R(LMU0_CFG + 0x04) = 0xFFFFFFFFu;            /* WRB */
    R(LMU0_CFG + 0x08) = 0xFFFFFFFFu;            /* RDA */
    R(LMU0_CFG + 0x0C) = 0xFFFFFFFFu;            /* RDB */
    R(LMU0_CFG + 0x10) = 0xFFFFFFFFu;            /* VM  */
    R(LMU0_CFG + 0x14) = 0xFFFFFFFFu;            /* PRS */
    R(LMU0_CFG + 0x18) = 0x90400000u;            /* RGNLA (address & 0xDFFFFFC0) */
    R(LMU0_CFG + 0x1C) = 0x90480000u;            /* RGNUA */
    R(LMU0_PROTRGN) = (1u << 3) | 4u;            /* commit -> run */
    return 0;
}

int dma_copy(uint32_t dst, uint32_t src, uint32_t words)
{
    R(DMA0_CLC) = 0;                           /* enable the DMA module clock */
    for (volatile int i = 0; i < 5000; i++) { }

    /* Unlock resource partition 0 and assign channel 0 to it. */
    R(RP0_PROT) = (1u << 31) | (1u << 30);      /* define owner */
    R(RP0_PROT) = (1u << 3) | 4u;               /* -> run */
    R(RP0_PROT) = (1u << 3) | 1u;               /* -> config */
    R(HRR0) = (1u << 4);                        /* HRPV=1, HRP=0 */
    R(RP0_MODE) = (1u << 31);                   /* VALID, user mode, DMA tag */

    /* Channel 0: copy words 32-bit moves, src++/dst++, whole transaction per request. */
    R(CH0_SADR) = src;
    R(CH0_DADR) = dst;
    R(CH0_ADICR) = (1u << 3) | (1u << 7);       /* INCS, INCD */
    R(CH0_CHCFGR) = (words & 0x3FFFu) | (1u << 19) | (2u << 21); /* TREL, RROAT, 32-bit */

    R(RP0_PROT) = (1u << 3) | 4u;               /* relock -> run */
    R(CH0_CHCSR) = (1u << 31);                  /* SCH: software trigger */

    int done = 0;
    for (int t = 0; t < 4000000; t++) {
        if (R(CH0_CHCSR) & (1u << 18)) { done = 1; break; } /* ICH */
    }
    if (!done) return 1;
    if (R(RP0_ERRSR) | R(DMA0_ERRSR)) return 1;
    return 0;
}
