# Changelog

All work is validated on real silicon, an Infineon AURIX TC4D7 Lite Kit, over
the on-board DAP debugger.

## Unreleased

- BSP CAN driver (`can.c`/`can.h`), a classic-CAN controller on MCMCAN CAN0 node 0
  with `can_init`, `can_send`, and `can_recv`, validated on real silicon by sending
  a frame in internal loopback and receiving it back with the id, length, and
  payload intact, so it self-tests with no external wiring or transceiver, with
  `can_demo.c`. The bring-up turned on one subtlety, the MCR change-enable bits
  CCCE and CI do not read back, so the value must be built in a single local and
  written without a read-back in between, otherwise the protected clock-select and
  RAM-init writes are dropped and the node never gets a clock and never leaves
  init. Internal loopback is the classic Bosch path (CCCR.TEST then TEST.LBCK then
  CCCR.MON), not PORTCTRL.LBM which is external loopback and gets no acknowledge
- BSP `clock_enable_can` enables the two MCMCAN clocks from the peripheral PLL, the
  async CAN kernel clock fMCAN (PERCCUCON0) that clocks the protocol engine and the
  MCANH host clock (SYSCCUCON1) for the message-RAM interface, both off at reset

## v2.0

An I2C master driver, validated on real silicon against the board's onboard
EEPROM with no external wiring. With this the bare-metal stack covers the full
planned peripheral set and every TriCore core, all brought up by clean-room
reverse engineering.

- BSP I2C master driver (`i2c.c`/`i2c.h`), a 7-bit I2C0 master at ~100 kHz with
  `i2c_init`, `i2c_write`, and `i2c_probe`, validated on real silicon against the
  TC4D7 Lite Kit's onboard EEPROM (it ACKs at 0x50, an unused address NACKs, and
  a non-destructive data write is acknowledged), so it needs no external wiring,
  with `i2c_demo.c`. The I2C is a two-block module: the real module clock gate is
  the wrapper CLC at the module base + 0x10000 and must be opened before the
  kernel CLC1 at base + 0, otherwise every register access is a bus error, this
  is why a single-CLC bring-up copied from the QSPI driver left it unreachable
- BSP `clock_enable_i2c` enables the I2C kernel clock fI2C from the peripheral
  PLL (PER PLL output 2 plus PERCCUCON0.I2CDIV); fI2C generates SCL, so without
  it a transfer makes no clock

## v1.9

Multicore reaches all six cores, every TriCore core runs our C with a full
runtime, validated on real silicon.

- BSP secondary-core C runtime, `core_start_c` starts a secondary TriCore core
  with its own stack and context save area (built like `crt0` does for CPU0) and
  runs an ordinary C entry on it, so the second core supports function calls,
  recursion, and stack locals, not just stack-free leaves. Each core uses its own
  local data scratchpad for stack and CSA. Validated on real silicon with
  `smp_c_demo.c`, where CPU1 answers requests by computing Fibonacci numbers
  recursively and CPU0 checks every answer
- All six cores brought up, `smp_all_demo.c` starts every secondary core
  (CPU1..CPU5) with the C runtime and runs a recursive worker on each, so all six
  TriCore cores execute our code at once, validated on real silicon

## v1.8

Multicore support, CPU0 starts a second TriCore core and runs code on it,
validated on real silicon.

- BSP multicore support (`smp.c`/`smp.h`), `core_start` brings a secondary
  TriCore core out of boot halt and runs an entry function on it (set the core's
  PC, release its boot-halt), and `core_id` reports which core is executing.
  Validated on real silicon with a CPU0 to CPU1 request and response handshake
  through the shared LMU, with `smp_demo.c`

## v1.7

PPU fast shared-memory output, the scalar core returns result vectors through the
shared LMU at full bandwidth, validated on real silicon.

- PPU fast shared-memory output, the scalar core now returns results straight
  through the shared LMU at full bandwidth instead of only the bit-serial
  run-state handshake. The core's LMU writes were being dropped by the LMU
  access-protection unit (read-open, write-CPU-only out of reset), not by an
  uncontrollable cache layer as previously thought, granting the core's master
  tag write access to the LMU region fixes it. With `ppu/fastio.s` (compute
  kernel) and `ppu/fastio_demo.c` (on-chip pipeline, verified on real silicon),
  and updated PPU notes in `docs/ppu-reverse-engineering.md`

## v1.6

A DMA driver on the System DMA, validated on real silicon.

- BSP DMA driver (`dma.c`/`dma.h`), blocking memory-to-memory transfers on the
  System DMA channel 0 with `dma_init` and `dma_copy`, validated on real silicon
  by copying and verifying a buffer in the shared LMU. `dma_init` opens the LMU
  region to the DMA master tag and disables the ECC-on-uninitialized fault, both
  required before the buffers are filled, with `dma_demo.c`

## v1.5

A PWM driver on the eGTM, validated on real silicon.

- BSP PWM driver (`pwm.c`/`pwm.h`), edge-aligned PWM on the eGTM TOM with
  `pwm_set` and a glitch-free `pwm_set_duty`, validated on real silicon by
  sampling the live TOM output (50 and 25 percent duty both tracked the
  configured value) with no external wiring, with `pwm_demo.c`
- BSP `clock_enable_egtm` enables the eGTM kernel clock fGTM from the peripheral
  PLL, the eGTM timers do not run without it

## v1.4

An ADC (TMADC) driver with start-up calibration, validated on real silicon.

- BSP ADC driver (`adc.c`/`adc.h`), brings up time-multiplexed ADC module 0, runs
  the mandatory start-up calibration, and does blocking 12-bit conversions with
  `adc_read_channel` and `adc_read_monitor`, self-tested on real silicon against
  the internal monitor channels (VSSM reads about 0, a core supply reads a large
  value) so it needs no external wiring, with `adc_demo.c`
- BSP `clock_enable_adc` enables the ADC kernel clock fADC from the peripheral
  PLL, the ADC converter does not run without it

## v1.3

A QSPI (SPI) master and peripheral PLL bring-up, validated on real silicon.

- BSP QSPI (SPI) master (`spi.c`/`spi.h`), an 8-bit mode-0 master on channel 0
  with blocking full-duplex `spi_transfer`, validated on real silicon over the
  QSPI internal loopback (no external wiring), with `spi_demo.c`
- BSP peripheral PLL bring-up (`clock.c`/`clock.h`), `clock_init_pll` powers the
  peripheral PLL from the always-on backup clock with no external crystal, locks
  it, and routes the peripheral clock tree to it, and `clock_qspi_select_pll`
  points the QSPI kernel clock at the PLL. Needed because the QSPI shift engine
  runs on the PLL clock, which a bare reset leaves off. The clock-tree and PLL
  registers are unlocked through the access-protection unit
- Hardware note, the QSPI clock divider must be non-zero, a divider of 0 switches
  the shift-engine clock off entirely (the root cause that made the engine look
  dead while register access worked)

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
