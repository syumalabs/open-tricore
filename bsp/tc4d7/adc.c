/*
 * Minimal TMADC driver for the TC4x. See adc.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "adc.h"

#define R(a) (*(volatile unsigned int *)(a))

/* ADC module and TMADC0 registers. */
#define ADC_CLC         0xF5000000u
#define ADC_CLKEN_TMADC 0xF5000890u   /* TMADCEN[3:0], one bit per module */
#define MODCFG          0xF5020000u   /* SUCAL[0], RUN[1] */
#define MODSTAT         0xF50201D4u   /* RUN[1], CALPH[3:2] (2 = done) */
#define MCH0_CFG        0xF50200D8u   /* SEL[1:0], ATREN[2], EN[3], ST[31:16] */
#define MRES0           0xF50201CCu   /* monitor channel 0 result */
#define CH_STC(n)       (0xF5020018u + (n) * 0xCu)   /* ST[15:0] sampling time */
#define CH_CFG(n)       (0xF502001Cu + (n) * 0xCu)   /* RSEL[19:16], ATREN[20], EN[21] */
#define AW0_RES(n)      (0xF5020240u + (n) * 0x4u)   /* result register n */

#define ST_SAMPLE 0x400u   /* sampling time, generous for accuracy */
#define RES_MASK  0xFFFu   /* 12-bit conversion result */

static void settle(void)
{
    for (volatile int i = 0; i < 200000; i++) { }
}

int adc_init(void)
{
    R(ADC_CLC) = 0;                              /* enable the module clock */
    for (volatile int i = 0; i < 5000; i++) { }
    R(ADC_CLKEN_TMADC) = R(ADC_CLKEN_TMADC) | 0x1u; /* enable TMADC0, enter CONFIG */
    for (volatile int i = 0; i < 5000; i++) { }

    R(MODCFG) = (1u << 0) | (1u << 1);          /* SUCAL | RUN */
    for (int t = 0; t < 8000000; t++) {
        unsigned ms = R(MODSTAT);
        if ((((ms >> 2) & 3u) == 2u) && (ms & 2u)) /* CALPH done and RUN */
            return 0;
    }
    return 1;
}

uint16_t adc_read_channel(unsigned ch)
{
    R(CH_STC(ch)) = ST_SAMPLE;
    R(CH_CFG(ch)) = (1u << 21) | ((ch & 0xFu) << 16) | (1u << 20); /* EN, RSEL=ch, ATREN */
    settle();
    return (uint16_t)(R(AW0_RES(ch)) & RES_MASK);
}

uint16_t adc_read_monitor(unsigned sel)
{
    R(MCH0_CFG) = (sel & 3u) | (1u << 2) | (1u << 3) | (ST_SAMPLE << 16); /* SEL, ATREN, EN, ST */
    settle();
    return (uint16_t)(R(MRES0) & RES_MASK);
}
