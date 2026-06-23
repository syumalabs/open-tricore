# Changelog

All work is validated on real silicon, an Infineon AURIX TC4D7 Lite Kit, over
the on-board DAP debugger.

## v1.2

A peripheral HAL, a periodic timer tick, a much stronger debugger, and PPU
tooling, all validated on real silicon.

- BSP interrupts and a periodic timer tick, an interrupt vector table (`ivt.S`)
  and `timer_tick_init` (`irq.c`) driving an STM compare interrupt on CPU0, with
  `timer_demo.c`
- BSP GPIO API (`gpio.c`/`gpio.h`), per-pin direction and mode, atomic set, clear,
  write, toggle, and read, with `gpio_demo.c`
- BSP timing helper (`timing.c`/`timing.h`), STM-based busy-wait delays
  (`tc_delay_us`/`tc_delay_ms`/`tc_delay_ticks`) and a time source
  (`tc_micros`/`tc_millis`), with `timing_demo.c`
- `tc-gdbserver` exposes the seven TriCore cores as GDB threads, `info threads`
  and `thread N` switch which core's registers and memory you inspect, each with
  its own register map, and the stop reply names the thread that stopped
- `tc-gdbserver` data watchpoints, GDB `watch`, `rwatch`, and `awatch` map to MCD
  write, read, and read-write triggers
- PPU result vectors, `tc-ppu call --out N` streams back N result words, with the
  reusable `ppu_call` host helper (`tools/common/ppu.{c,h}`), the shared
  `ppu/rt.inc` runtime, and the `ppu/checksum.s` example

## v1.1

- PPU bring-up by clean-room reverse engineering, our own code runs on the PPU
  scalar core (Synopsys ARC EV71) with no MetaWare and no NDA material
- `tc-ppu`, load and start ARC code on the PPU, feed it input through LMU, and
  read an arbitrary-width result back over a run-state handshake
- `ppu/`, a worked example (sum of two operands) with build steps
- `docs/ppu-reverse-engineering.md`, the full register-level findings, the boot
  recipe, the memory map, the STU loader, and the data channels

## v1.0

A complete, documented bare-metal development stack for the AURIX TC4D7 on Linux.

- Open-source `tricore-elf` GCC and GDB toolchain (GCC 13.4, binutils 2.40,
  newlib, GDB 14)
- Board-support package, startup with watchdog disable, newlib stubs, a UART
  driver, and linker scripts for RAM, flash, and boot
- `tc-load`, load, run, flash, peek, watch, and boot over MCD
- Hosted C runtime, printf, malloc, and recursion on chip
- Real UART serial console at 115200, printf over `/dev/ttyUSB0`
- Boot from reset, flashed code runs on power-up with no debugger
- `tc-gdbserver`, source-level debugging in GDB, registers, memory, disassembly,
  single step, continue, Ctrl-C, hardware breakpoints, and `load` to flash over
  vFlash
- Setup and debugging guides under `docs/`

## Release history

- **v0.4** GDB debugging over MCD, `tc-gdbserver` with breakpoints
- **v0.3** boot from reset via the boot mode header, watchdog disable in startup
- **v0.2** ASCLIN UART and printf over the serial console
- **v0.1** toolchain, BSP, hosted C runtime, and `tc-load`
