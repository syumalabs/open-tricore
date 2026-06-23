/*
 * GPIO demo using the BSP GPIO API. Blinks LED1 on P03.9 (active low) and reads
 * the pin back so the level is visible over the debugger as well as on the board.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/gpio.c bsp/tc4d7/gpio_demo.c \
 *     -I bsp/tc4d7 -o gpio_demo.elf
 *   tricore-elf-objcopy -O binary gpio_demo.elf gpio_demo.bin
 *   tc-load run gpio_demo.bin 0x70100000     # heartbeat counts blinks
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "gpio.h"

#define LED_PORT GPIO_P03
#define LED_PIN  9
#define R(a) (*(volatile unsigned int *)(a))

int main(void)
{
    gpio_output(LED_PORT, LED_PIN);
    unsigned int blinks = 0;
    for (;;) {
        gpio_toggle(LED_PORT, LED_PIN);
        R(0x70000000u) = ++blinks;                       /* blink count */
        R(0x70000004u) = (unsigned)gpio_read(LED_PORT, LED_PIN); /* current level */
        for (volatile int d = 0; d < 2000000; d++) { }   /* crude delay */
    }
}
