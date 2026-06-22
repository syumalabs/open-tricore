# PPU reverse engineering notes

The TC4x PPU (Parallel Processing Unit) is a Synopsys ARC EV71 vision and AI
processor, a scalar ARC core plus a wide vector DSP, sitting next to the TriCore
cores. Infineon does not document its boot or internal memory in the public user
manual (it refers to a separate restricted PPU chapter), and the Synopsys
MetaWare toolkit is the official way to program it.

These notes are the result of clean-room reverse engineering on our own
hardware, an AURIX TC4D7 Lite Kit, observed entirely through the on-board DAP
debugger over MCD. Every address and sequence here was confirmed by reading and
writing the live silicon, nothing is taken from restricted documentation. They
are incomplete, getting our own code to execute on the scalar core is still
open, see the last section.

All register and memory access below is from the TriCore side over the debugger.

## Identity and state out of reset

- PPU controller (PPUC) base `0xE9800000`.
- `PPU_ID` at `0xE9800008` reads `0x00EFC001`.
- Out of reset the PPU is clock-gated, `PPU_CLC` (`0xE9800000`) reads `0x3`
  (disable request and disable status set). The deeper control registers fault
  until the clock is enabled.

## Control plane

Verified register map (PPUC block at `0xE9800000`):

| Register | Address | Notes |
|---|---|---|
| CLC      | 0xE9800000 | write 0 to enable the module clock |
| ID       | 0xE9800008 | reads 0x00EFC001 |
| RST_CTRLA| 0xE980000C | bit0 KRST (kernel reset) |
| RST_CTRLB| 0xE9800010 | bit0 KRST, bit31 STATCLR |
| RST_STAT | 0xE9800014 | bits[2:0] KRST status, wait for value 2 |
| CTRL     | 0xE9800060 | bit0 REQR (run), bit3 + bits[13:8] interface enables |
| STAT     | 0xE9800064 | bits[1:0] RUN, 0 running, 1 sleeping, 2 halted |
| VECBASE  | 0xE9800074 | vector base, written before reset |

Boot sequence that drives the core to its run state (mirrors the reference
flow):

1. `CLC = 0` and wait until it reads 0. Enables the clock, the control registers
   become accessible. After enable, `CTRL` reads `0x3f08`, all six interface
   clocks (bits 8-13) and the irq interface (bit 3) are already on, only the run
   request (bit 0) is clear. `STAT` reads `0x2c2`, RUN bits = 2 (halted).
2. `VECBASE = <address>`.
3. Kernel reset: `RST_CTRLA = 1`, `RST_CTRLB = 1`, poll `RST_STAT & 7 == 2`,
   then `RST_CTRLB = 0x80000000` (STATCLR).
4. `CTRL = 0x3f09` (interfaces plus run request). `STAT` RUN bits go from 2 to 0.

Memory contents survive the kernel reset.

## Memory map (verified on silicon)

| Memory | System address (TriCore/SRI) | Notes |
|---|---|---|
| VMEM (vector memory) | 0xB2040000, 128 KB | also readable/writable directly from the debugger after clock enable |
| VMEM, PPU-internal view | 0xC0000000 | the STU and ARC core see VMEM here |
| CSM (cluster shared memory) | 0x92080000 (alias 0xB2080000), 512 KB | not reachable by direct SRI writes, only via the STU |

Two facts cost a lot of time and are worth stating plainly:

- **The PPU-internal segment-C view of VMEM (`0xC0000000`) is not the same as the
  TriCore's segment-C address.** From the debugger, `0xC0000000` and the VMEM
  system address `0xB2040000` hold independent values, the segment-C mapping to
  VMEM applies only to accesses from inside the PPU (the STU and the ARC core).
  To load VMEM from the debugger, write the system alias `0xB2040000`.

- **PPU memory has ECC and faults on reads of uninitialized or partially written
  locations.** A single 4-byte write into a fresh region can still fault on
  read-back, you must initialize with contiguous full-block writes. This is why
  CSM faults wholesale (never initialized) and why sparse writes misbehave.

## The STU, a working code loader

The Streaming Transfer Unit (STU) is a DMA engine inside the PPU. It can be
driven by an external master (the TriCore) and, being internal to the PPU, it
can write CSM even though direct SRI writes to CSM fault. Writing CSM through the
STU also ECC-initializes it, after which CSM reads back normally.

STU registers (STUDMI block at `0xF9810000`):

| Register | Address | Use |
|---|---|---|
| BUILD       | 0xF9810000 | reads 0x00121004 |
| ENTRY_NUM   | 0xF9810004 | max channels, write 0 means 16 channels |
| NEXT_FREE   | 0xF9810008 | next free channel index |
| NEXT_FREE_INC | 0xF9810010 | write N to start N queued transfers |
| ENTRY_SELECT| 0xF9810014 | select a channel to query |
| ENTRY_STAT  | 0xF9810018 | bit2 done, bit3 error |
| BASE_L      | 0xF981001C | descriptor base, in the PPU-internal view |
| EVENT       | 0xF9810028 | write 0x80000000 to clear all events |

The descriptor is a 384-byte block in memory, six arrays of 16 entries (4 bytes
each), in order Ctrl, Size, Src, Dst, Width, Stride. For channel 0 the fields
are at byte offsets 0, 64, 128, 192. For a system-to-system linear copy:

- Ctrl = 0
- Size = transfer_bytes - 1
- Src  = source address in the PPU-internal view
- Dst  = destination address

Working sequence (verified, VMEM to VMEM and VMEM to CSM):

1. Put the source bytes in VMEM, contiguous, at system `0xB2040000` (the STU
   sees this at internal `0xC0000000`).
2. Build the full 384-byte descriptor as one contiguous block and write it to
   VMEM, for example at system `0xB2050000` (STU view `0xC0010000`). Set Src to
   `0xC0000000`, Dst to `0x92080000` for a copy into CSM.
3. `ENTRY_NUM = 0`, `BASE_L = 0xC0010000`.
4. read `NEXT_FREE` (channel), clear events, `NEXT_FREE_INC = 1`.
5. poll `ENTRY_SELECT = channel` then `ENTRY_STAT` bit 2 (done).

After this, CSM at `0x92080000` reads back the copied bytes. We use this to load
ARC code into CSM.

## What still does not work

With ARC code verifiably loaded into CSM and the boot sequence driving the core
to RUN, the scalar core does not fetch and execute our code, no observable
effect. The blocking unknowns:

- The scalar core's exact reset program counter. `VECBASE` changes the core
  state (`STAT` differs for different values) but does not start our code.
- The scalar core's own address view of CSM, which may differ from the STU and
  system view `0x92080000`.
- Whether the assembler target (`-mcpu=archs`) exactly matches ARC EV71, our
  instructions could decode differently on the real core.

These cannot be separated without observing the ARC program counter, and MCD
does not expose the PPU core while the PPU DEBUG block (`0xF9820000`) has no PC
readout. The remaining work is to determine the reset entry and the core's CSM
view, either by further reverse engineering or from the ARC EV71 ISA and the
MetaWare linker layout.

## Tooling

All experiments were driven from small host programs linking the shared `tcmcd`
MCD layer, the same backend as `tc-load`. The ARC code was assembled with the
stock `binutils-arc-linux-gnu` package (`arc-linux-gnu-as`).
