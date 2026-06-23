/*
 * sum.s, PPU scalar-core example, adds two operands and returns the sum.
 *
 * The host places two 32-bit operands in LMU, the core adds them and returns one
 * result word. Build and run:
 *
 *   arc-linux-gnu-as -mcpu=archs sum.s -o sum.o
 *   arc-linux-gnu-ld -e _vectors -Ttext=0xB0000000 sum.o -o sum.elf
 *   arc-linux-gnu-objcopy -O binary sum.elf sum.bin
 *   tc-ppu call sum.bin 12340000 5678      # -> 0x12345678
 *
 * The vector table and the result-streaming loop come from rt.inc, this file
 * only provides `compute`. See ppu/README.md and docs/ppu-reverse-engineering.md.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

    .include "rt.inc"

    .equ INPUTS, 0xB0000310      /* host -> core operands */

    .text
    .global compute
compute:
    mov  r3, INPUTS
    ld   r4, [r3]                /* A */
    ld   r5, [r3, 4]            /* B */
    add  r4, r4, r5             /* A + B */
    mov  r8, RESULT
    st   r4, [r8]               /* RESULT[0] = sum */
    j    [blink]
