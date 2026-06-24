# bsp/tc4d7

Board-support package for the TC4D7 Lite Kit. Startup code, newlib stubs, a UART
driver, and linker scripts so programs built with our toolchain run on the chip,
from RAM, from flash, or on a cold boot. Build with `-mtc18 -nostartfiles` and
one of the linker scripts below. See [`../../docs/debugging.md`](../../docs/debugging.md)
for the full build, flash, and debug flow.

## Startup

- `crt0.S` sets the stack and small-data base registers, then jumps to `_cstart`.
- `crt0.c` (`_cstart`) disables the CPU and system watchdogs, builds the context
  save area free list (iLLD `initCSA` algorithm), copies initialized data, clears
  bss, and calls `main`. The watchdog disable is essential for boot from reset,
  without it the watchdog resets the chip before `main` runs.

## Runtime

- `syscalls.c` provides newlib stubs. `write` is routed to the UART, so `printf`
  goes out the serial console. `_sbrk` is backed by a heap in DSPR0.
- `uart.c` is the ASCLIN0 UART driver, TX on P14.0, 115200 8N1. The baud is
  trimmed with the fractional divider because the backup clock measures slightly
  above its nominal 100 MHz.

## Interrupts and timer

- `ivt.S` is the interrupt vector table. The CPU jumps to `BIV + priority*32`, so
  one 32-byte slot per priority. The slot saves the lower context, calls the C
  handler with `call` (so its `ret` balances the upper context), restores, and
  returns with `rfe`.
- `irq.c` / `irq.h` provide `timer_tick_init(interval)`, which points `BIV` at the
  table, sets a dedicated interrupt stack (`ISP`), arms STM compare 0 against the
  free-running system timer, routes its request through the service node to CPU0,
  and enables interrupts. The handler advances the compare one interval ahead,
  clears the flag, and bumps `g_ticks`. Call it once after the runtime is up, then
  `g_ticks` advances on its own.
- The system timer lives in the CPU module (`STM` at `0xF88000xx`), and its
  compare 0 drives service node `STMCPU0 SR2`, both verified on silicon.

## GPIO

- `gpio.c` / `gpio.h` are a small port API. A port is its module base
  (`GPIO_P03` and friends), pins are 0..15. `gpio_output` and `gpio_input` set the
  per-pin direction through DRVCFG, `gpio_mode` takes a raw DRVCFG for alternate
  functions (DIR bit0, OD bit1, MODE bits[7:4]). `gpio_set`, `gpio_clear`,
  `gpio_write`, and `gpio_toggle` drive an output through the atomic OMR, and
  `gpio_read` returns the pin level from IN.

## Timing

- `timing.c` / `timing.h` are busy-wait delays and a monotonic time source over
  the free-running STM, no interrupts. `tc_delay_ticks` counts raw STM cycles and
  is exact, `tc_delay_us` and `tc_delay_ms` convert through `STM_HZ`, and
  `tc_micros` / `tc_millis` report time since the counter started. `STM_HZ` is the
  measured ~50.3 MHz STM clock (the ~100.7 MHz backup clock halved), override it
  if your clock setup differs. The 32-bit counter wraps about every 85 seconds.

## Clock

- `clock.c` / `clock.h` bring up the peripheral PLL. After a bare reset the chip
  runs on the always-on backup clock with the PLLs and external oscillator off,
  which is fine for STM, ports, and the UART but not for peripherals whose kernel
  clock comes from a PLL. `clock_init_pll` powers the peripheral PLL from the
  backup clock (no crystal needed), waits for lock, and routes the peripheral
  clock tree to it. `clock_qspi_select_pll(divsel)` then points the QSPI kernel
  clock at the PLL. The CCU and PLL registers are access-protected (the `PROTE`
  unit), which the module unlocks and re-locks around its writes.

## SPI

- `spi.c` / `spi.h` are a minimal QSPI master, 8-bit, MSB-first, mode 0, on
  channel 0. `spi_init(qspi, loopback)` configures a module (`SPI_QSPI0` and
  friends); `spi_transfer` does a blocking full-duplex byte exchange. The shift
  engine runs on the PLL clock, so call `clock_init_pll` and
  `clock_qspi_select_pll(1)` first. With `loopback` set, the module ties its own
  output to its input internally, so a transfer is verifiable with no wiring.
  Note: the QSPI clock divider must be non-zero; a divider of 0 switches the
  shift-engine clock off entirely.

## ADC

- `adc.c` / `adc.h` are a minimal time-multiplexed ADC (TMADC) driver. `adc_init`
  enables the converter, runs the mandatory start-up calibration, and enters RUN
  state, returning 0 once calibrated. `adc_read_channel` converts an input channel
  to a 12-bit code, and `adc_read_monitor` samples an internal signal (a supply or
  ground). The converter runs on the PLL clock, so call `clock_init_pll` and
  `clock_enable_adc` first. The internal monitor channels give a wiring-free
  self-test: VSSM (ground) reads about 0 and VDDK1 (a core supply) reads a large
  value, which is what `adc_demo` checks.

## PWM

- `pwm.c` / `pwm.h` generate edge-aligned PWM on the eGTM TOM (Timer Output
  Module). `pwm_init` brings up eGTM cluster 0 (module clock, CMU, fixed clock,
  submodule enables), `pwm_set(ch, period, duty)` configures and starts a TOM
  channel (period and duty in TOM-clock ticks, output high for the first `duty`
  ticks), and `pwm_set_duty` updates the duty glitch-free at the next period. The
  eGTM runs on the PLL clock, so call `clock_init_pll` and `clock_enable_egtm`
  first. The eGTM is the largest module on the chip; this is a minimal TOM PWM
  path, the access-protection, CMU fractional clock, and cluster layers are all
  handled in `pwm_init`.

## DMA

- `dma.c` / `dma.h` do blocking memory-to-memory transfers on the System DMA
  (DMA0) channel 0. `dma_init` prepares the shared LMU for DMA use (it opens the
  LMU region to the DMA's master tag and disables the ECC-on-uninitialized fault)
  and must run before the buffers are filled. `dma_copy(dst, src, words)` copies
  32-bit words and blocks until done. Buffers must live in the DMA-accessible
  shared LMU (`DMA_LMU`), not a CPU-local scratchpad, which the DMA cannot write.
  The SDMA is a safety DMA, so the driver also unlocks the channel's resource
  partition and grants the DMA tag access to the buffer memory.

## I2C

- `i2c.c` / `i2c.h` are a minimal 7-bit I2C master on I2C0. `i2c_init` brings the
  module up on SCL P13.1 / SDA P13.2 (the Lite Kit's onboard bus) at ~100 kHz,
  `i2c_write` does a START, address, data, STOP transfer and reports ACK or NACK,
  and `i2c_probe` address-only-probes a device. Call `clock_init_pll` and
  `clock_enable_i2c` first; fI2C from the PLL generates SCL. The I2C is a
  two-block module: the real module clock gate is the wrapper CLC at the module
  base + 0x10000, which must be opened before the kernel CLC1 at base + 0, so a
  single-CLC enable like the QSPI driver leaves the module unreachable. The
  board's onboard EEPROM at 0x50 makes the master self-testable with no wiring.

## CAN

- `can.c` / `can.h` are a classic-CAN controller on the MCMCAN (CAN0 node 0).
  `can_init` brings the node up with one dedicated TX buffer and a 2-entry RX
  FIFO, optionally in internal loopback, `can_send` transmits a standard-id frame
  of up to 8 bytes, and `can_recv` reads one back. Call `clock_init_pll` and
  `clock_enable_can` first; fMCAN from the PLL clocks the protocol engine. The MCR
  change-enable bits (CCCE, CI) do not read back, so the clock-select and RAM-init
  values are built in a single local and written without a read-back in between,
  or the node never gets a clock and never leaves init. Internal loopback is the
  classic Bosch sequence (CCCR.TEST, TEST.LBCK, CCCR.MON), which transmits with a
  self-generated ACK so the controller self-tests with no transceiver or wiring.

## Multicore

- `smp.c` / `smp.h` start a second TriCore core. After reset only CPU0 runs and
  the others sit in boot halt. `core_start(core, entry)` sets a secondary core's
  program counter and releases its boot halt so it runs `entry`, and `core_id`
  returns the index of the core executing the call. The secondary core starts
  with no stack and no context save area, so `entry` must be a leaf that makes no
  calls and needs no stack (a polling or compute loop), or set one up itself.
  `core_start_c(core, entry)` goes further: it gives the secondary core a full C
  runtime by setting up a stack and context save area in the core's local data
  scratchpad (the same CSA build `crt0` does for CPU0) before calling `entry`, so
  `entry` can be ordinary C with function calls and recursion. Cores share data
  through the LMU once its access-protection region is opened to all masters, the
  same grant the DMA driver uses; reads of shared globals also work, but a
  secondary core's writes to CPU0-owned memory are access-protection gated, so
  publish results through the LMU. `smp_demo` and `smp_c_demo` show both APIs.

## Linker scripts

- `ram.ld`, `hosted.ld` place code in PSRAM0 at `0x70100000` and data in DSPR0 at
  `0x70000000`. `hosted.ld` also defines the CSA, stack, and heap symbols the C
  runtime needs. Load and run with `tc-load run <bin> 0x70100000`.
- `flash.ld` places code in PFLASH at `0xA0300000`, away from the boot bank.
- `boot.ld` places code at `0xA0000000`, the address the factory boot mode header
  jumps to on reset, so a flashed program runs on power-up. Data load images sit
  in flash after the code and are copied to DSPR0 by `crt0`.

## Demos

- `blink.c` toggles P03.9, the LED the read-write demo drives.
- `selftest.c` proves execution four ways, a parked PC, a challenge response, a
  moving heartbeat, and a done marker, all read back over the debugger.
- `hello.c` and `hello_uart.c` exercise the hosted C runtime, the latter prints
  `printf`, `malloc`, and recursion output over the UART.
- `uart_hello.c` is a minimal UART transmit demo with no libc.
- `timer_demo.c` enables the periodic timer tick and publishes `g_ticks` to the
  heartbeat, so the moving count over the debugger shows interrupts running.
- `gpio_demo.c` blinks LED1 (P03.9) through the GPIO API and reads the pin back.
- `timing_demo.c` publishes `tc_millis` to the heartbeat and paces the loop with
  `tc_delay_ms`, showing the STM timing helper.
- `spi_demo.c` brings up the PLL, enables QSPI internal loopback, and exchanges a
  set of bytes, publishing the round-trip count to the heartbeat (6 = all good).
- `adc_demo.c` brings up the PLL and ADC and self-tests against the internal
  monitor channels, publishing `0xADC0` to the heartbeat when ground reads about 0
  and a supply reads a large value.
- `pwm_demo.c` brings up the PLL and eGTM, drives a PWM on TOM channel 0, and
  self-tests by sampling the live output, confirming the measured duty matches 50
  then 25 percent.
- `dma_demo.c` brings up DMA0, copies a 64-word buffer in the shared LMU with the
  DMA, and publishes the number of words that match the source to the heartbeat.
- `smp_demo.c` starts CPU1 on a worker loop and runs an 8-round request and
  response handshake with it through the shared LMU, publishing the number of
  rounds that passed to the heartbeat.
- `smp_c_demo.c` starts CPU1 with a full C runtime and runs a worker that answers
  each request by computing a Fibonacci number recursively, proving function
  calls and stack work on the secondary core, and checks each answer on CPU0.
- `smp_all_demo.c` starts all five secondary cores with the C runtime and runs a
  recursive worker on each, so all six TriCore cores run our code at once, and
  CPU0 checks every core's result.
- `i2c_demo.c` brings up the I2C master and self-tests against the onboard EEPROM,
  publishing 0x12C0AC to the heartbeat when the EEPROM ACKs at 0x50, an unused
  address NACKs, and a data write is acknowledged.
- `can_demo.c` brings up the CAN controller in internal loopback, sends a frame,
  receives it back, and publishes 0x0CA00D to the heartbeat when the id, length,
  and payload all match.

Register definitions are taken from the iLLD TC4Dx headers under
`third_party/illd_release_tc4x`.
