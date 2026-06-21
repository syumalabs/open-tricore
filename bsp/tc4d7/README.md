# bsp/tc4d7

Board-support package for the TC4D7 Lite Kit. This is where the linker scripts and startup code live so that programs built with our toolchain can run on the chip.

## Planned contents

- A linker script for the real memory map, RAM regions first, then PFLASH
- Minimal startup that sets up the context save area, the stack, and clears bss
- A small set of register headers, sourced from the iLLD TC4Dx definitions under `third_party/illd_release_tc4x`

## Bring-up order

1. A RAM-only linker script so we can load and run without touching flash
2. A blink program that toggles P03.9, the same LED the read-write demo drives, so we can confirm execution by reading `P03_IN` back
3. A flash-capable linker script once PFLASH programming works
