/*
 * Minimal RAM linker script for the TC4D7, CPU0.
 * Code runs from PSRAM0 (program scratchpad, executable) at 0x70100000.
 * Data, bss, and stack live in DSPR0 (data scratchpad) at 0x70000000.
 * Addresses taken from the iLLD TC4Dx memory map.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_start)

MEMORY
{
    psram0 (rx) : ORIGIN = 0x70100000, LENGTH = 64K
    dspr0  (rw) : ORIGIN = 0x70000000, LENGTH = 240K
}

SECTIONS
{
    .text : {
        KEEP(*(.text._start))
        *(.text*)
        *(.rodata*)
    } > psram0

    .data : {
        *(.data*)
    } > dspr0

    .bss : {
        *(.bss*)
        *(COMMON)
    } > dspr0

    /* Stack top for later programs that need one. The current blink does not. */
    _stack_top = ORIGIN(dspr0) + LENGTH(dspr0) - 16;
}
