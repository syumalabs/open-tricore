/*
 * sum.s, an example program for the AURIX TC4x PPU scalar core (ARC EV71).
 *
 * It reads two 32-bit operands the host placed in LMU, adds them, then streams
 * the 32-bit result back to the host one bit at a time over the halt-signaling
 * channel. Run it with the tc-ppu host tool:
 *
 *   arc-linux-gnu-as -mcpu=archs sum.s -o sum.o
 *   arc-linux-gnu-ld -e _vectors -Ttext=0xB0000000 sum.o -o sum.elf
 *   arc-linux-gnu-objcopy -O binary sum.elf sum.bin
 *   tc-ppu call sum.bin 12340000 5678      # prints 0x12345678
 *
 * Why it is shaped this way (all clean-room, see docs/ppu-reverse-engineering.md):
 *   - The reset is ARCv2 address-vectored, so entry 0 of the vector table holds
 *     the ADDRESS of the reset handler, not an instruction.
 *   - The core fetches from CSM or LMU, here non-cached LMU at 0xB0000000.
 *   - The core reads host-written LMU coherently (the input direction), but its
 *     data writes are not visible to the host, so output uses the run state, the
 *     core halts to send a 1 bit and keeps polling to send a 0 bit.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

    .equ CMD,    0xB0000300      /* host -> core: (round << 8) | bit_index */
    .equ INPUTS, 0xB0000310      /* host -> core: operand A, then operand B */

    .text
    .global _vectors
_vectors:
    .rept 8
    .word _start                 /* reset vector and the first few entries */
    .endr

_start:
    mov  r11, CMD
    mov  r3,  INPUTS
    ld   r4,  [r3]               /* A */
    ld   r5,  [r3, 4]            /* B */
    add  r10, r4, r5             /* result = A + B */
    mov  r12, 0                  /* last command token seen */

poll:
    ld   r0, [r11]
    cmp  r0, r12
    beq  poll                    /* spin until the host posts a new token */
    mov  r12, r0
    and  r1, r0, 0xFF            /* bit index requested */
    cmp  r1, 0xFF
    beq  done                    /* sentinel, host is finished */
    lsr  r2, r10, r1            /* result >> index */
    and  r2, r2, 1
    cmp  r2, 0
    beq  poll                    /* bit is 0, keep polling (host reads "running") */
    flag 1                       /* bit is 1, halt (host reads "halted", then resumes) */
    b    poll

done:
    flag 1
    b    done
