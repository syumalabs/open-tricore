/*
 * ppu, host-side PPU helper. See ppu.h and docs/ppu-reverse-engineering.md.
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "ppu.h"
#include "tcmcd.h"
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

/* Shared LMU layout, must match ppu/rt.inc. */
#define CMD_WORD      0xB0000300u   /* host -> core: (nonce<<16)|(word<<8)|bit */
#define INPUT_BASE    0xB0000310u   /* host -> core operands */
#define CMD_DONE_BIT  0xFFu

static void w(uint64_t a, uint32_t v) { xfer(a, &v, 4, MCD_TX_AT_W); }
static uint32_t r(uint64_t a) { uint32_t v = 0; rd32_try(a, &v); return v; }

int ppu_runstate(void) { return (int)(r(PPU_STAT) & 3u); }

int ppu_load_run(const uint8_t *image, uint32_t len)
{
    w(PPU_CLC, 0);
    usleep(2000);
    if (tcmcd_write(PPU_CODE_BASE, image, len) != 0)
        return -1;
    w(PPU_VECBASE, PPU_CODE_BASE);
    w(PPU_RST_CTRLA, 1);
    w(PPU_RST_CTRLB, 1);
    for (int k = 0; k < 100000; k++)
        if ((r(PPU_RST_STAT) & 7u) == 2u) break;
    w(PPU_RST_CTRLB, 0x80000000u);   /* clear reset status */
    w(PPU_CTRL, CTRL_RUN);
    return 0;
}

static int ppu_resume(void)
{
    for (int k = 0; k < 40; k++) {
        w(PPU_CTRL, CTRL_RESUME);
        usleep(2500);
        w(PPU_CTRL, CTRL_RUN);
        usleep(1200);
        if (ppu_runstate() == 0) return 1;
    }
    return 0;
}

/* Clock one 32-bit result word out of the core. nonce is advanced so the core
   always sees a fresh command. Returns 0 on success, -1 if a resume failed. */
static int read_word(uint32_t word, unsigned *nonce, uint32_t *out)
{
    uint32_t v = 0;
    for (int bit = 0; bit < 32; bit++) {
        w(CMD_WORD, ((*nonce)++ << 16) | (word << 8) | (uint32_t)bit);
        int halted = 0;
        for (int t = 0; t < 20; t++) {
            usleep(1000);
            if (ppu_runstate() == 2) { halted = 1; break; }
        }
        if (halted) {
            v |= (1u << bit);
            if (!ppu_resume()) return -1;
        }
    }
    *out = v;
    return 0;
}

int ppu_call(const uint8_t *image, uint32_t len,
             const uint32_t *inputs, uint32_t nin,
             uint32_t *outputs, uint32_t nout)
{
    for (uint32_t i = 0; i < nin; i++)
        w(INPUT_BASE + 4u * i, inputs[i]);
    w(CMD_WORD, 0);
    if (ppu_load_run(image, len) != 0)
        return -1;
    usleep(20000);   /* let the core read inputs and compute */

    unsigned nonce = 1;
    int rc = 0;
    for (uint32_t word = 0; word < nout; word++) {
        if (read_word(word, &nonce, &outputs[word]) != 0) { rc = -1; break; }
    }
    w(CMD_WORD, (nonce << 16) | (0xFFu << 8) | CMD_DONE_BIT);   /* tell core we are done */
    return rc;
}
