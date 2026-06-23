/*
 * ppu, host-side helper for running code on the AURIX TC4x PPU scalar core
 * (Synopsys ARC EV71) and exchanging data with it. Built on the shared tcmcd
 * MCD layer, the caller connects with open_target() first.
 *
 * The PPU was brought up entirely by clean-room reverse engineering, see
 * docs/ppu-reverse-engineering.md for the boot recipe and the data channels.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef PPU_H
#define PPU_H

#include <stdint.h>

/* The core fetches from here, non-cached LMU. ARC images must be linked at this
   address (see ppu/). */
#define PPU_CODE_BASE 0xB0000000u

/* Load an ARC image into PPU code memory and start the core at it.
   Returns 0 on success, -1 on a load failure. */
int ppu_load_run(const uint8_t *image, uint32_t len);

/* PPU run state, 0 running, 1 sleeping, 2 halted. */
int ppu_runstate(void);

/* Run an ARC image as a call, write nin input words into LMU where the core
   reads them, start the core, then stream nout result words back over the
   halt-signaling channel into outputs. inputs or outputs may be NULL when the
   corresponding count is zero. The caller must have opened the target first.
   Returns 0 on success, -1 if the core failed to resume mid-stream. */
int ppu_call(const uint8_t *image, uint32_t len,
             const uint32_t *inputs, uint32_t nin,
             uint32_t *outputs, uint32_t nout);

#endif
