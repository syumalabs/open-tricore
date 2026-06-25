/*
 * On-chip data flash (DFLASH) driver for the TC4x.
 *
 * Provides persistent storage on the AURIX's own data flash via the DMU command
 * interface - no external memory needed. Erase is per sector, programming is per
 * 8-byte page (the DFLASH program granularity), reads are ordinary loads.
 *
 * Only DFLASH (0xAE000000+) is touched; the program flash that holds code is
 * never written. Register sequence verified against the iLLD IfxFlash headers
 * and the TC4Dx user manual (DMU chapter), exercised on real silicon
 * (erase -> program -> read-back).
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

/* Base of the on-chip data flash (non-cached view). */
#define FLASH_DFLASH_BASE 0xAE000000u

/* Erase the DFLASH sector containing addr. Returns 0 on success, -1 if the
   command timed out or a real flash error (sequence/protection/verify/operation)
   was flagged. */
int flash_erase_sector(uint32_t addr);

/* Program one 8-byte page: lo is written at addr, hi at addr+4. addr must be
   8-byte aligned and the page must be erased first. Returns 0 / -1. */
int flash_write_page(uint32_t addr, uint32_t lo, uint32_t hi);

/* Read a 32-bit word from flash. */
uint32_t flash_read32(uint32_t addr);

#endif
