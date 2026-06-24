/*
 * Minimal CAN (MCMCAN / Bosch M_CAN) driver for the TC4x.
 *
 * Brings up CAN0 node 0 as a classic-CAN controller. The TC4D7 Lite Kit has no
 * on-board CAN transceiver wired for self-test, so can_init() can put the node in
 * internal loopback mode, where transmitted frames are looped back to the
 * receiver on-chip with a self-generated ACK and no bus or transceiver is needed,
 * which makes the controller verifiable end to end with no external wiring.
 *
 * Bring-up notes (all verified on real silicon):
 *  - The MCR (module control register) carries change-enable bits CCCE and CI that
 *    do NOT read back set. Build the value in a single local and write it; never
 *    read MCR back mid-sequence, or those bits drop and the protected CLKSEL /
 *    RAM-init writes are silently rejected (this was the bug that left the node
 *    with no clock so it never left init).
 *  - Internal loopback is the classic Bosch path: CCCR.TEST=1, then TEST.LBCK=1,
 *    then CCCR.MON=1. PORTCTRL.LBM alone is external loopback (it transmits but
 *    gets no ACK), and MON alone makes the node receive-only (it will not transmit).
 *  - The message-RAM ECC must be initialised once (MCR.RINIT) before use.
 *  - Call clock_init_pll() and clock_enable_can() before can_init(), or fMCAN is
 *    off and the node never leaves init.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef CAN_H
#define CAN_H

#include <stdint.h>

/* Bring up CAN0 node 0 for classic CAN with one dedicated TX buffer and a 2-entry
   RX FIFO 0 (8-byte frames, accept-all filter). If internal_loopback is non-zero
   the node runs in internal loopback (wiring-free self-test); otherwise it runs in
   normal mode on the real bus. Call clock_init_pll() and clock_enable_can() first.
   Returns 0, or -1 if the node did not leave init. */
int can_init(int internal_loopback);

/* Transmit one classic CAN frame from dedicated TX buffer 0: standard 11-bit id,
   0..8 data bytes. Returns 0 when the transmission completes, -1 on timeout. */
int can_send(uint32_t id, const uint8_t *data, unsigned len);

/* Read one frame from RX FIFO 0 if present. On success stores the standard id in
   *id (when non-NULL), up to 8 payload bytes at data, the length in *len, and
   returns 0. Returns 1 if the FIFO is empty, -1 on error. */
int can_recv(uint32_t *id, uint8_t *data, unsigned *len);

#endif
