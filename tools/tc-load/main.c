/*
 * tc-load drives the TC4x over the Infineon MCD API (libmcdxdas), the same
 * backend as tas_server. Two subcommands.
 *
 *   tc-load run   <file.bin> <load-hex> [dump <buf-hex>]
 *       Load a flat binary into RAM (skipped if the address is in flash),
 *       set CPU0 PC to it, run, then either verify the selftest four ways or,
 *       with dump, read back a captured text buffer.
 *
 *   tc-load flash <target-hex> [file.bin]
 *       Erase one 16K PFLASH sector and program it, then verify by read-back.
 *       With no file it writes a test pattern. Refuses targets below 0xA0200000
 *       so it can never touch the boot bank, and never writes UCBs.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "mcd_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

/* flash command interface */
#define CMD_BASE   0xF8080000u
#define HCI_STATUS 0xF8040004u
#define BUSY_MASK  0x000F0FFFu
#define PAGE_LEN   32u
#define SECTOR_LEN 0x4000u
#define FLASH_SAFE_MIN 0xA0200000u
#define FLASH_SAFE_MAX 0xA1400000u
#define FLASH_VIEW_MIN 0xA0000000u

static const mcd_core_st *g_core = NULL;
static uint32_t g_msid = 0;

static void die(const char *what, mcd_return_et ret) {
    fprintf(stderr, "ERROR: %s (ret=%d)\n", what, (int)ret);
    if (g_core) { mcd_error_info_st ei; mcd_qry_error_info_f(g_core, &ei); fprintf(stderr, "  mcd: %s\n", ei.error_str); }
    exit(1);
}
static mcd_return_et xfer(uint64_t addr, void *data, uint32_t n, mcd_tx_access_type_et at) {
    mcd_tx_st tx; memset(&tx, 0, sizeof(tx));
    tx.addr.address = addr; tx.addr.mem_space_id = g_msid;
    tx.access_type = at; tx.options = MCD_TX_OPT_DEFAULT; tx.access_width = 4;
    tx.data = (uint8_t *)data; tx.num_bytes = n;
    mcd_txlist_st txl = { &tx, 1, 0 };
    return mcd_execute_txlist_f(g_core, &txl);
}
static uint32_t rd32(uint64_t a){ uint32_t v=0; if (xfer(a,&v,4,MCD_TX_AT_R)) die("rd32",0); return v; }
static void     wr32(uint64_t a, uint32_t v){ if (xfer(a,&v,4,MCD_TX_AT_W)) die("wr32",0); }
static int      rd32_try(uint64_t a, uint32_t *v){ *v=0; return xfer(a,v,4,MCD_TX_AT_R) ? -1 : 0; }

/* connect, enumerate to CPU0, open it, pick a memory space, halt */
static void open_target(void) {
    mcd_api_version_st ver = { MCD_API_VER_MAJOR, MCD_API_VER_MINOR, MCD_API_VER_AUTHOR };
    mcd_impl_version_info_st impl;
    if (mcd_initialize_f(&ver, &impl)) { fprintf(stderr, "mcd_initialize failed\n"); exit(1); }
    static mcd_server_st *server = NULL;
    if (mcd_open_server_f("", "", &server)) die("open_server", 0);
    uint32_t n;
    static mcd_core_con_info_st sys, dev;
    n = 1; if (mcd_qry_systems_f(0, &n, &sys) || !n) die("qry_systems", 0);
    n = 1; if (mcd_qry_devices_f(&sys, 0, &n, &dev) || !n) die("qry_devices", 0);
    uint32_t ncores = 0; mcd_qry_cores_f(&dev, 0, &ncores, NULL);
    mcd_core_con_info_st *cores = calloc(ncores, sizeof(*cores));
    if (mcd_qry_cores_f(&dev, 0, &ncores, cores) || !ncores) die("qry_cores", 0);
    static mcd_core_st *core = NULL;
    if (mcd_open_core_f(&cores[0], &core)) die("open_core", 0);
    g_core = core;
    uint32_t nms = 0; mcd_qry_mem_spaces_f(core, 0, &nms, NULL);
    mcd_memspace_st *ms = calloc(nms, sizeof(*ms)); mcd_qry_mem_spaces_f(core, 0, &nms, ms);
    g_msid = ms[0].mem_space_id;
    if (mcd_stop_f(core, 1)) die("stop", 0);
    printf("Device %s, core 0 halted.\n", dev.device);
}

static int find_pc(mcd_addr_st *pc) {
    uint32_t ngrp = 0; mcd_qry_reg_groups_f(g_core, 0, &ngrp, NULL);
    for (uint32_t g = 0; g < ngrp; g++) {
        uint32_t nr = 0; mcd_qry_reg_map_f(g_core, g, 0, &nr, NULL);
        mcd_register_info_st *ri = calloc(nr, sizeof(*ri));
        mcd_qry_reg_map_f(g_core, g, 0, &nr, ri);
        for (uint32_t i = 0; i < nr; i++)
            if (!strcmp(ri[i].regname, "PC") || !strcmp(ri[i].regname, "pc")) { *pc = ri[i].addr; free(ri); return 1; }
        free(ri);
    }
    return 0;
}
static void set_reg(mcd_addr_st a, uint32_t v) {
    mcd_tx_st tx; memset(&tx, 0, sizeof(tx));
    tx.addr = a; tx.access_type = MCD_TX_AT_W; tx.access_width = 4;
    tx.data = (uint8_t *)&v; tx.num_bytes = 4;
    mcd_txlist_st txl = { &tx, 1, 0 };
    if (mcd_execute_txlist_f(g_core, &txl)) die("set reg", 0);
}
static uint32_t get_reg(mcd_addr_st a) {
    uint32_t v = 0; mcd_tx_st tx; memset(&tx, 0, sizeof(tx));
    tx.addr = a; tx.access_type = MCD_TX_AT_R; tx.access_width = 4;
    tx.data = (uint8_t *)&v; tx.num_bytes = 4;
    mcd_txlist_st txl = { &tx, 1, 0 };
    mcd_execute_txlist_f(g_core, &txl); return v;
}

static int cmd_run(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: tc-load run <file.bin> <load-hex> [dump <buf-hex>]\n"); return 2; }
    uint64_t load = strtoull(argv[3], NULL, 0);
    int dump = (argc > 5 && !strcmp(argv[4], "dump"));
    uint64_t dumpaddr = dump ? strtoull(argv[5], NULL, 0) : 0;

    FILE *f = fopen(argv[2], "rb"); if (!f) { perror("open bin"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *img = malloc(sz);
    if (fread(img, 1, sz, f) != (size_t)sz) { perror("read"); return 1; }
    fclose(f);

    open_target();
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

static int wait_unbusy(const char *what){
    for (int i=0;i<200000;i++){ if ((rd32(HCI_STATUS) & BUSY_MASK)==0) return 0; usleep(50); }
    fprintf(stderr, "timeout after %s, status=0x%08X\n", what, rd32(HCI_STATUS)); return 1;
}
static int program_page(uint64_t pageAddr, const uint32_t *w){
    wr32(CMD_BASE|0x5554, 0xFA);
    wr32(CMD_BASE|0x5554, 0x50);
    for (int i=0;i<8;i++) wr32(CMD_BASE|0x55f4, w[i]);
    wr32(CMD_BASE|0xaa50, (uint32_t)pageAddr);
    wr32(CMD_BASE|0xaa58, 0);
    wr32(CMD_BASE|0xaaa8, 0xa0);
    wr32(CMD_BASE|0xaaa8, 0xaa);
    return wait_unbusy("program");
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
    if (len > SECTOR_LEN) { fprintf(stderr, "payload spans >1 sector, not supported\n"); return 2; }

    open_target();
    wr32(CMD_BASE|0x5554, 0xFA);
    wr32(CMD_BASE|0xaa50, (uint32_t)target);
    wr32(CMD_BASE|0xaa58, 1);
    wr32(CMD_BASE|0xaaa8, 0x80);
    wr32(CMD_BASE|0xaaa8, 0x50);
    if (wait_unbusy("erase")) return 1;
    printf("Erased 16K sector at 0x%08llX\n", (unsigned long long)target);

    for (uint32_t off=0; off<len; off+=PAGE_LEN) { uint32_t w[8]; memcpy(w, img+off, PAGE_LEN); if (program_page(target+off, w)) return 1; }
    wr32(CMD_BASE|0x5554, 0xf0);
    printf("Programmed %u pages\n", len/PAGE_LEN);

    int ok=1, faults=0;
    for (uint32_t off=0; off<len; off+=4) { uint32_t want; memcpy(&want, img+off, 4); uint32_t got; if (rd32_try(target+off,&got)){faults++;ok=0;continue;} if (got!=want) ok=0; }
    printf("\nFLASH PROGRAM + VERIFY: %s  (%u faults, HCI_STATUS=0x%08X)\n", ok?"PASS":"FAIL", faults, rd32(HCI_STATUS));
    return ok?0:1;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s run|flash ...\n", argv[0]); return 2; }
    int rc;
    if (!strcmp(argv[1], "run"))        rc = cmd_run(argc, argv);
    else if (!strcmp(argv[1], "flash")) rc = cmd_flash(argc, argv);
    else { fprintf(stderr, "unknown subcommand %s\n", argv[1]); return 2; }
    if (g_core) { mcd_close_core_f(g_core); mcd_exit_f(); }
    return rc;
}
