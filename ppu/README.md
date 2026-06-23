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
- read results back two ways, directly out of the shared LMU at full bandwidth
  (after granting the core's master tag, see below), or, with no grant, an
  arbitrary-width result vector over a run-state handshake

The vector DSP is not used here, that needs the MetaWare vector toolchain.

## Files

- `rt.inc`, the shared runtime, the ARCv2 vector table and the result-streaming
  loop. An example only provides `compute`.
- `sum.s`, adds two operands, returns one word.
- `checksum.s`, sums a host-provided buffer, returns one word, exercises the
  input channel with a variable-length buffer.
- `fastio.s`, a compute kernel that returns a result vector through the shared
  LMU directly, no handshake. Reads 16 words, doubles them, writes 16 back.
- `fastio_demo.c`, a self-contained TriCore program that grants the LMU, boots
  `fastio.s`, feeds inputs, and reads the result vector straight out of the LMU.

## Build and run

The examples need the stock GNU ARC assembler, on Debian and Ubuntu that is the
`binutils-arc-linux-gnu` package. Build from this directory so the `.include`
finds `rt.inc`:

```bash
arc-linux-gnu-as -mcpu=archs sum.s -o sum.o
arc-linux-gnu-ld -e _vectors -Ttext=0xB0000000 sum.o -o sum.elf
arc-linux-gnu-objcopy -O binary sum.elf sum.bin
```

Run with the `tc-ppu` host tool (built with the other tools, see the top-level
README). It needs the DAS `tas_server` running, same as `tc-load`.

```bash
tc-ppu run  sum.bin                       # load, start, report the PPU run state
tc-ppu call sum.bin 12340000 5678         # -> 0x12345678
tc-ppu call checksum.bin 4 1 2 3 4        # count then words, -> 0x0000000A
tc-ppu call <image> --out 3 <inputs..>    # read a 3-word result vector
```

`call` writes the operands into LMU, starts the core, then clocks the result
words out one bit at a time and prints them. The same flow is available to C
programs through the `ppu_call` helper in
[`../tools/common/ppu.h`](../tools/common/ppu.h).

## How it works

The recipe, confirmed on silicon:

1. `PPU_CLC = 0`, enable the PPU clock.
2. Load an ARCv2 image into a fetchable memory. The reset is **address-vectored**,
   so entry 0 of the vector table holds the *address* of the reset handler, not an
   instruction. The scalar core fetches from CSM or LMU, these examples use
   non-cached LMU at `0xB0000000`.
3. `PPU_VECBASE = load base`.
4. Kernel reset, then `PPU_CTRL = 0x3f09` to run.
5. `PPU_STAT` RUN bits report 0 running, 1 sleeping, 2 halted.

Input is a plain memory write, the core reads host-written LMU coherently. For
output there are two paths.

**Direct, full bandwidth (`fastio.s` / `fastio_demo.c`).** The core writes its
results straight into the shared LMU and the TriCore reads them back like any
memory. This needs one setup step: out of reset the LMU access-protection unit
lets every master *read* but only the CPU *write*, so an ARC store to the LMU is
silently dropped. Granting the ARC's master tag write access to the LMU region
(the same LMU APU mechanism the DMA driver uses) makes the stores land. The
earlier bring-up read this drop as an uncontrollable cache layer, it is an
access-protection drop. `fastio_demo.c` does the grant, boots the kernel, and
reads the result vector with no handshake.

**Run-state handshake (`rt.inc`, no grant needed).** Without the LMU grant the
core's writes are invisible, so output rides the run state. The host posts a
command word at `0xB0000300` naming a result word and bit, the core **halts** to
signal a 1 and **keeps polling** to signal a 0, and the host resumes the core
after each halt. Bit-serial and slow, but it returns arbitrary-width results and
needs no setup. To write a handshake kernel, copy an example and replace
`compute`, read inputs from `0xB0000310`, write result words to the `RESULT`
scratch, and return, then `tc-ppu call --out N` reads N of them.
