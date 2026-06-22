/*
 * tc-load drives the TC4x over the Infineon MCD API (libmcdxdas), the same
 * backend as tas_server. The MCD connection and access primitives live in the
 * shared tcmcd unit. Subcommands.
 *
 *   tc-load run   <file.bin> <load-hex> [dump <buf-hex> | free]
 *       Load a flat binary into RAM (skipped if the address is in flash),
 *       set CPU0 PC to it, run, then either verify the selftest four ways,
 *       dump a captured text buffer, or leave it running (free).
 *
 *   tc-load flash <target-hex> [file.bin]
 *       Erase one 16K PFLASH sector and program it, then verify by read-back.
 *       Refuses targets outside PFLASH so it never writes the UCBs.
 *
 *   tc-load peek  <addr-hex> [count]
 *   tc-load watch <file.bin> <load-hex> <counter-hex> <gap-ms>
 *   tc-load boot
 *       Reset and release so the boot ROM runs flashed code from the BMHD.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "tcmcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define HCI_STATUS 0xF8040004u
#define PAGE_LEN   32u
#define SECTOR_LEN TCMCD_FLASH_SECTOR
#define FLASH_SAFE_MIN 0xA0000000u  /* whole PFLASH incl boot bank, all recoverable via debugger */
#define FLASH_SAFE_MAX 0xA1400000u  /* refuses the UCB region (0xAE...) which holds passwords */
#define FLASH_VIEW_MIN 0xA0000000u

static int cmd_run(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: tc-load run <file.bin> <load-hex> [dump <buf-hex>]\n"); return 2; }
    uint64_t load = strtoull(argv[3], NULL, 0);
    int dump = (argc > 5 && !strcmp(argv[4], "dump"));
    uint64_t dumpaddr = dump ? strtoull(argv[5], NULL, 0) : 0;
    int free_run = (argc > 4 && !strcmp(argv[4], "free"));

    FILE *f = fopen(argv[2], "rb"); if (!f) { perror("open bin"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *img = malloc(sz);
    if (fread(img, 1, sz, f) != (size_t)sz) { perror("read"); return 1; }
    fclose(f);

    open_target(1);
    mcd_addr_st pc; if (!find_pc(&pc)) die("PC register not found", 0);

    if (load >= FLASH_VIEW_MIN) {
        printf("Target in flash, skipping load (assumed already programmed)\n");
    } else {
        const uint32_t CHUNK = 512;
        for (long off = 0; off < sz; off += CHUNK) {
            uint32_t nb = (uint32_t)((sz - off) < CHUNK ? (sz - off) : CHUNK);
            if (xfer(load + off, img + off, nb, MCD_TX_AT_W)) die("write image chunk", 0);
        }
        printf("Wrote %ld bytes to 0x%08llX\n", sz, (unsigned long long)load);
    }

    const uint32_t SEED = 0x4D2;
    if (dump) wr32(dumpaddr, 0);
    else { wr32(0x70000000,0); wr32(0x70000004,0); wr32(0x7000000C,0); wr32(0x70000008,SEED); }

    set_reg(pc, (uint32_t)load);
    printf("Set PC = 0x%08X, running\n", (uint32_t)load);
    if (mcd_run_f(g_core, 1)) die("run", 0);
    if (free_run) { printf("Core left running (free mode), not halting.\n"); return 0; }
    uint32_t hb1=0, hb2=0;
    if (!dump) { hb1 = rd32(0x70000000); usleep(150000); hb2 = rd32(0x70000000); }
    else usleep(300000);
    if (mcd_stop_f(g_core, 1)) die("stop", 0);
    uint32_t pc_now = get_reg(pc);

    if (dump) {
        uint32_t cnt = rd32(dumpaddr); if (cnt > 4096) cnt = 4096;
        printf("\nPC now 0x%08X. Captured %u bytes of output:\n", pc_now, cnt);
        printf("--------------------------------------------------\n");
        for (uint32_t i = 0; i < cnt; i++) { uint32_t w = rd32(dumpaddr + 4 + (i & ~3u)); putchar((w >> ((i&3)*8)) & 0xFF); }
        printf("--------------------------------------------------\n");
    } else {
        uint32_t result = rd32(0x70000004), marker = rd32(0x7000000C), expect = SEED*7u+3u;
        printf("\n==== execution proofs ====\n");
        printf("1. PC parked in our code : pc=0x%08X  %s\n", pc_now, (pc_now>=load && pc_now<load+sz)?"PASS":"FAIL");
        printf("2. challenge response    : got 0x%X expect 0x%X  %s\n", result, expect, result==expect?"PASS":"FAIL");
        printf("3. heartbeat moving      : %u -> %u  %s\n", hb1, hb2, hb2!=hb1?"PASS":"FAIL");
        printf("4. done marker           : 0x%08X  %s\n", marker, marker==0xC0DECAFEu?"PASS":"FAIL");
    }
    return 0;
}

static int cmd_flash(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: tc-load flash <target-hex> [file.bin]\n"); return 2; }
    uint64_t target = strtoull(argv[2], NULL, 0);
    if (target < FLASH_SAFE_MIN || target >= FLASH_SAFE_MAX || (target & (SECTOR_LEN-1))) {
        fprintf(stderr, "refusing unsafe or unaligned target 0x%llX\n", (unsigned long long)target); return 2;
    }
    uint8_t *img; uint32_t len;
    if (argc >= 4) {
        FILE *f = fopen(argv[3], "rb"); if (!f) { perror("open file"); return 1; }
        fseek(f,0,SEEK_END); long s=ftell(f); fseek(f,0,SEEK_SET);
        len = ((s + PAGE_LEN - 1)/PAGE_LEN)*PAGE_LEN; img = calloc(1, len);
        if (fread(img,1,s,f) != (size_t)s) { perror("read"); return 1; }
        fclose(f);
        printf("Programming %ld bytes from %s -> 0x%08llX (%u pages)\n", s, argv[3], (unsigned long long)target, len/PAGE_LEN);
    } else {
        len = PAGE_LEN; img = malloc(len);
        uint32_t pat[8]; for (int i=0;i<8;i++) pat[i]=0x5417C0DEu+i; memcpy(img, pat, len);
        printf("Programming test pattern -> 0x%08llX\n", (unsigned long long)target);
    }

    open_target(1);
    if (tcmcd_flash_erase(target, len)) { fprintf(stderr, "erase failed\n"); return 1; }
    printf("Erased %u sectors from 0x%08llX\n", (len + SECTOR_LEN - 1)/SECTOR_LEN, (unsigned long long)target);
    if (tcmcd_flash_program(target, img, len)) { fprintf(stderr, "program failed\n"); return 1; }
    printf("Programmed %u pages\n", len/PAGE_LEN);

    int ok=1, faults=0;
    for (uint32_t off=0; off<len; off+=4) { uint32_t want; memcpy(&want, img+off, 4); uint32_t got; if (rd32_try(target+off,&got)){faults++;ok=0;continue;} if (got!=want) ok=0; }
    printf("\nFLASH PROGRAM + VERIFY: %s  (%u faults, HCI_STATUS=0x%08X)\n", ok?"PASS":"FAIL", faults, rd32(HCI_STATUS));
    return ok?0:1;
}

static int cmd_peek(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: tc-load peek <addr-hex> [count]\n"); return 2; }
    uint64_t a = strtoull(argv[2], NULL, 0);
    int n = (argc > 3) ? atoi(argv[3]) : 1;
    open_target(1);
    for (int i = 0; i < n; i++) {
        uint32_t v;
        if (rd32_try(a + 4*i, &v)) printf("  0x%08llX: <fault>\n", (unsigned long long)(a + 4*i));
        else printf("  0x%08llX: 0x%08X\n", (unsigned long long)(a + 4*i), v);
    }
    return 0;
}

static int cmd_watch(int argc, char **argv) {
    /* tc-load watch <file.bin> <load-hex> <counter-hex> <gap-ms>
       Loads and runs the firmware (like run), then samples a RAM counter twice
       live while the core runs, to measure its increment rate. */
    if (argc < 6) { fprintf(stderr, "usage: tc-load watch <file.bin> <load-hex> <counter-hex> <gap-ms>\n"); return 2; }
    uint64_t load = strtoull(argv[3], NULL, 0);
    uint64_t counter = strtoull(argv[4], NULL, 0);
    int ms = atoi(argv[5]);

    FILE *f = fopen(argv[2], "rb"); if (!f) { perror("open bin"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *img = malloc(sz);
    if (fread(img, 1, sz, f) != (size_t)sz) { perror("read"); return 1; }
    fclose(f);

    open_target(1);
    mcd_addr_st pc; if (!find_pc(&pc)) die("PC register not found", 0);
    const uint32_t CHUNK = 512;
    for (long off = 0; off < sz; off += CHUNK) {
        uint32_t nb = (uint32_t)((sz - off) < CHUNK ? (sz - off) : CHUNK);
        if (xfer(load + off, img + off, nb, MCD_TX_AT_W)) die("write image chunk", 0);
    }
    wr32(counter, 0);
    set_reg(pc, (uint32_t)load);
    if (mcd_run_f(g_core, 1)) die("run", 0);
    uint32_t v0 = rd32(counter);
    usleep((useconds_t)ms * 1000);
    uint32_t v1 = rd32(counter);
    mcd_stop_f(g_core, 1);
    unsigned long rate = (unsigned long)(v1 - v0) * 1000UL / (ms ? ms : 1);
    printf("counter 0x%llX: %u -> %u, delta %u over %d ms = %lu /s\n",
           (unsigned long long)counter, v0, v1, (v1 - v0), ms, rate);
    printf("implied baud (bytes/s * 10) = %lu\n", rate * 10UL);
    return 0;
}

static int cmd_boot(int argc, char **argv) {
    (void)argc; (void)argv;
    open_target(0); /* reset without halt, the boot ROM reads the BMHD and runs flashed code */
    printf("Booting from flash, the BMHD start address (stad) decides what runs.\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s run|flash|peek|watch|boot ...\n", argv[0]); return 2; }
    int rc;
    if (!strcmp(argv[1], "run"))        rc = cmd_run(argc, argv);
    else if (!strcmp(argv[1], "flash")) rc = cmd_flash(argc, argv);
    else if (!strcmp(argv[1], "peek"))  rc = cmd_peek(argc, argv);
    else if (!strcmp(argv[1], "watch")) rc = cmd_watch(argc, argv);
    else if (!strcmp(argv[1], "boot"))  rc = cmd_boot(argc, argv);
    else { fprintf(stderr, "unknown subcommand %s\n", argv[1]); return 2; }
    tcmcd_close();
    return rc;
}
