# tc-gdbserver design

A GDB remote stub for the AURIX TC4D7. It speaks the GDB Remote Serial Protocol
(RSP) over TCP to a TriCore GDB on one side, and drives the target through the
MCD API (the same libmcdxdas and tas_server path that tc-load already uses) on
the other. The result is source-level debugging on Linux, breakpoints, single
step, register and memory inspection, and stack unwinding, for code running from
RAM or flash on real silicon.

This document is the plan. No gdbserver code exists yet.

## Goals and scope

Version 1 targets the common debug loop on one core.

In scope for v1
- Connect a TriCore GDB to a running or halted target over TCP
- Read and write all 44 core registers GDB knows about
- Read and write memory (RAM, flash view, peripherals)
- Continue, single step, and asynchronous stop (Ctrl-C)
- Hardware breakpoints via MCD IP triggers
- Correct stop replies so GDB shows the right line and reason
- Attach to core 0, report a single thread

Out of scope for v1, listed so the design leaves room for them
- Multi-core (the TC4D7 has six cores), exposed later as GDB threads
- Watchpoints (data triggers), the MCD support is there, wire up after breakpoints
- Flashing through GDB `load` (vFlash packets driving our flash-prog)
- RTOS thread awareness

## Architecture

```
  tricore-elf-gdb  <--- TCP (RSP) --->  tc-gdbserver  <--- MCD --->  tas_server  <--- USB DAP --->  TC4D7
```

tc-gdbserver is a single process. It opens the MCD connection once (reset and
halt, reusing the tc-load sequence), then listens on a TCP port (default 3333,
configurable). When GDB connects it serves one session, translating RSP packets
to MCD calls and back, until GDB detaches or disconnects.

Reuse. tc-load's `main.c` already contains the MCD connection and access
primitives we need, `open_target`, `xfer` (memory read and write via
mcd_execute_txlist), `find_pc`, `set_reg`, `rd32`, `wr32`. Rather than duplicate
them, factor the MCD glue into a small shared unit, `tools/common/tcmcd.{c,h}`,
linked by both tc-load and tc-gdbserver. This keeps one copy of the connection,
reset, and transfer logic.

Layout
```
tools/common/tcmcd.c        shared MCD connect, memory, register, run-control helpers
tools/common/tcmcd.h
tools/tc-gdbserver/main.c    TCP server, RSP parser, packet dispatch
tools/tc-gdbserver/rsp.c     RSP framing (checksums, acks, escaping), helpers
```

## Dependency, build the TriCore GDB

The toolchain source tree already contains GDB with TriCore support at
`toolchain/tricore-gcc-toolchain/tricore-binutils-gdb` (gdb/tricore-tdep.c and
the features xml), but only gcc and binutils were installed, there is no
`tricore-elf-gdb` in `toolchain/install/bin`. So step zero is to build and
install the GDB client. This is a configure and make in the existing tree with
`--target=tricore-elf`, producing `tricore-elf-gdb`. It is the reference client
we develop and test the stub against.

## RSP protocol surface

Packets are framed as `$<payload>#<checksum>` with `+` and `-` acknowledgements.
An out of band `0x03` byte is the interrupt request. We implement in phases.

Phase 1, handshake and inspection (target stays halted)
- `qSupported` reply `PacketSize=4000;hwbreak+;qXfer:features:read+;swbreak+`
- `?` report last stop signal, `S05` to start
- `g` and `G`, read and write all registers
- `p n` and `P n=v`, read and write one register
- `m addr,len` and `M addr,len:data`, read and write memory
- `qXfer:features:read:target.xml`, serve the TriCore target description so GDB
  knows the register set even before reading tricore-tdep, sourced from
  gdb/features/tricore.xml
- housekeeping, `Hg`, `Hc`, `qC`, `qAttached` (reply 1), `vMustReplyEmpty`,
  `qfThreadInfo` and `qsThreadInfo` (one thread, `m1` then `l`), `T` thread alive

Phase 2, execution
- `c` continue, `s` step one instruction
- `vCont?` reply `vCont;c;C;s;S`, then `vCont;c` and `vCont;s` as the modern path
- interrupt `0x03`, call mcd_stop, then send the stop reply
- stop replies, `T05` with `thread:1;` and a reason key, `hwbreak:` for a
  breakpoint hit, `swbreak:` if we placed a RAM trap, plain `S05` otherwise

Phase 3, breakpoints
- `Z1,addr,kind` and `z1,addr,kind`, hardware breakpoint via MCD IP trigger
- `Z0,addr,kind` and `z0,addr,kind`, software breakpoint. On TC4D7 most debug
  targets are flash, where we cannot write a trap byte, so v1 maps Z0 to the
  same MCD IP trigger as Z1, with a note in the docs. A true RAM software
  breakpoint (write a debug instruction, restore on removal) can come later
- `Z2 Z3 Z4`, watchpoints, deferred to a later phase

Phase 4, optional, flash through GDB
- `qXfer:memory-map:read`, describe the flash regions
- `vFlashErase`, `vFlashWrite`, `vFlashDone`, drive the existing flash-prog so
  `load` in GDB programs flash directly

Anything unrecognized gets an empty reply `$#00`, which GDB treats as
unsupported, so partial implementations degrade gracefully.

## Register map

GDB's TriCore target defines 44 registers in this fixed order (from
gdb/tricore-tdep.h), each 32 bits. The `g` packet is therefore 44 * 4 = 176
bytes, 352 hex digits, little endian per register.

| GDB # | name  | TriCore reg | MCD source |
|------:|-------|-------------|------------|
| 0-15  | d0-d15 | data GPRs   | CSFR GPR block |
| 16-31 | a0-a15 | address GPRs| CSFR GPR block |
| 32    | lcx   | LCX         | core CSFR |
| 33    | fcx   | FCX         | core CSFR |
| 34    | pcx   | PCXI        | core CSFR |
| 35    | psw   | PSW         | core CSFR |
| 36    | pc    | PC          | core CSFR (tc-load uses 0xF881FE08) |
| 37    | icr   | ICR         | core CSFR |
| 38    | isp   | ISP         | core CSFR |
| 39    | btv   | BTV         | core CSFR |
| 40    | biv   | BIV         | core CSFR |
| 41    | syscon| SYSCON      | core CSFR |
| 42    | pcon0 | PMUCON0     | core CSFR |
| 43    | dcon0 | DMUCON      | core CSFR |

Mapping strategy. At startup query mcd_qry_reg_groups then mcd_qry_reg_map to
enumerate the target's registers with their names and MCD addresses, and build a
name to MCD register table. Then resolve the 44 GDB registers by name, with a
small alias list for the spellings that differ (GDB pcx is MCD PCXI, pcon0 is
PMUCON0, dcon0 is DMUCON). This avoids hardcoding addresses and adapts if the
map differs. Known CSFR offsets (core 0 CSFR base 0xF8810000) are kept as a
fallback, PCXI 0xFE00, PSW 0xFE04, PC 0xFE08, SYSCON 0xFE14, BIV 0xFE20,
BTV 0xFE24, ISP 0xFE28, ICR 0xFE2C, FCX 0xFE38, LCX 0xFE3C, data GPRs
0xFF00-0xFF3C, address GPRs 0xFF80-0xFFBC.

Reads and writes use the same mcd_execute_txlist path as memory, the register
addresses are just another address space in MCD.

## Breakpoints via MCD triggers

A GDB `Z1,addr,kind` becomes an MCD trigger created with mcd_create_trig_f using
mcd_trig_simple_core_st, type MCD_TRIG_TYPE_IP, addr_start set to the breakpoint
address, addr_range 0 (single address), action MCD_TRIG_ACTION_DBG_DEBUG (stop
this core into debug mode). The returned trig_id is stored in a table keyed by
address so `z1` can call mcd_remove_trig_f. After editing the trigger set,
mcd_activate_trig_set_f commits it.

On stop, mcd_qry_state returns the trig_id that caused the halt, which we match
back to an address so the stop reply can carry `hwbreak:`. The number of
hardware triggers is limited, query the maximum once and return an error packet
if GDB asks for more than the hardware has.

## Run-control and the stop loop

Continue, mcd_run_f(core, false). Step, mcd_step_f(core, false,
MCD_CORE_STEP_TYPE_INSTR, 1). Both then enter the wait loop.

Wait loop. Poll mcd_qry_state at a modest interval. While the core reports
MCD_CORE_STATE_RUNNING, also poll the GDB socket for an incoming `0x03`
interrupt, if it arrives call mcd_stop_f. When the state becomes
MCD_CORE_STATE_HALTED or DEBUG, read the stop reason from the state struct
(trig_id and event), compose the `T` stop reply, and send it. This is a simple
poll rather than an MCD event callback, which keeps v1 small, the interval is a
tunable.

## Milestones

1. Build and install tricore-elf-gdb from the toolchain source
2. Factor tcmcd shared unit out of tc-load, confirm tc-load still works
3. RSP framing and the static handshake, GDB connects and stays halted
4. Registers and memory, GDB shows registers and can read memory and disassemble
5. Continue, step, and Ctrl-C with correct stop replies
6. Hardware breakpoints, the full break, inspect, continue loop
7. Polish, target.xml feature serving, docs, and a scripted smoke test

A natural first proof is, flash hello_uart, connect GDB, break on uart_putc, and
watch GDB stop with the right source line and registers.

## Risks and open questions

- Register name spellings in the MCD map versus GDB, handled by the alias list,
  to be confirmed against the live map at implementation time
- Single step over the CSA based call and return, mcd_step at instruction
  granularity should be transparent, verify around call, ret, and traps
- Breakpoints in flash, hardware triggers work regardless of flash or RAM, so
  this is fine, the software breakpoint case is the one we defer
- Stop latency from polling, acceptable for v1, can move to MCD event
  notification later if it matters
- Reset semantics, GDB `R` and `kill`, map to our reset and halt, define clearly
  so a GDB session leaves the target in a known state
