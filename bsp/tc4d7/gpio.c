/*
 * GPIO API for the TC4x ports. See gpio.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "gpio.h"

/* Per-port register offsets. */
#define P_OUT(p) (*(volatile uint32_t *)((p) + 0x20u))
#define P_IN(p)  (*(volatile uint32_t *)((p) + 0x24u))
#define P_OMR(p) (*(volatile uint32_t *)((p) + 0x3Cu))
/* Per-pin DRVCFG, inside the PADCFGn block, 16 bytes per pin, DRVCFG at +4. */
#define P_DRVCFG(p, n) (*(volatile uint32_t *)((p) + 0x300u + (n) * 0x10u + 0x4u))

#define DRVCFG_OUTPUT 0x1u   /* DIR = 1, push-pull, MODE = GPIO */
#define DRVCFG_INPUT  0x0u   /* DIR = 0 */

void gpio_output(uint32_t port, unsigned pin) { P_DRVCFG(port, pin) = DRVCFG_OUTPUT; }
void gpio_input(uint32_t port, unsigned pin)  { P_DRVCFG(port, pin) = DRVCFG_INPUT; }
void gpio_mode(uint32_t port, unsigned pin, uint32_t drvcfg) { P_DRVCFG(port, pin) = drvcfg; }

void gpio_set(uint32_t port, unsigned pin)   { P_OMR(port) = 1u << pin; }          /* PS */
void gpio_clear(uint32_t port, unsigned pin) { P_OMR(port) = 1u << (pin + 16); }   /* PCL */

void gpio_write(uint32_t port, unsigned pin, int value)
{
    if (value) gpio_set(port, pin); else gpio_clear(port, pin);
}

void gpio_toggle(uint32_t port, unsigned pin)
{
    if (P_OUT(port) & (1u << pin)) gpio_clear(port, pin); else gpio_set(port, pin);
}

int gpio_read(uint32_t port, unsigned pin) { return (int)((P_IN(port) >> pin) & 1u); }
