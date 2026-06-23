/*
 * Interrupts and a periodic timer tick for the TC4x BSP.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef IRQ_H
#define IRQ_H

/* Incremented once per timer tick by the STM compare interrupt. */
extern volatile unsigned int g_ticks;

/* Set up the interrupt vector base and an STM compare interrupt that fires
   every `interval` STM cycles, then enable interrupts. After this returns,
   g_ticks advances on its own. Call once, after the C runtime is up. */
void timer_tick_init(unsigned int interval);

#endif
