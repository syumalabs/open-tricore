/*
 * tcmcd, shared MCD connection and access layer. See tcmcd.h.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "tcmcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const mcd_core_st *g_core = NULL;
uint32_t g_msid = 0;

void die(const char *what, mcd_return_et ret) {
    fprintf(stderr, "ERROR: %s (ret=%d)\n", what, (int)ret);
    if (g_core) { mcd_error_info_st ei; mcd_qry_error_info_f(g_core, &ei); fprintf(stderr, "  mcd: %s\n", ei.error_str); }
    exit(1);
}

mcd_return_et xfer(uint64_t addr, void *data, uint32_t n, mcd_tx_access_type_et at) {
    mcd_tx_st tx; memset(&tx, 0, sizeof(tx));
    tx.addr.address = addr; tx.addr.mem_space_id = g_msid;
    tx.access_type = at; tx.options = MCD_TX_OPT_DEFAULT; tx.access_width = 4;
    tx.data = (uint8_t *)data; tx.num_bytes = n;
    mcd_txlist_st txl = { &tx, 1, 0 };
    return mcd_execute_txlist_f(g_core, &txl);
}

uint32_t rd32(uint64_t a){ uint32_t v=0; if (xfer(a,&v,4,MCD_TX_AT_R)) die("rd32",0); return v; }
void     wr32(uint64_t a, uint32_t v){ if (xfer(a,&v,4,MCD_TX_AT_W)) die("wr32",0); }
int      rd32_try(uint64_t a, uint32_t *v){ *v=0; return xfer(a,v,4,MCD_TX_AT_R) ? -1 : 0; }

void open_target(int halt) {
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
    /* reset, halting for load or releasing to boot from flash */
    uint32_t rstv = 0;
    if (mcd_qry_rst_classes_f(core, &rstv) == 0 && rstv != 0) mcd_rst_f(core, rstv, halt ? 1 : 0);
    uint32_t nms = 0; mcd_qry_mem_spaces_f(core, 0, &nms, NULL);
    mcd_memspace_st *ms = calloc(nms, sizeof(*ms)); mcd_qry_mem_spaces_f(core, 0, &nms, ms);
    g_msid = ms[0].mem_space_id;
    if (halt && mcd_stop_f(core, 1)) die("stop", 0);
    printf("Device %s, %s.\n", dev.device, halt ? "core 0 halted" : "reset and released to boot from flash");
}

int find_pc(mcd_addr_st *pc) {
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

void set_reg(mcd_addr_st a, uint32_t v) {
    mcd_tx_st tx; memset(&tx, 0, sizeof(tx));
    tx.addr = a; tx.access_type = MCD_TX_AT_W; tx.access_width = 4;
    tx.data = (uint8_t *)&v; tx.num_bytes = 4;
    mcd_txlist_st txl = { &tx, 1, 0 };
    if (mcd_execute_txlist_f(g_core, &txl)) die("set reg", 0);
}

uint32_t get_reg(mcd_addr_st a) {
    uint32_t v = 0; mcd_tx_st tx; memset(&tx, 0, sizeof(tx));
    tx.addr = a; tx.access_type = MCD_TX_AT_R; tx.access_width = 4;
    tx.data = (uint8_t *)&v; tx.num_bytes = 4;
    mcd_txlist_st txl = { &tx, 1, 0 };
    mcd_execute_txlist_f(g_core, &txl); return v;
}

int tcmcd_read(uint64_t addr, uint8_t *buf, uint32_t len) {
    uint64_t a0 = addr & ~3ull, a1 = (addr + len + 3) & ~3ull;
    for (uint64_t a = a0; a < a1; a += 4) {
        uint32_t w;
        if (rd32_try(a, &w)) return -1;
        for (int b = 0; b < 4; b++) {
            uint64_t cur = a + b;
            if (cur >= addr && cur < addr + len) buf[cur - addr] = (uint8_t)(w >> (b * 8));
        }
    }
    return 0;
}

int tcmcd_write(uint64_t addr, const uint8_t *buf, uint32_t len) {
    uint64_t a0 = addr & ~3ull, a1 = (addr + len + 3) & ~3ull;
    for (uint64_t a = a0; a < a1; a += 4) {
        uint32_t w = 0;
        int full = (a >= addr) && (a + 4 <= addr + len);
        if (!full && rd32_try(a, &w)) return -1; /* read-modify-write the partial edge word */
        for (int b = 0; b < 4; b++) {
            uint64_t cur = a + b;
            if (cur >= addr && cur < addr + len)
                w = (w & ~(0xFFu << (b * 8))) | ((uint32_t)buf[cur - addr] << (b * 8));
        }
        if (xfer(a, &w, 4, MCD_TX_AT_W)) return -1;
    }
    return 0;
}

void tcmcd_close(void) {
    if (g_core) { mcd_close_core_f(g_core); mcd_exit_f(); g_core = NULL; }
}
