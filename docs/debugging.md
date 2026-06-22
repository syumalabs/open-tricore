# Building, running, and debugging on the TC4D7

This walks the full flow from C source to a live GDB session on the chip, using
the open-tricore toolchain and tools. Every command here was run against real
silicon. It assumes you have completed the host setup in
[`linux-setup.md`](linux-setup.md), built the tools with `scripts/build.sh`, and
built the toolchain with `toolchain/build.sh`.

Put the toolchain on your path first.

```bash
export PATH="$PWD/toolchain/install/bin:$PATH"
```

The board has one FTDI device with two interfaces, the DAP debugger (used by
`tas_server`) and a UART console at `/dev/ttyUSB0`. Make sure `tas_server` is
running.

```bash
./scripts/run-tas-server.sh &
ss -tlnp | grep 24817        # confirm it is listening
```

## 1. Compile a program

The BSP under `bsp/tc4d7/` supplies the startup (`crt0.S`, `crt0.c`), newlib
stubs (`syscalls.c`), the UART driver (`uart.c`), and linker scripts. Use
`hosted.ld` to run from RAM, `boot.ld` to run from flash on reset. Build with
`-mtc18` for the TC4x cores and `-nostartfiles` so our `crt0` is used.

This builds the printf-over-UART demo to run from RAM.

```bash
tricore-elf-gcc -mtc18 -O2 -ffunction-sections -nostartfiles -T bsp/tc4d7/hosted.ld \
    bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/syscalls.c bsp/tc4d7/uart.c \
    bsp/tc4d7/hello_uart.c -o hello_uart.elf
tricore-elf-objcopy -O binary hello_uart.elf hello_uart.bin
```

## 2. Run it from RAM and read the serial console

`tc-load run` loads the binary into RAM, sets the PC, and runs it. The `free`
option leaves it running after the loader exits.

```bash
./build/tools/tc-load/tc-load run hello_uart.bin 0x70100000 free
```

In another terminal, read the UART at 115200.

```bash
sudo stty -F /dev/ttyUSB0 115200 raw -echo -crtscts
sudo cat /dev/ttyUSB0
```

You will see the program's `printf` output, formatted text, a malloc result, and
a recursive computation, all running on the chip and printed over real serial.

## 3. Flash a program and boot it from reset

To run on power-up with no debugger, link for the boot address `0xA0000000` with
`boot.ld`, program it, and reset. The factory boot mode header already points
there, and our `crt0` disables the watchdogs so the program survives a cold boot.

```bash
tricore-elf-gcc -mtc18 -O2 -ffunction-sections -nostartfiles -T bsp/tc4d7/boot.ld \
    bsp/tc4d7/crt0.S bsp/tc4d7/crt0.c bsp/tc4d7/uart.c bsp/tc4d7/uart_hello.c \
    -o boot_uart.elf
tricore-elf-objcopy -O binary boot_uart.elf boot_uart.bin

./build/tools/tc-load/tc-load flash 0xA0000000 boot_uart.bin
./build/tools/tc-load/tc-load boot      # reset and release, the boot ROM runs the flashed code
```

Read `/dev/ttyUSB0` again and the flashed program is running on its own. It also
runs after a physical power cycle.

## 4. Debug with GDB

Start the gdbserver. It resets and halts the target, then listens on TCP 3333.

```bash
./build/tools/tc-gdbserver/tc-gdbserver 3333 &
```

Connect GDB with the ELF, so it has symbols and the TriCore architecture.

```bash
tricore-elf-gdb boot_uart.elf -ex "target remote :3333"
```

Then program flash and debug, all from GDB. The gdbserver serves a memory map,
so GDB's `load` programs the flash region directly over vFlash, no separate
`tc-load flash` step needed. You can still flash with `tc-load` if you prefer.

```
(gdb) load               # program the ELF straight to flash over vFlash
(gdb) break main
(gdb) continue
Breakpoint 1, 0xa000027e in main ()
(gdb) info registers pc psw
(gdb) stepi
(gdb) x/4i $pc          # disassemble
(gdb) x/4xw 0xA0000000  # read memory
(gdb) set $d5 = 0x1234  # write a register
(gdb) continue          # run to the next breakpoint, Ctrl-C interrupts a run
```

Breakpoints are hardware instruction-pointer triggers, so they work in flash.
Note that a breakpoint on a function the compiler inlined at `-O2` will not hit,
its symbol address is never executed. Set the breakpoint on a line or a function
that is actually called, or compile that translation unit at `-O0`.

## Tool reference

`tc-load`, run over MCD against CPU0, always reset and halt first.

| Command | What it does |
|---|---|
| `tc-load run <bin> <load-hex> [free \| dump <buf> ]` | load to RAM, set PC, run, optionally leave running or dump a captured buffer |
| `tc-load flash <target-hex> [file.bin]` | erase one 16K PFLASH sector and program it, then verify, refuses the UCB region |
| `tc-load peek <addr-hex> [count]` | read words from any address |
| `tc-load watch <bin> <load-hex> <counter-hex> <gap-ms>` | run and sample a RAM counter to measure a rate |
| `tc-load boot` | reset and release so the boot ROM runs flashed code |

`tc-gdbserver [port]`, GDB remote stub, default port 3333. Supports registers,
memory, disassembly, single step, continue, Ctrl-C, hardware breakpoints, and
GDB `load` straight to flash over vFlash.

## Notes

- Your user needs the `dialout` group to open `/dev/ttyUSB0`.
- An uncontrolled reset can drop the `tas_server` link. If a tool reports
  `open_server` or `open_core` errors, power-cycle the board and restart
  `tas_server`. A reset and halt always re-takes the core, nothing gets bricked.
- All PFLASH writes are recoverable with the debugger. The tools never write the
  UCB region, which holds passwords and protection settings.
