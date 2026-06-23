# PPU reverse engineering notes

The TC4x PPU (Parallel Processing Unit) is a Synopsys ARC EV71 vision and AI
processor, a scalar ARC core plus a wide vector DSP, sitting next to the TriCore
cores. Infineon does not document its boot or internal memory in the public user
manual (it refers to a separate restricted PPU chapter), and the Synopsys
MetaWare toolkit is the official way to program it.

These notes are the result of clean-room reverse engineering on our own
hardware, an AURIX TC4D7 Lite Kit, observed entirely through the on-board DAP
debugger over MCD. Every address and sequence here was confirmed by reading and
writing the live silicon, nothing is taken from restricted documentation.

**Update, the scalar core now runs our own code.** We can load ARC code, start
the core, and watch it execute a real computation correctly. See the breakthrough
section at the end. The middle sections record the journey and the dead ends, the
recipe is at the bottom.

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

These cannot be separated without observing the ARC program counter.

## The ARC debug port (found, not yet activated)

The DEBUG block at `0xF9820000` is the ARC OCDS host interface, a command and
status port that maps onto the standard ARC debug transactions:

| Register | Address | Fields |
|---|---|---|
| DB_STATUS | 0xF9820000 | bit0 ST stalled, bit1 FL fail, bit2 RD ready, bit4 RU ARC running, bit5 RA reset applied |
| DB_CMD    | 0xF9820004 | bits[3:0] command |
| DB_ADDR   | 0xF9820008 | aux/core/memory address |
| DB_DATA   | 0xF982000C | data |
| DB_RESET  | 0xF9820010 | system reset request |

The 4-bit command uses the standard ARC encoding (confirmed against OpenOCD's
ARC target), 0 write mem, 1 write core reg, 2 write aux reg, 3 NOP, 4 read mem,
5 read core reg, 6 read aux reg. Relevant ARC aux registers, PC at aux `0x6`,
STATUS32 at aux `0xA` (halt is bit 0), DEBUG at aux `0x5` (force halt), IDENTITY
at aux `0x4`. So in principle this port reads the ARC PC and registers and can
halt and single step the core, exactly the visibility we lack.

Writes to the DEBUG block are gated by the PPU Access Protection Unit, the entry
is `PPU_AP_ACCEN_DEBUG_WRA` at `0xF9840060` (open it to 0xFFFFFFFF). But even with
that open, the port does not yet accept transactions, `DB_STATUS` reflects the
run state (RU) and clears `RU` when the core is halted via `CTRL.REQH`, but the
ready bit never sets and command writes have no visible effect. The port appears
to need a further OCDS level activation, or the ARC must be put into true debug
mode rather than the CTRL halt (which sleeps or clock-halts the core). Cracking
that activation is the highest-value next step, it would give full ARC visibility
(PC, registers, single step) and almost certainly resolve the reset entry and
CSM view questions, turning this into a working PPU debugger.

The remaining work is either to activate this debug port, or to obtain the reset
entry and the core's CSM view from the ARC EV71 documentation and the MetaWare
linker layout.

## Update, exhaustive bring-up attempts and the convergent wall

After extensive further work, including online research across the ARCv2 ISA,
OpenOCD, embARC, and the public Infineon AURIX code examples, the boot mechanism
is now fully understood and matches the shipped iLLD `IfxPpucCore_configureCoreAndRun`,
CLC, VECBASE, kernel reset, then `CTRL = 0x3f09`. Two corrections to earlier
assumptions were applied and tested:

- The ARCv2 reset is address vectored, the first vector table entry holds the
  address of the reset handler, the core reads that word and jumps. We build a
  proper vector table (entry 0 = handler address) rather than placing
  instructions at the base. Confirmed correct against Linux, embARC, and OpenOCD.
- PPU code should be loaded through the non cached segment alias to avoid a stale
  instruction cache.

With those corrections the scalar core still does not execute. We then swept
VECBASE across every plausible value (system view, the PPU side `0xA0000000`
view, segment bases `0x0`, `0x9/0xA/0xB/0xC/0xD/0x80000000`), with a program that
stores a marker to every candidate address view, and scanned all readable PPU
memory afterward. No execution was observed for any combination.

Three independent gates converge on the one missing fact, the ARC core's own
address view of CSM (where VECBASE must point and where the loaded image must
land):

1. It is not in any public source. The ARCv2 mechanism is public, but the EV71
   specific CSM and CCM base addresses are in the restricted reference manual and
   the Synopsys EV71 databook.
2. It is not observable. The ARC debug port that would reveal the PC is owned by
   the OCDS, software writes to it bus error (an on chip CPU write traps), so the
   PC cannot be read.
3. It is not configurable from the TriCore side. The CSMAP and VMEMAP blocks are
   access protection only, the ARC memory view is set by an ARC side aux register
   (`AUX_VECMEM_REGION`, aux `0x544`) reachable only by code already running on
   the core.

The raw JTAG route was also evaluated and is a dead end for this part, public
OpenOCD has no ARCv3 or EV backend, and the PPU is not a separately scannable
JTAG TAP, it sits behind the chip level Cerberus or MCD fabric.

A conclusive diagnostic settles it. `VECBASE` has a strap default of `0x92080000`
(the CSM system address) and our writes to it persist across the kernel reset, so
the register works and the silicon's intended reset location is CSM at
`0x92080000`. We placed a single ARC `sleep` instruction directly at that exact
address (verified, CSM reads back `0x003f216f`), set `VECBASE = 0x92080000`, and
ran. If the core fetched and executed that instruction it would enter the sleep
run state, observable in `STAT` without needing any store. It does not, `STAT`
goes from halt to run and stays running. So the scalar core's instruction fetch
does not come from the system CSM we can write, it fetches a physically different
memory (its own ICCM, or a cached private view of `0x92080000`) that we cannot
write or observe from the TriCore side. That is the wall, proven rather than
inferred.

That earlier conclusion was wrong, and the reason it was wrong is instructive,
see the breakthrough below.

## Breakthrough, running our code on the scalar core

The wall above was a measurement error, not a hardware limit. Every prior attempt
detected execution by having the loaded code do a memory store and then reading
that store back. The scalar core's data accesses go to its own private memory
(DCCM), which has no TriCore visible alias, so the stores were invisible even
though the core was very likely executing the whole time. The fix was to detect
execution a different way.

The key idea, use the run state as a view independent output. The ARC `sleep`
instruction puts the core into the sleep state, visible in `PPU_STAT` RUN bits
(`0x...c1`), with no memory access required. Place a `sleep` at the entry, run,
and read `STAT`. If it sleeps, the core executed our code.

Two more facts were needed, both found by sweeping writable memory and watching
`STAT`:

- The reset is ARCv2 address vectored. The vector table entry 0 must hold the
  ADDRESS of the entry handler, the core reads that word and jumps. Placing
  instructions directly at the base does not work, only the address vector form.
- The scalar core fetches from CSM and LMU, not from the vector memory. Loading
  into VMEM/VCCM (`0xC0000000`, `0xD0000000`) never executes, those are vector
  data memory. CSM (`0x92080000` or non cached `0xB2080000`) and LMU
  (`0x90000000` cached, `0xB0000000` non cached) all execute.

Working recipe (verified on silicon):

1. `PPU_CLC = 0`, enable the clock.
2. Build an ARC image whose first words are a vector table, entry 0 = the address
   of the entry point, then the code. Link it at the load base.
3. Load the image into a fetchable memory. Non cached LMU `0xB0000000` is the
   easiest, the debugger can write it directly. CSM works too, write it directly
   now that it is ECC initialized, or stream it with the STU.
4. `PPU_VECBASE = load base`. The strap default is `0x92080000` (CSM).
5. Kernel reset, `RST_CTRLA = 1`, `RST_CTRLB = 1`, wait `RST_STAT.KRST == 2`,
   then `RST_CTRLB.STATCLR = 1`.
6. `PPU_CTRL = 0x3f09`, run.
7. Observe `PPU_STAT` RUN bits, 0 running, 1 sleeping, 2 halted.

Proof it is real computation, not just reaching a `sleep`. We ran a loop that
sums 1 through 10 in registers, compares the result, and sleeps only on a match.
With the compare value 55 (the correct sum) the core sleeps. With the compare
value 99 it keeps running. So the core executed the loop, the adds, the compare,
and the conditional branch, and produced the right answer. That is our own code
running on the ARC EV71 scalar core.

What is still open. Getting data back out to the TriCore through shared memory
needs the ARC side address of CSM for data accesses (its data view differs from
its fetch view, and plain stores land in private DCCM). The run state channel
(sleep, halt, run, and timing) is enough to prove execution and to build simple
result reporting, richer shared memory output is the next step. The OCDS owned
debug port and the NDA memory map remain the only way to read the ARC PC and the
private data map directly.

## Tooling

All experiments were driven from small host programs linking the shared `tcmcd`
MCD layer, the same backend as `tc-load`. The ARC code was assembled with the
stock `binutils-arc-linux-gnu` package (`arc-linux-gnu-as`).
