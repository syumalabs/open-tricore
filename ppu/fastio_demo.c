/*
 * PPU fast shared-memory output demo (runs on the TriCore, drives the PPU).
 *
 * The PPU scalar core (ARC EV71) can return results to the TriCore through the
 * shared LMU at full bandwidth, once the ARC's master tag is granted write
 * access to the LMU region. The earlier PPU bring-up could only get results out
 * one bit at a time over a run-state handshake because the ARC's LMU writes were
 * being silently dropped, which looked like an uncontrollable cache layer but is
 * actually the LMU access-protection unit denying the ARC's tag. Granting it (the
 * same mechanism the DMA driver uses) makes ARC stores land in the LMU directly.
 *
 * This demo: grant the LMU region to all master tags, feed a 16-word input
 * vector into the shared LMU, boot the ARC kernel in fastio.s (doubles each
 * element), wait for it to finish, then read the whole 16-word result vector
 * straight out of the LMU and verify it. No handshake.
 *
 *   tricore-elf-gcc -mtc18 -O2 -nostartfiles -T bsp/tc4d7/hosted.ld \
 *     bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c ppu/fastio_demo.c -I bsp/tc4d7 \
 *     -o fastio_demo.elf
 *   tricore-elf-objcopy -O binary fastio_demo.elf fastio_demo.bin
 *   tc-load run fastio_demo.bin 0x70100000     # heartbeat = correct words (16)
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#define R(a) (*(volatile unsigned int *)(a))

/* PPU controller. */
#define PPU_CLC       0xE9800000u
#define PPU_RST_CTRLA 0xE980000Cu
#define PPU_RST_CTRLB 0xE9800010u
#define PPU_RST_STAT  0xE9800014u
#define PPU_CTRL      0xE9800060u
#define PPU_STAT      0xE9800064u
#define PPU_VECBASE   0xE9800074u

/* LMU0 access-protection (shared LMU region grant), same as the DMA driver. */
#define LMU0_MEMCON   0xFB000060u
#define LMU0_PROTRGN  0xFB000070u
#define LMU0_CFG      0xFB000300u

#define PPU_CODE_BASE 0xB0000000u   /* the ARC fetches its image from here */
#define IN_BASE       0xB0400100u   /* shared LMU, must match fastio.s */
#define OUT_BASE      0xB0400200u
#define N             16u

/* fastio.s assembled, address-vectored image linked at 0xB0000000. Regenerate
   with the build steps in fastio.s, then objdump the binary to words. */
static const unsigned int kernel[15] = {
    0xb0000004u, 0x0f80240au, 0x0100b040u, 0x0f80250au, 0x0200b040u,
    0x0400264au, 0x00001400u, 0x00002000u, 0x00001d00u, 0x01042440u,
    0x01052540u, 0x80462642u, 0xffc207e8u, 0x003f216fu, 0x00000001u,
};

/* Open the shared LMU region to every master tag (including the ARC), and
   disable the ECC-on-uninitialized fault. Must run before the buffers are
   written, the region commit does not preserve data already present. */
static void lmu_grant(void)
{
    R(LMU0_MEMCON) = 0x300u;                      /* ERRDISWE | ERRDIS */
    R(LMU0_PROTRGN) = (1u << 31) | (1u << 30);    /* define owner */
    R(LMU0_PROTRGN) = (1u << 3) | 4u;             /* -> run */
    R(LMU0_PROTRGN) = (1u << 3) | 1u;             /* -> config */
    R(LMU0_PROTRGN) = 0u;                         /* region select 0, SWEN=0 */
    R(LMU0_CFG + 0x00) = 0xFFFFFFFFu;             /* WRA */
    R(LMU0_CFG + 0x04) = 0xFFFFFFFFu;             /* WRB */
    R(LMU0_CFG + 0x08) = 0xFFFFFFFFu;             /* RDA */
    R(LMU0_CFG + 0x0C) = 0xFFFFFFFFu;             /* RDB */
    R(LMU0_CFG + 0x10) = 0xFFFFFFFFu;             /* VM  */
    R(LMU0_CFG + 0x14) = 0xFFFFFFFFu;             /* PRS */
    R(LMU0_CFG + 0x18) = 0x90400000u;             /* RGNLA */
    R(LMU0_CFG + 0x1C) = 0x90480000u;             /* RGNUA */
    R(LMU0_PROTRGN) = (1u << 3) | 4u;             /* commit */
}

/* Load the ARC image and start the core, the boot recipe from the PPU bring-up. */
static void ppu_boot(void)
{
    for (unsigned i = 0; i < sizeof(kernel) / 4u; i++)
        R(PPU_CODE_BASE + i * 4u) = kernel[i];
    R(PPU_CLC) = 0;
    for (volatile int i = 0; i < 20000; i++) { }
    R(PPU_VECBASE) = PPU_CODE_BASE;
    R(PPU_RST_CTRLA) = 1u;
    R(PPU_RST_CTRLB) = 1u;
    for (int k = 0; k < 100000; k++)
        if ((R(PPU_RST_STAT) & 7u) == 2u) break;  /* kernel reset done */
    R(PPU_RST_CTRLB) = 0x80000000u;               /* clear reset status */
    R(PPU_CTRL) = 0x3f09u;                         /* interface clocks + run */
}

int main(void)
{
    lmu_grant();

    for (unsigned i = 0; i < N; i++) {
        R(IN_BASE + i * 4u) = i + 1u;             /* inputs 1..16 */
        R(OUT_BASE + i * 4u) = 0u;
    }

    ppu_boot();
    for (int t = 0; t < 4000000; t++)
        if ((R(PPU_STAT) & 3u) == 1u) break;      /* wait for the core to sleep */

    unsigned ok = 0;
    for (unsigned i = 0; i < N; i++)
        if (R(OUT_BASE + i * 4u) == (i + 1u) * 2u) ok++;

    R(0x70000004u) = R(OUT_BASE);                  /* out[0] (want 2) */
    R(0x7000000Cu) = R(PPU_STAT);                  /* PPU run state (sleep = 0x2C1) */
    for (;;)
        R(0x70000000u) = ok;                       /* heartbeat = correct words (want 16) */
}
