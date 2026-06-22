# Changelog

All work is validated on real silicon, an Infineon AURIX TC4D7 Lite Kit, over
the on-board DAP debugger.

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
