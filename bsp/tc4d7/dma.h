/*
 * Minimal DMA driver for the TC4x System DMA (SDMA, module DMA0).
 *
 * Does blocking memory-to-memory transfers on DMA channel 0. The SDMA is a
 * safety DMA: channel registers live behind a resource-partition access-protection
 * unit, and the DMA presents a master tag that target memories must grant. Two
 * consequences for a bare-metal target with no Infineon startup software:
 *   - dma_init opens the shared LMU region to the DMA's tag and disables the
 *     ECC-on-uninitialized-read fault, so LMU buffers are usable.
 *   - buffers must be in that DMA-accessible shared LMU (DMA_LMU below), NOT in a
 *     CPU-local scratchpad (DSPR), which the DMA cannot write.
 *
 * Register layout and the access-protection model were verified against the iLLD
 * TC4Dx headers and the TC4xx user manual, exercised on real silicon.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef DMA_H
#define DMA_H

#include <stdint.h>

/* Base of the shared-LMU region that dma_init opens for DMA access (512 KB,
   non-cached alias). Place DMA source and destination buffers in here. */
#define DMA_LMU 0xB0400000u

/* Bring up DMA0 and make the shared LMU region DMA-accessible. Returns 0. */
int dma_init(void);

/* Blocking copy of words 32-bit words from src to dst (both DMA-accessible
   addresses, e.g. in DMA_LMU). Returns 0 on success, 1 on error or timeout.
   words must be 1..16383. */
int dma_copy(uint32_t dst, uint32_t src, uint32_t words);

#endif
