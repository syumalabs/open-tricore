/*
 * tcmcd, shared MCD connection and access layer for the open-tricore host
 * tools. Wraps the Infineon MCD API (libmcdxdas, via tas_server) with the
 * connect, reset, memory, and register primitives that both tc-load and
 * tc-gdbserver use.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef TCMCD_H
#define TCMCD_H

#include "mcd_api.h"
#include <stdint.h>

/* The opened CPU0 core handle and its primary memory space id, valid after
   open_target. Shared so callers can issue run-control calls directly. */
extern const mcd_core_st *g_core;
extern uint32_t g_msid;

/* Print an MCD error with context and exit. */
void die(const char *what, mcd_return_et ret);

/* Raw transfer against the core's primary memory space. */
mcd_return_et xfer(uint64_t addr, void *data, uint32_t n, mcd_tx_access_type_et at);

/* 32-bit memory helpers. rd32 and wr32 abort on error, rd32_try returns -1. */
uint32_t rd32(uint64_t a);
void     wr32(uint64_t a, uint32_t v);
int      rd32_try(uint64_t a, uint32_t *v);

/* Connect, enumerate to CPU0, open it, pick a memory space.
   halt=1 resets and halts for loading. halt=0 resets and lets it boot. */
void open_target(int halt);

/* Locate the PC register in the MCD register map. Returns 1 on success. */
int find_pc(mcd_addr_st *pc);

/* Read and write a register by its MCD address. */
void     set_reg(mcd_addr_st a, uint32_t v);
uint32_t get_reg(mcd_addr_st a);

/* Arbitrary address and length memory access, handled via aligned 32-bit
   words (with read-modify-write on the edges for writes). Return 0 on success,
   -1 on any access fault. Non-fatal, for callers that report errors instead of
   aborting, such as the gdbserver. */
int tcmcd_read(uint64_t addr, uint8_t *buf, uint32_t len);
int tcmcd_write(uint64_t addr, const uint8_t *buf, uint32_t len);

/* PFLASH programming over the command sequence interface. erase wipes every 16K
   sector covering [addr, addr+len). program writes data starting at a 32-byte
   page aligned address, padding the final page with 0xFF. Both refuse any target
   outside PFLASH, so the UCB region (passwords and protection) is never touched.
   Return 0 on success, -1 on failure. The sector size is TCMCD_FLASH_SECTOR. */
#define TCMCD_FLASH_SECTOR 0x4000u
int tcmcd_flash_erase(uint64_t addr, uint64_t len);
int tcmcd_flash_program(uint64_t addr, const uint8_t *data, uint32_t len);

/* Close the core and shut down MCD, if open. */
void tcmcd_close(void);

#endif
