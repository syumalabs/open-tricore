# toolchain

Build scripts for an open-source `tricore-elf-gcc` toolchain (GCC, binutils, newlib) that runs on Linux and targets the TriCore cores.

## Plan

TriCore 1.8 in the TC4x cores is backward compatible with the base 1.6.x ISA, so code from an open-source 1.6.x GCC is expected to run on the TC4D7. We validate that claim on real silicon by loading a small program into RAM and running it, rather than assuming it.

Steps.

1. Build GCC, binutils, and newlib from the GPL TriCore sources. The community `free_tricore_toolchain` automates this.
2. Add a TC4D7 board-support package, see `bsp/tc4d7`.
3. Compile a tiny program, load it into RAM over TAS, run it with MCD run-control, and confirm behavior by reading registers back.
4. Move on to PFLASH programming for persistence.

## Caveats

- The build output targets the 1.6.x ISA subset. TC4x-only instructions are not emitted.
- The TC4x PPU vector unit is a different ISA and is not a target of this toolchain.
- This toolchain is GPL under its own license. It is isolated here and does not affect the license of the rest of the repo.
