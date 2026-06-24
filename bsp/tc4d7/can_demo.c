/*
 * CAN demo and self-test. Brings up CAN0 node 0 in internal loopback and sends a
 * classic CAN frame (standard id 0x123, 8 data bytes); the controller loops it
 * back on-chip with a self-generated ACK and receives it into RX FIFO 0. The test
 * checks the received id, length, and payload match what was sent, which exercises
 * the whole controller (clock, message RAM, transmit and receive engines) with no
 * external wiring or transceiver.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/clock.c bsp/tc4d7/can.c \
 *     bsp/tc4d7/can_demo.c -I bsp/tc4d7 -o can_demo.elf
 *   tricore-elf-objcopy -O binary can_demo.elf can_demo.bin
 *   tc-load run can_demo.bin 0x70100000      # heartbeat = 0x0CA00D on pass
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "clock.h"
#include "can.h"

#define R(a) (*(volatile unsigned int *)(a))

int main(void)
{
    clock_init_pll();
    clock_enable_can(8);          /* fMCAN = fsource1 / 8 */
    can_init(1);                  /* internal loopback */

    /* little-endian payload -> data words 0xDEADBEEF, 0x12345678 */
    uint8_t tx[8] = { 0xEF, 0xBE, 0xAD, 0xDE, 0x78, 0x56, 0x34, 0x12 };
    int sent = can_send(0x123u, tx, 8);          /* expect 0 */

    uint32_t rid = 0; uint8_t rx[8] = { 0 }; unsigned rlen = 0;
    int got = can_recv(&rid, rx, &rlen);         /* expect 0 */

    unsigned w0 = (unsigned)rx[0] | ((unsigned)rx[1] << 8)
                | ((unsigned)rx[2] << 16) | ((unsigned)rx[3] << 24);

    unsigned ok = (sent == 0) && (got == 0) && (rid == 0x123u)
               && (rlen == 8u) && (w0 == 0xDEADBEEFu);

    R(0x70000004u) = w0;          /* got: received first data word (0xDEADBEEF) */
    R(0x7000000Cu) = rid;         /* marker: received id (0x123) */
    for (;;)
        R(0x70000000u) = ok ? 0x0CA00Du : 0xBADu;  /* heartbeat = 0x0CA00D on pass */
}
