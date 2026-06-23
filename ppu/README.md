# Running code on the PPU

The TC4x PPU is a Synopsys ARC EV71, a scalar ARC core plus a wide vector DSP,
sitting next to the TriCore cores. Infineon does not document its boot or memory
publicly and the Synopsys MetaWare toolkit is the official way to program it.

This directory runs our own code on the PPU's **scalar core** with no MetaWare and
no NDA material. Everything here came from clean-room reverse engineering on an
AURIX TC4D7 Lite Kit, the full story and register-level findings are in
[`../docs/ppu-reverse-engineering.md`](../docs/ppu-reverse-engineering.md).

What works today, all over the on-board debugger:

- load and start arbitrary scalar ARC code on the PPU core
- feed it input through LMU, which the core reads coherently
- read an arbitrary-width result back through a run-state handshake

The vector DSP is not used here, that needs the MetaWare vector toolchain.

## Build the example

`sum.s` reads two operands from LMU, adds them, and streams the 32-bit result
back. It needs the stock GNU ARC assembler, on Debian and Ubuntu that is the
`binutils-arc-linux-gnu` package.

```bash
arc-linux-gnu-as -mcpu=archs sum.s -o sum.o
arc-linux-gnu-ld -e _vectors -Ttext=0xB0000000 sum.o -o sum.elf
arc-linux-gnu-objcopy -O binary sum.elf sum.bin
```

## Run it

Use the `tc-ppu` host tool (built with the other tools, see the top-level
README). It needs the DAS `tas_server` running, same as `tc-load`.

```bash
tc-ppu run  sum.bin              # load, start, report the PPU run state
tc-ppu call sum.bin 12340000 5678   # -> 0x12345678
tc-ppu call sum.bin DEAD0000 BEEF   # -> 0xDEADBEEF
tc-ppu call sum.bin 7 23            # -> 0x0000002A
```

`call` writes the operands into LMU, starts the core, then clocks the 32-bit
result out bit by bit and prints it.

## How it works

The recipe, confirmed on silicon:

1. `PPU_CLC = 0`, enable the PPU clock.
2. Load an ARCv2 image into a fetchable memory. The reset is **address-vectored**,
   so entry 0 of the vector table holds the *address* of the reset handler, not an
   instruction. The scalar core fetches from CSM or LMU, this example uses
   non-cached LMU at `0xB0000000`.
3. `PPU_VECBASE = load base`.
4. Kernel reset, then `PPU_CTRL = 0x3f09` to run.
5. `PPU_STAT` RUN bits report 0 running, 1 sleeping, 2 halted.

The data channel works around a one-way coherency. The core reads host-written
LMU coherently, so input is a plain memory write. The core's data *writes* are not
visible to the host (they sit behind a cache layer with no public control), so
output uses the run state instead. For each result bit the host asks for, the core
**halts** to signal a 1 and **keeps polling** to signal a 0, the host reads
`PPU_STAT` and resumes the core after each halt. The shared command word at
`0xB0000300` carries the requested bit index. It is bit-serial and therefore slow,
but it returns arbitrary-width results with no documentation.

To run your own computation, change the body of `_start` between the input loads
and the streaming loop, and keep the same vector-table prologue and the poll and
halt streaming convention so `tc-ppu call` can read the result.
