/*
 * tc-ppu, run code on the AURIX TC4x PPU (a Synopsys ARC EV71) from the host
 * over MCD, and exchange data with it.
 *
 * The PPU scalar core was brought up entirely by clean-room reverse
 * engineering, see docs/ppu-reverse-engineering.md. This tool implements the
 * resulting recipe and the bit-serial data channel.
 *
 * Boot recipe:
 *   1. PPU_CLC = 0                          enable the PPU clock
 *   2. load an ARCv2 image (vector table whose entry 0 is the reset handler
 *      ADDRESS) into a fetchable memory, non-cached LMU at 0xB0000000
 *   3. PPU_VECBASE = load base
 *   4. kernel reset, then PPU_CTRL = 0x3f09 (run)
 *   5. PPU_STAT RUN bits, 0 running, 1 sleeping, 2 halted
 *
 * Data channel (see the matching ARC code in ppu/):
 *   in   the host writes operands into LMU, the core reads them coherently
 *   out  the core streams a result back one bit per round, it halts to signal a
 *        1 bit and keeps polling to signal a 0 bit, the host reads PPU_STAT and
 *        resumes the core after each halt. The shared command word carries the
 *        requested bit index.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "tcmcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* PPU controller registers. */
#define PPU_CLC       0xE9800000u
#define PPU_RST_CTRLA 0xE980000Cu
#define PPU_RST_CTRLB 0xE9800010u
#define PPU_RST_STAT  0xE9800014u
#define PPU_CTRL      0xE9800060u
#define PPU_STAT      0xE9800064u
#define PPU_VECBASE   0xE9800074u

#define CTRL_RUN      0x3f09u   /* interface clocks + run request */
#define CTRL_RESUME   0x3f0du   /* poke that resumes a halted core */

/* Shared LMU layout agreed with the ARC code. */
#define CODE_BASE     0xB0000000u   /* non-cached LMU, the core fetches from here */
#define CMD_WORD      0xB0000300u   /* host -> core: (round << 8) | bit_index */
#define INPUT_BASE    0xB0000310u   /* host -> core: operands, one word each */
#define CMD_DONE      0xFFu         /* bit index sentinel meaning "finished" */

static void w(uint64_t a, uint32_t v) { xfer(a, &v, 4, MCD_TX_AT_W); }
static uint32_t r(uint64_t a) { uint32_t v = 0; rd32_try(a, &v); return v; }
static int runbits(void) { return (int)(r(PPU_STAT) & 3u); }

/* Load an image into a fetchable memory and start the core at it. */
static int ppu_start(const uint8_t *img, long n, uint32_t base)
{
    w(PPU_CLC, 0);
    usleep(2000);
    if (tcmcd_write(base, img, (uint32_t)n) != 0) {
        fprintf(stderr, "failed to load image into PPU memory at 0x%08X\n", base);
        return -1;
    }
    w(PPU_VECBASE, base);
    w(PPU_RST_CTRLA, 1);
    w(PPU_RST_CTRLB, 1);
    for (int k = 0; k < 100000; k++)
        if ((r(PPU_RST_STAT) & 7u) == 2u) break;
    w(PPU_RST_CTRLB, 0x80000000u);   /* clear reset status */
    w(PPU_CTRL, CTRL_RUN);
    return 0;
}

/* Resume a halted core. Returns 1 once it is running again. */
static int ppu_resume(void)
{
    for (int k = 0; k < 40; k++) {
        w(PPU_CTRL, CTRL_RESUME);
        usleep(2500);
        w(PPU_CTRL, CTRL_RUN);
        usleep(1200);
        if (runbits() == 0) return 1;
    }
    return 0;
}

/* Clock one 32-bit result out of the core over the halt-signaling channel. */
static uint32_t ppu_read_result(void)
{
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        w(CMD_WORD, ((uint32_t)(i + 1) << 8) | (uint32_t)i);
        int halted = 0;
        for (int t = 0; t < 20; t++) {
            usleep(1000);
            if (runbits() == 2) { halted = 1; break; }
        }
        if (halted) {
            result |= (1u << i);
            if (!ppu_resume()) {
                fprintf(stderr, "core did not resume after bit %d\n", i);
                break;
            }
        }
    }
    w(CMD_WORD, ((uint32_t)33 << 8) | CMD_DONE);   /* tell the core we are done */
    return result;
}

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
        "  tc-ppu run  <image.bin>                 load, run, report PPU run state\n"
        "  tc-ppu call <image.bin> <A> <B> [..]    feed operands, stream a result back\n"
        "operands and the result are 32-bit hex. The image must be ARC code linked\n"
        "at 0x%08X, see ppu/ for an example and the build steps.\n", CODE_BASE);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 3) { usage(); return 2; }

    static uint8_t img[64 * 1024];
    long n = load_file(argv[2], img, sizeof img);
    if (n < 0) return 1;

    open_target(1);   /* connect, reset and halt CPU0, the PPU is poked over SRI */

    if (strcmp(argv[1], "run") == 0) {
        if (ppu_start(img, n, CODE_BASE) != 0) { tcmcd_close(); return 1; }
        usleep(50000);
        const char *st[] = { "running", "sleeping", "halted", "?" };
        printf("PPU started at 0x%08X, STAT=0x%08X (%s)\n",
               CODE_BASE, r(PPU_STAT), st[runbits()]);
    } else if (strcmp(argv[1], "call") == 0) {
        if (argc < 4) { usage(); tcmcd_close(); return 2; }
        for (int i = 3; i < argc; i++)
            w(INPUT_BASE + 4u * (uint32_t)(i - 3), (uint32_t)strtoul(argv[i], NULL, 16));
        w(CMD_WORD, 0);
        if (ppu_start(img, n, CODE_BASE) != 0) { tcmcd_close(); return 1; }
        usleep(20000);   /* let the core read its inputs and compute */
        uint32_t result = ppu_read_result();
        printf("0x%08X\n", result);
    } else {
        usage();
        tcmcd_close();
        return 2;
    }

    tcmcd_close();
    return 0;
}
