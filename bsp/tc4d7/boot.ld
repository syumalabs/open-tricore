/*
 * Boot linker script for the TC4D7. Code and read-only data in PFLASH at
 * 0xA0000000, the address the factory boot mode header (BMHD) jumps to on
 * reset. Writable data, bss, CSA, stack, and heap in DSPR0. Initialized data
 * has its load image in PFLASH right after the code, copied to DSPR0 by crt0.
 * This lets a flashed program run on power-up with no debugger.
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
    pflash (rx) : ORIGIN = 0xA0000000, LENGTH = 64K
    dspr0  (rw) : ORIGIN = 0x70000000, LENGTH = 0x3A000
}

SECTIONS
{
    .text 0xA0000000 : {
        KEEP(*(.text._start))
        KEEP(*(.text._cstart))
        *(.text*)
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);
        __sdata2_base = . ;
        *(.sdata2*)
    } > pflash
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
    } > dspr0 AT> pflash
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
    __heap_end = ORIGIN(dspr0) + LENGTH(dspr0) ;
    __HEAP = __heap_start ;
    __HEAP_END = __heap_end ;
}
