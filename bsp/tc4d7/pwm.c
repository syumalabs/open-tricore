/*
 * Minimal eGTM TOM PWM driver for the TC4x. See pwm.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "pwm.h"

#define R(a) (*(volatile unsigned int *)(a))

/* eGTM cluster 0 registers. */
#define EGTM_CLC        0xF90E0000u
#define CMU_CLK_EN      0xF9080080u   /* EN_CLK0[1:0], EN_FXCLK[23:22] */
#define CMU_FXCLK_CTRL  0xF90800C4u   /* FXCLK_SEL[3:0] */
#define CCM_CMU_CLK_CFG 0xF90841F0u   /* cluster clock routing */
#define CCM_CFG         0xF90841F8u   /* EN_TIM/EN_TOM submodule enables */
#define CCM_PROT        0xF90841FCu   /* CLS_PROT, resets locked */

/* TOM channel block: 0x40 apart, channel 0 at 0xF9081000. */
#define TOM_CH(n)       (0xF9081000u + (n) * 0x40u)
#define TOM_CTRL(n)     (TOM_CH(n) + 0x00u)   /* CLK_SRC[15:12], SL[11] */
#define TOM_SR0(n)      (TOM_CH(n) + 0x04u)   /* shadow period */
#define TOM_SR1(n)      (TOM_CH(n) + 0x08u)   /* shadow duty */
#define TOM_CM0(n)      (TOM_CH(n) + 0x0Cu)   /* period */
#define TOM_CM1(n)      (TOM_CH(n) + 0x10u)   /* duty/compare */
#define TOM_CN0(n)      (TOM_CH(n) + 0x14u)   /* counter */
#define TOM_IRQ_EN(n)   (TOM_CH(n) + 0x20u)
#define TOM_CTRL_SR(n)  (TOM_CH(n) + 0x30u)   /* shadow of CTRL */

/* TGC0 governs TOM channels 0..7. */
#define TGC0_GLB_CTRL   0xF9081430u   /* HOST_TRIG[0], UPEN_CTRL at bit 16 + 2*ch */
#define TGC0_ENDIS_CTRL 0xF9081470u   /* 2 bits per channel */
#define TGC0_OUTEN_CTRL 0xF9081478u   /* 2 bits per channel */

#define FEAT_ENABLE 0x2u   /* 10b in a 2-bit feature field */

int pwm_init(void)
{
    R(EGTM_CLC) = 0;                              /* module clock */
    for (volatile int i = 0; i < 10000; i++) { }

    R(CMU_FXCLK_CTRL) = 0;                        /* FXCLK input select 0 */
    R(CMU_CLK_EN) = 0x2u | (2u << 22);           /* EN_CLK0 + EN_FXCLK (the TOM clock) */

    R(CCM_PROT) = 0;                             /* unlock the cluster config */
    R(CCM_CMU_CLK_CFG) = 0;                      /* default cluster clock routing */
    R(CCM_CFG) = 0x7u;                          /* enable TIM, TOM, ATOM submodules */
    for (volatile int i = 0; i < 10000; i++) { }
    return 0;
}

void pwm_set(unsigned ch, uint16_t period, uint16_t duty)
{
    unsigned ctrl = (0u << 12) | (1u << 11);     /* CLK_SRC = FXCLK0, SL = high-active */
    R(TOM_CTRL(ch))    = ctrl;
    R(TOM_CTRL_SR(ch)) = ctrl;
    R(TOM_CN0(ch))     = 0;
    R(TOM_IRQ_EN(ch))  = 0;
    R(TOM_CM0(ch)) = period;  R(TOM_SR0(ch)) = period;
    R(TOM_CM1(ch)) = duty;    R(TOM_SR1(ch)) = duty;

    /* Enable update, arm channel + output, then trigger to latch and start. */
    R(TGC0_GLB_CTRL)   = (R(TGC0_GLB_CTRL) & ~(0x3u << (16 + 2 * ch))) | (FEAT_ENABLE << (16 + 2 * ch));
    R(TGC0_ENDIS_CTRL) = (R(TGC0_ENDIS_CTRL) & ~(0x3u << (2 * ch))) | (FEAT_ENABLE << (2 * ch));
    R(TGC0_OUTEN_CTRL) = (R(TGC0_OUTEN_CTRL) & ~(0x3u << (2 * ch))) | (FEAT_ENABLE << (2 * ch));
    R(TGC0_GLB_CTRL)   = R(TGC0_GLB_CTRL) | 1u;  /* HOST_TRIG */
}

void pwm_set_duty(unsigned ch, uint16_t duty)
{
    R(TOM_SR1(ch)) = duty;                        /* shadow; reloaded on update */
    R(TGC0_GLB_CTRL) = R(TGC0_GLB_CTRL) | 1u;     /* HOST_TRIG -> takes effect next period */
}
