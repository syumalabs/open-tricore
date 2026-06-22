/*
 * Hosted RAM linker script for the TC4D7, CPU0. Code and read-only data in
 * PSRAM0 at 0x70100000. Writable data, bss, CSA, stack, and heap in DSPR0 at
 * 0x70000000. Initialized data has its load image in PSRAM0 right after the
 * code, which crt0 copies into DSPR0 at startup. A 4K printf capture buffer
 * sits at the top of DSPR0 (0x7003A000).
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

OUTPUT_FORMAT("elf32-tricore")
OUTPUT_ARCH(tricore)
ENTRY(_start)

__USTACK_SIZE = 0x2000;  /* 8K */
__CSA_SIZE    = 0x2000;  /* 8K, 128 contexts */

MEMORY
{
    psram0 (rx) : ORIGIN = 0x70100000, LENGTH = 64K
    dspr0  (rw) : ORIGIN = 0x70000000, LENGTH = 0x3A000  /* 232K, top 24K reserved */
}

SECTIONS
{
    .text 0x70100000 : {
        KEEP(*(.text._start))
        KEEP(*(.text._cstart))
        *(.text*)
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);
        __sdata2_base = . ;
        *(.sdata2*)
    } > psram0
    _SMALL_DATA2_ = __sdata2_base ;

    .data : {
        . = ALIGN(4);
        __data_start = . ;
        *(.data*)
        . = ALIGN(4);
        _SMALL_DATA_ = . ;
        *(.sdata*)
        . = ALIGN(4);
        __data_end = . ;
    } > dspr0 AT> psram0
    __data_load = LOADADDR(.data) ;

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = . ;
        *(.sbss*)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = . ;
    } > dspr0

    . = ALIGN(64);
    __csa_begin = . ;
    . += __CSA_SIZE ;
    __csa_end = . ;

    . = ALIGN(8);
    . += __USTACK_SIZE ;
    __stack_top = . ;

    . = ALIGN(8);
    __heap_start = . ;
    __heap_end = ORIGIN(dspr0) + LENGTH(dspr0) ;  /* up to 0x7003A000 */

    /* heap symbols the toolchain libos _sbrk expects */
    __HEAP = __heap_start ;
    __HEAP_END = __heap_end ;
}
