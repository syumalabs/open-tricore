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

Register definitions are taken from the iLLD TC4Dx headers under
`third_party/illd_release_tc4x`.
