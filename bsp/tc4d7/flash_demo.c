/*
 * DFLASH demo / self-test: erase a data-flash sector, program an 8-byte page,
 * read it back, and verify - persistent storage with no external memory.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/clock.c bsp/tc4d7/flash.c \
 *     bsp/tc4d7/flash_demo.c -I bsp/tc4d7 -o flash_demo.elf
 *   tc-load run flash_demo.bin 0x70100000      # heartbeat = 0xDF0A11 on pass
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "clock.h"
#include "flash.h"

#define R(a) (*(volatile unsigned int *)(a))

int main(void)
{
    clock_init_pll();

    uint32_t addr = FLASH_DFLASH_BASE;
    int e = flash_erase_sector(addr);
    int p = flash_write_page(addr, 0xDEADBEEFu, 0xCAFEF00Du);
    uint32_t w0 = flash_read32(addr);
    uint32_t w1 = flash_read32(addr + 4u);

    unsigned ok = (e == 0) && (p == 0) && (w0 == 0xDEADBEEFu) && (w1 == 0xCAFEF00Du);

    R(0x70000004u) = w0;                          /* got  = word0 (want 0xDEADBEEF) */
    R(0x70000008u) = w1;                          /* seed = word1 */
    R(0x7000000Cu) = (unsigned)(((e & 0xFF) << 8) | (p & 0xFF)); /* marker = erase/prog rc */
    for (;;)
        R(0x70000000u) = ok ? 0xDF0A11u : 0xBADu; /* heartbeat = 0xDF0A11 on pass */
}
