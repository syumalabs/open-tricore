/*
 * tc-gdbserver, a GDB remote stub for the TC4x. It speaks the GDB Remote Serial
 * Protocol over TCP and drives the target through the shared tcmcd MCD layer,
 * the same backend as tc-load. With this, a TriCore GDB can connect to real
 * silicon for source-level inspection, registers, and memory.
 *
 * Phase 3 scope, connect to a halted target and inspect it. Register and memory
 * read and write, the handshake, and a single thread. Execution control and
 * breakpoints come in later phases, continue and step currently report the
 * target as stopped without running it.
 *
 * Usage, tc-gdbserver [port], default port 3333. Point GDB at it with
 *   tricore-elf-gdb your.elf -ex 'target remote :3333'
 * GDB learns the TriCore architecture from the ELF, so no target description is
 * served, the built in 44 register layout is used.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "tcmcd.h"
#include "rsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>

#define NREG  44          /* TRICORE_NUM_REGS, d0-d15 a0-a15 lcx fcx pcx psw pc icr isp btv biv syscon pcon0 dcon0 */
#define BUFSZ 0x4000

static const char hexd[] = "0123456789abcdef";
static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static void put_u32_le(char *out, uint32_t v) {
    for (int b = 0; b < 4; b++) { out[b*2] = hexd[(v >> (b*8+4)) & 0xF]; out[b*2+1] = hexd[(v >> (b*8)) & 0xF]; }
}
static uint32_t get_u32_le(const char *in) {
    uint32_t v = 0;
    for (int b = 0; b < 4; b++) { int hi = hexval(in[b*2]), lo = hexval(in[b*2+1]); v |= (uint32_t)((hi<<4)|lo) << (b*8); }
    return v;
}

/* The 12 core registers after the d and a GPRs, in GDB order. */
static const char *const g_sfr_names[12] =
    { "lcx","fcx","pcx","psw","pc","icr","isp","btv","biv","syscon","pcon0","dcon0" };

/* The GDB name for a register index. */
static void gdb_reg_name(int idx, char *out, size_t n) {
    if (idx < 16)      snprintf(out, n, "d%d", idx);
    else if (idx < 32) snprintf(out, n, "a%d", idx - 16);
    else               snprintf(out, n, "%s", g_sfr_names[idx - 32]);
}

/* Map GDB register numbers to MCD register addresses. */
static mcd_addr_st g_rega[NREG];
static int         g_regok[NREG];

/* Does an MCD register name correspond to GDB register index idx? Case
   insensitive, with the few aliases where the spellings differ. */
static int reg_name_match(const char *mcd, int idx) {
    char prim[8];
    if (idx < 16)  { snprintf(prim, sizeof(prim), "d%d", idx);      return strcasecmp(mcd, prim) == 0; }
    if (idx < 32)  { snprintf(prim, sizeof(prim), "a%d", idx - 16); return strcasecmp(mcd, prim) == 0; }
    static const char *const al[12]  = { 0,0,"pcxi",0,0,0,0,0,0,0,"pmucon0","dmucon" };
    static const char *const al2[12] = { 0,0,0,0,0,0,0,0,0,0,"pmu0con","dmu0con" };
    int s = idx - 32;
    if (strcasecmp(mcd, g_sfr_names[s]) == 0) return 1;
    if (al[s]  && strcasecmp(mcd, al[s])  == 0) return 1;
    if (al2[s] && strcasecmp(mcd, al2[s]) == 0) return 1;
    return 0;
}

static void build_regmap(void) {
    uint32_t ngrp = 0; mcd_qry_reg_groups_f(g_core, 0, &ngrp, NULL);
    for (uint32_t grp = 0; grp < ngrp; grp++) {
        uint32_t nr = 0; mcd_qry_reg_map_f(g_core, grp, 0, &nr, NULL);
        mcd_register_info_st *ri = calloc(nr, sizeof(*ri));
        mcd_qry_reg_map_f(g_core, grp, 0, &nr, ri);
        for (uint32_t i = 0; i < nr; i++) {
            if (getenv("TC_GDB_DUMPREGS")) fprintf(stderr, "  mcd reg: %s\n", ri[i].regname);
            for (int idx = 0; idx < NREG; idx++)
                if (!g_regok[idx] && reg_name_match(ri[i].regname, idx)) { g_rega[idx] = ri[i].addr; g_regok[idx] = 1; }
        }
        free(ri);
    }
    int missing = 0;
    for (int i = 0; i < NREG; i++) if (!g_regok[i]) missing++;
    if (missing) {
        fprintf(stderr, "tc-gdbserver, %d registers not exposed by MCD, shown as unavailable:", missing);
        for (int i = 0; i < NREG; i++) if (!g_regok[i]) { char nm[8]; gdb_reg_name(i, nm, sizeof(nm)); fprintf(stderr, " %s", nm); }
        fprintf(stderr, "\n");
    }
}

/* --- packet handlers --- */

static void handle_q(int fd, const char *p) {
    if (!strncmp(p, "qSupported", 10))      rsp_put_str(fd, "PacketSize=1000");
    else if (!strcmp(p, "qAttached"))       rsp_put_str(fd, "1");
    else if (!strcmp(p, "qC"))              rsp_put_str(fd, "QC1");
    else if (!strcmp(p, "qfThreadInfo"))    rsp_put_str(fd, "m1");
    else if (!strcmp(p, "qsThreadInfo"))    rsp_put_str(fd, "l");
    else if (!strncmp(p, "qSymbol", 7))     rsp_put_str(fd, "OK");
    else                                    rsp_put_str(fd, "");
}

static void handle_v(int fd, const char *p) {
    if (!strcmp(p, "vMustReplyEmpty")) rsp_put_str(fd, "");
    else                               rsp_put_str(fd, ""); /* vCont? empty, GDB falls back to c and s */
}

static void handle_g(int fd) {
    char out[NREG*8];
    for (int i = 0; i < NREG; i++) {
        if (g_regok[i]) put_u32_le(out + i*8, get_reg(g_rega[i]));
        else memset(out + i*8, 'x', 8);
    }
    rsp_put_packet(fd, out, NREG*8);
}

static void handle_G(int fd, const char *p) {
    if (strlen(p) < (size_t)NREG*8) { rsp_put_str(fd, "E01"); return; }
    for (int i = 0; i < NREG; i++) if (g_regok[i]) set_reg(g_rega[i], get_u32_le(p + i*8));
    rsp_put_str(fd, "OK");
}

static void handle_p(int fd, const char *p) {
    unsigned idx = (unsigned)strtoul(p + 1, NULL, 16);
    char out[8];
    if (idx < NREG && g_regok[idx]) put_u32_le(out, get_reg(g_rega[idx]));
    else memset(out, 'x', 8);
    rsp_put_packet(fd, out, 8);
}

static void handle_P(int fd, const char *p) {
    const char *eq = strchr(p, '=');
    if (!eq) { rsp_put_str(fd, "E01"); return; }
    unsigned idx = (unsigned)strtoul(p + 1, NULL, 16);
    if (idx < NREG && g_regok[idx]) set_reg(g_rega[idx], get_u32_le(eq + 1));
    rsp_put_str(fd, "OK");
}

static void handle_m(int fd, const char *p) {
    const char *comma = strchr(p, ',');
    uint64_t addr = strtoull(p + 1, NULL, 16);
    uint32_t len  = comma ? (uint32_t)strtoul(comma + 1, NULL, 16) : 0;
    if (len == 0) { rsp_put_str(fd, ""); return; }
    if (len > (BUFSZ - 8) / 2) len = (BUFSZ - 8) / 2;
    uint8_t *mb = malloc(len);
    if (tcmcd_read(addr, mb, len)) { rsp_put_str(fd, "E01"); free(mb); return; }
    char *out = malloc((size_t)len * 2);
    for (uint32_t i = 0; i < len; i++) { out[i*2] = hexd[mb[i] >> 4]; out[i*2+1] = hexd[mb[i] & 0xF]; }
    rsp_put_packet(fd, out, (size_t)len * 2);
    free(out); free(mb);
}

static void handle_M(int fd, const char *p) {
    const char *comma = strchr(p, ',');
    const char *colon = strchr(p, ':');
    if (!comma || !colon) { rsp_put_str(fd, "E01"); return; }
    uint64_t addr = strtoull(p + 1, NULL, 16);
    uint32_t len  = (uint32_t)strtoul(comma + 1, NULL, 16);
    const char *d = colon + 1;
    uint8_t *mb = malloc(len ? len : 1);
    for (uint32_t i = 0; i < len; i++) {
        int hi = hexval(d[i*2]), lo = hexval(d[i*2+1]);
        if (hi < 0 || lo < 0) { rsp_put_str(fd, "E01"); free(mb); return; }
        mb[i] = (uint8_t)((hi << 4) | lo);
    }
    if (tcmcd_write(addr, mb, len)) rsp_put_str(fd, "E01");
    else rsp_put_str(fd, "OK");
    free(mb);
}

static void serve(int fd) {
    char buf[BUFSZ];
    for (;;) {
        int n = rsp_get_packet(fd, buf, sizeof(buf));
        if (n == -1) { printf("GDB disconnected\n"); return; }
        if (n == -2) { rsp_put_str(fd, "S05"); continue; } /* interrupt, already halted */
        switch (buf[0]) {
            case '?': rsp_put_str(fd, "S05"); break;
            case 'g': handle_g(fd); break;
            case 'G': handle_G(fd, buf + 1); break;
            case 'p': handle_p(fd, buf); break;
            case 'P': handle_P(fd, buf); break;
            case 'm': handle_m(fd, buf); break;
            case 'M': handle_M(fd, buf); break;
            case 'q': handle_q(fd, buf); break;
            case 'v': handle_v(fd, buf); break;
            case 'H': rsp_put_str(fd, "OK"); break;
            case 'T': rsp_put_str(fd, "OK"); break;   /* thread alive */
            case 'c': case 's': rsp_put_str(fd, "S05"); break; /* execution not yet implemented */
            case 'D': rsp_put_str(fd, "OK"); return;  /* detach */
            case 'k': return;                          /* kill */
            default:  rsp_put_str(fd, ""); break;
        }
    }
}

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 3333;

    open_target(1); /* reset and halt, target ready for inspection */
    build_regmap();

    int lfd = rsp_listen(port);
    if (lfd < 0) { perror("listen"); tcmcd_close(); return 1; }
    printf("tc-gdbserver listening on localhost:%d, target halted.\n", port);
    printf("Connect with:\n  tricore-elf-gdb your.elf -ex 'target remote :%d'\n", port);

    int fd = rsp_accept(lfd);
    if (fd < 0) { perror("accept"); close(lfd); tcmcd_close(); return 1; }
    printf("GDB connected.\n");
    serve(fd);

    close(fd); close(lfd); tcmcd_close();
    return 0;
}
