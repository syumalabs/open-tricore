/*
 * Minimal GPIO API for the TC4x ports.
 *
 * A port is identified by its module base address (the GPIO_Pxx constants).
 * Pins are 0..15. Direction and pad mode come from the per-pin DRVCFG register,
 * output drive uses the atomic OMR, and the level is read from IN. Register
 * layout verified against the iLLD TC4Dx headers, see docs/hardware-notes.md.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

/* Port module base addresses. */
#define GPIO_P00 0xF003A000u
#define GPIO_P01 0xF003A400u
#define GPIO_P02 0xF003A800u
#define GPIO_P03 0xF003AC00u
#define GPIO_P04 0xF003B000u
#define GPIO_P10 0xF003C800u
#define GPIO_P13 0xF003D400u
#define GPIO_P14 0xF003D800u
#define GPIO_P15 0xF003DC00u
#define GPIO_P16 0xF003E000u

/* Configure a pin as a push-pull digital output, or a digital input. */
void gpio_output(uint32_t port, unsigned pin);
void gpio_input(uint32_t port, unsigned pin);

/* Configure a pin with a raw DRVCFG value, for alternate functions. DIR is bit0,
   OD bit1, MODE bits[7:4] (alternate output select or input mode). Example, a
   UART TX alternate-2 output is (1 << 0) | (2 << 4) = 0x21. */
void gpio_mode(uint32_t port, unsigned pin, uint32_t drvcfg);

/* Drive an output pin. set is high, clear is low, write picks by value, toggle
   flips the current output. These use the atomic OMR and do not disturb siblings. */
void gpio_set(uint32_t port, unsigned pin);
void gpio_clear(uint32_t port, unsigned pin);
void gpio_write(uint32_t port, unsigned pin, int value);
void gpio_toggle(uint32_t port, unsigned pin);

/* Read the level at a pin, 0 or 1. */
int gpio_read(uint32_t port, unsigned pin);

#endif
