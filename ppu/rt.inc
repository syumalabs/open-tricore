/*
 * rt.inc, shared runtime for PPU scalar-core examples (ARC EV71).
 *
 * Provides the ARCv2 vector table and the result-streaming loop. An example
 * just defines `compute`, which reads its inputs from LMU and writes its result
 * words to the RESULT scratch, then returns. The host (tc-ppu / ppu_call) reads
 * the result words back over the halt-signaling channel.
 *
 * Protocol, the host writes a command token to CMD:
 *   bits  [7:0]   bit index, 0..31, or 0xFF meaning "done"
 *   bits [15:8]   result word index
 *   bits [31:16]  nonce, changes every request so the core sees a new command
 * For the requested (word, bit) the core halts to signal a 1 bit and keeps
 * polling to signal a 0 bit. See docs/ppu-reverse-engineering.md.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

    .equ CMD,    0xB0000300      /* host -> core command token */
    .equ RESULT, 0x30000400      /* core-private scratch for result words */

    .text
    .global _vectors
_vectors:
    .rept 8
    .word _start                 /* address-vectored reset, entry 0 is _start */
    .endr

_start:
    bl   compute                 /* example fills RESULT[0..] */
    mov  r11, CMD
    mov  r9,  RESULT
    mov  r12, 0                  /* last token seen */
.Lpoll:
    ld   r0, [r11]
    cmp  r0, r12
    beq  .Lpoll
    mov  r12, r0
    and  r1, r0, 0xFF            /* bit index */
    cmp  r1, 0xFF
    beq  .Ldone
    lsr  r6, r0, 8
    and  r6, r6, 0xFF            /* result word index */
    ld.as r7, [r9, r6]          /* RESULT[word], address scaled by 4 */
    lsr  r2, r7, r1            /* result word >> bit */
    and  r2, r2, 1
    cmp  r2, 0
    beq  .Lpoll                  /* bit 0, keep polling */
    flag 1                       /* bit 1, halt; host resumes us */
    b    .Lpoll
.Ldone:
    flag 1
    b    .Ldone
