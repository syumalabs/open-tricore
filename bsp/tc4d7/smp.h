/*
 * Minimal multicore (SMP) support for the TC4x. Start a secondary TriCore core
 * from CPU0 and find out which core you are running on.
 *
 * After reset only CPU0 runs; the other cores sit in boot halt. core_start sets
 * a secondary core's program counter and releases its boot halt, so it begins
 * executing the entry function. Cores communicate through shared memory, the
 * shared LMU works once its access-protection region is opened to all master
 * tags (see the demo and the DMA driver).
 *
 * Scope: the secondary core starts with no stack and no context save area, so
 * the entry function must be a leaf that makes no calls and needs no stack
 * (a polling or compute loop over fixed memory). Setting up a full per-core C
 * runtime (stack plus CSA) is a separate step the entry can do itself.
 *
 * Register layout verified against the iLLD TC4Dx headers, exercised on silicon.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef SMP_H
#define SMP_H

/* Start secondary core (1..5) executing at entry. Returns 0 on success, -1 for
   an invalid core number. Call from CPU0. */
int core_start(unsigned core, void (*entry)(void));

/* The index (0..6) of the core running this code, from the CORE_ID register. */
unsigned core_id(void);

#endif
