/*
 * RAM-data, flash-code linker script for the TC4D7.
 * Code runs from PFLASH at 0xA0300000, a safe sector well clear of the boot
 * bank. Data, bss, and stack live in DSPR0 at 0x70000000.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_start)

MEMORY
{
    pflash (rx) : ORIGIN = 0xA0300000, LENGTH = 16K
    dspr0  (rw) : ORIGIN = 0x70000000, LENGTH = 240K
}

SECTIONS
{
    .text : {
        KEEP(*(.text._start))
        *(.text*)
        *(.rodata*)
    } > pflash

    .data : { *(.data*) } > dspr0
    .bss  : { *(.bss*) *(COMMON) } > dspr0

    _stack_top = ORIGIN(dspr0) + LENGTH(dspr0) - 16;
}
