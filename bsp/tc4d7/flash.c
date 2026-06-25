/*
 * On-chip data flash (DFLASH) driver for the TC4x. See flash.h.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "flash.h"

#define R(a) (*(volatile unsigned int *)(a))

/* DMU command interface + status. Commands are writes to CMD_BASE | offset;
   status/errors are read from the DMU HCI registers. */
#define CMD 0xF8080000u             /* command base */
#define ST  0xF8040004u             /* DMU_HCI_STATUS: DFPAGE bit24, REQDONE bit31 */
#define ERR 0xF8040010u             /* DMU_HCI_ERR */

/* Real, operation-relevant error bits. ORIER (bit17) is deliberately excluded:
   it reflects a pre-existing UCB ECC condition, not a fault of erase/program. */
#define ERR_MASK ((1u<<1)|(1u<<2)|(1u<<6)|(1u<<7)|(1u<<16)) /* SQER,PROER,PVER,EVER,OPER */

static void dsync(void) { __asm__ volatile ("dsync" ::: "memory"); }
static void settle(void) { for (volatile int d = 0; d < 200000; d++) { } }
static void clear_status(void) { R(CMD | 0x5554u) = 0xFAu; dsync(); }

/* Return the flash to read mode and let it settle - a read issued too soon after
   a command faults on the freshly accessed page. */
static void reset_read(void) { R(CMD | 0x5554u) = 0xF0u; dsync(); settle(); }

static int wait_done(void)
{
    for (int t = 0; t < 4000000; t++)
        if (R(ST) & (1u << 31))                  /* REQDONE */
            return 1;
    return 0;
}

int flash_erase_sector(uint32_t addr)
{
    clear_status();
    R(CMD | 0xaa50u) = addr;
    R(CMD | 0xaa58u) = 1u;
    R(CMD | 0xaaa8u) = 0x80u;
    R(CMD | 0xaaa8u) = 0x50u;
    dsync();
    int done = wait_done();
    unsigned er = R(ERR);
    reset_read();
    return (done && !(er & ERR_MASK)) ? 0 : -1;
}

int flash_write_page(uint32_t addr, uint32_t lo, uint32_t hi)
{
    clear_status();
    R(CMD | 0x5554u) = 0x5Du;                    /* enter DFLASH page mode */
    dsync();
    int pm = 0;
    for (int t = 0; t < 2000000; t++)
        if (R(ST) & (1u << 24)) { pm = 1; break; } /* DFPAGE */
    if (!pm) { reset_read(); return -1; }

    R(CMD | 0x55f4u) = lo;                        /* load the 8-byte page */
    R(CMD | 0x55f4u) = hi;
    dsync();
    R(CMD | 0xaa50u) = addr;                      /* write page */
    R(CMD | 0xaa58u) = 0u;
    R(CMD | 0xaaa8u) = 0xA0u;
    R(CMD | 0xaaa8u) = 0xAAu;
    dsync();
    int done = wait_done();
    unsigned er = R(ERR);
    reset_read();
    return (done && !(er & ERR_MASK)) ? 0 : -1;
}

uint32_t flash_read32(uint32_t addr)
{
    return R(addr);
}
