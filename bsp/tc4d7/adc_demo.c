/*
 * ADC demo using the BSP clock and ADC APIs. Brings up the PLL and the ADC, then
 * self-tests with the internal monitor channels, which needs no external wiring:
 * VSSM (analog ground) must read about 0 and VDDK1 (a core supply) must read a
 * large value, proving the converter is calibrated and discriminating. It also
 * samples input channel 0 to show a normal conversion.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/clock.c bsp/tc4d7/adc.c \
 *     bsp/tc4d7/adc_demo.c -I bsp/tc4d7 -o adc_demo.elf
 *   tricore-elf-objcopy -O binary adc_demo.elf adc_demo.bin
 *   tc-load run adc_demo.bin 0x70100000     # heartbeat 0xADC0 = self-test passed
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "clock.h"
#include "adc.h"

#define R(a) (*(volatile unsigned int *)(a))

int main(void)
{
    clock_init_pll();
    clock_enable_adc();
    int adc_fail = adc_init();

    unsigned vssm  = adc_read_monitor(ADC_MON_VSSM);   /* ground, expect about 0 */
    unsigned vddk1 = adc_read_monitor(ADC_MON_VDDK1);  /* supply, expect large */
    unsigned ch0   = adc_read_channel(0);              /* input channel 0 */

    int pass = !adc_fail && vssm < 80u && vddk1 > 80u;

    R(0x70000004u) = (vssm << 16) | vddk1;  /* ground in the high half, supply in the low */
    R(0x7000000Cu) = ch0;                   /* input channel 0 reading */
    for (;;) {
        R(0x70000000u) = pass ? 0xADC0u : 0xFA11u;  /* heartbeat, 0xADC0 = passed */
    }
}
