/*
 * checksum.s, PPU scalar-core example, sums a buffer the host provides.
 *
 * The host writes a word count followed by that many words into LMU, the core
 * reads the whole buffer coherently and returns their 32-bit sum. This exercises
 * the input channel with a variable-length buffer, not just two operands.
 *
 *   arc-linux-gnu-as -mcpu=archs checksum.s -o checksum.o
 *   arc-linux-gnu-ld -e _vectors -Ttext=0xB0000000 checksum.o -o checksum.elf
 *   arc-linux-gnu-objcopy -O binary checksum.elf checksum.bin
 *   tc-ppu call checksum.bin 4 1 2 3 4        # count then words, -> 0x0000000A
 *
 * The first operand is the count, the rest are the buffer. The vector table and
 * the result-streaming loop come from rt.inc.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

    .include "rt.inc"

    .equ INPUTS, 0xB0000310      /* host -> core: count, then the buffer words */

    .text
    .global compute
compute:
    mov  r3, INPUTS
    ld   r4, [r3]                /* count */
    add  r3, r3, 4              /* buffer start */
    mov  r5, 0                  /* running sum */
.Lsum:
    cmp  r4, 0
    beq  .Lend
    ld   r6, [r3]
    add  r5, r5, r6
    add  r3, r3, 4
    sub  r4, r4, 1
    b    .Lsum
.Lend:
    mov  r8, RESULT
    st   r5, [r8]               /* RESULT[0] = sum */
    j    [blink]
