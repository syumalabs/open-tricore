/*
 * tc-ppu, run code on the AURIX TC4x PPU (a Synopsys ARC EV71) from the host
 * and exchange data with it. Thin CLI over the shared ppu helper, which carries
 * the boot recipe and data protocol, see tools/common/ppu.{c,h} and
 * docs/ppu-reverse-engineering.md.
 *
 *   tc-ppu run  <image.bin>                          load, start, report run state
 *   tc-ppu call <image.bin> [--out N] <A> <B> ..     feed operands, read N results
 *
 * Operands and results are 32-bit hex. The image must be ARC code linked at the
 * PPU code base, see ppu/ for examples and the build steps.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "ppu.h"
#include "tcmcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static long load_file(const char *path, uint8_t *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > cap) { fclose(f); fprintf(stderr, "bad image size\n"); return -1; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); return -1; }
    fclose(f);
    return n;
}

static void usage(void)
{
    fprintf(stderr,
        "usage:\n"
        "  tc-ppu run  <image.bin>                       load, run, report PPU run state\n"
        "  tc-ppu call <image.bin> [--out N] <A> <B> ..  feed operands, stream N results back\n"
        "operands and results are 32-bit hex, --out defaults to 1. The image must be\n"
        "ARC code linked at 0x%08X, see ppu/ for examples and build steps.\n", PPU_CODE_BASE);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 3) { usage(); return 2; }

    static uint8_t img[64 * 1024];
    long n = load_file(argv[2], img, sizeof img);
    if (n < 0) return 1;

    open_target(1);   /* connect, reset and halt CPU0, the PPU is driven over SRI */

    int rc = 0;
    if (strcmp(argv[1], "run") == 0) {
        if (ppu_load_run(img, (uint32_t)n) != 0) { fprintf(stderr, "load failed\n"); rc = 1; }
        else {
            usleep(50000);
            const char *st[] = { "running", "sleeping", "halted", "?" };
            printf("PPU started at 0x%08X (%s)\n", PPU_CODE_BASE, st[ppu_runstate()]);
        }
    } else if (strcmp(argv[1], "call") == 0) {
        int argi = 3;
        uint32_t nout = 1;
        if (argi + 1 < argc && strcmp(argv[argi], "--out") == 0) {
            nout = (uint32_t)strtoul(argv[argi + 1], NULL, 10);
            argi += 2;
        }
        if (nout == 0 || nout > 256) { fprintf(stderr, "--out must be 1..256\n"); tcmcd_close(); return 2; }

        uint32_t nin = (uint32_t)(argc - argi);
        uint32_t *in = nin ? calloc(nin, sizeof(uint32_t)) : NULL;
        for (uint32_t i = 0; i < nin; i++) in[i] = (uint32_t)strtoul(argv[argi + i], NULL, 16);
        uint32_t *out = calloc(nout, sizeof(uint32_t));

        if (ppu_call(img, (uint32_t)n, in, nin, out, nout) != 0) {
            fprintf(stderr, "ppu_call failed (core did not resume)\n");
            rc = 1;
        } else {
            for (uint32_t i = 0; i < nout; i++) printf("0x%08X\n", out[i]);
        }
        free(in);
        free(out);
    } else {
        usage();
        rc = 2;
    }

    tcmcd_close();
    return rc;
}
