# open-tricore

Open-source tooling for developing on Infineon AURIX TriCore microcontrollers from Linux. The focus is the AURIX TC4x family, validated on the TC4D7 Lite Kit (`KIT_A3G_TC4D7_LITE`).

Infineon's TriCore toolchain is Windows-centric and the device-access layer (DAS) was historically Windows-only. open-tricore assembles a working Linux flow from the open-source pieces that already exist and fills the gaps with original, permissively licensed tools, all verified against real silicon.

Maintained by Syuma Labs.

## Status

A complete bare-metal C and C++ development flow for the TC4D7 on Linux, every layer validated on real silicon over the on-board DAP debugger. Current release is v1.5.

- Connect to the chip from Linux via Infineon's TAS server, read and write any memory or peripheral register with hardware read-back verification
- Open-source `tricore-elf` GCC and GDB toolchain (GCC 13.4, binutils 2.40, newlib, GDB 14), see [`toolchain/`](toolchain)
- Board-support package with startup, watchdog handling, newlib stubs, and linker scripts for RAM, flash, and boot, see [`bsp/tc4d7`](bsp/tc4d7)
- Load, run, flash, peek, watch, and boot with one tool, see [`tools/tc-load`](tools/tc-load)
- Full hosted C runtime proven on chip, printf, malloc, and recursion all run
- Real UART serial console at 115200, printf goes out `/dev/ttyUSB0`
- Boot from reset, flashed code runs on power-up with no debugger attached
- Source-level debugging with [`tools/tc-gdbserver`](tools/tc-gdbserver), breakpoints, watchpoints, single step, registers, and memory in GDB, the six cores as GDB threads, and GDB `load` programs flash directly over vFlash
- Our own code runs on the PPU scalar core (Synopsys ARC EV71), a clean-room first, load and start ARC code, feed it input through LMU, and read an arbitrary-width result back, see [`ppu/`](ppu) and [`tools/tc-ppu`](tools/tc-ppu)
- Interrupts and a periodic timer tick in the BSP, an interrupt vector table and an STM compare interrupt on CPU0, see [`bsp/tc4d7`](bsp/tc4d7)
- A GPIO API in the BSP, per-pin direction and mode, atomic set, clear, write, toggle, and read, see [`bsp/tc4d7`](bsp/tc4d7)
- STM-based timing in the BSP, microsecond and millisecond busy-wait delays and a monotonic time source, see [`bsp/tc4d7`](bsp/tc4d7)
- A QSPI (SPI) master in the BSP, an 8-bit mode-0 master with blocking full-duplex transfers, verified on chip over the QSPI's internal loopback with no wiring, see [`bsp/tc4d7`](bsp/tc4d7)
- Peripheral PLL bring-up in the BSP, from a bare reset with no Infineon startup software, which the QSPI shift engine and other PLL-clocked peripherals need, see [`bsp/tc4d7`](bsp/tc4d7)
- An ADC (TMADC) driver in the BSP, a 12-bit converter with start-up calibration and blocking conversions, self-tested on chip against the internal monitor channels with no external wiring, see [`bsp/tc4d7`](bsp/tc4d7)
- A PWM driver in the BSP, edge-aligned PWM on the eGTM TOM with runtime duty update, self-tested on chip by sampling the live output (50 and 25 percent duty both tracked), see [`bsp/tc4d7`](bsp/tc4d7)

Roadmap.

- More peripheral drivers, for example I2C, CAN, or DMA
- The PPU vector DSP, the scalar core is up (see above), the wide vector unit needs a vector toolchain, and a faster shared-memory result path

## Layout

```
open-tricore/
  docs/          setup, the debugging guide, and reverse-engineered hardware and register notes
  scripts/       host setup automation (udev, FTDI, launch tas_server, build)
  toolchain/     build script for the open-source tricore-elf GCC and GDB
  bsp/tc4d7/     startup, watchdog disable, newlib stubs, and linker scripts (RAM, flash, boot)
  ppu/           run code on the PPU scalar core (ARC EV71), example and build steps
  tools/         our original Apache-2.0 tools
    common/        shared MCD access layer (tcmcd), used by the tools below
    led-demo/      GPIO and LED control with register read-back verification
    tc-load/       load, run, flash, peek, watch, and boot over MCD
    tc-gdbserver/  GDB remote stub over MCD for source-level debugging
    tc-ppu/        load, run, and exchange data with the PPU scalar core
  third_party/   upstream open-source deps as git submodules under their own licenses
  vendor/        git-ignored proprietary Infineon binaries you download yourself
```

## Licensing boundary

Read this before contributing.

- Our code in `tools/`, `bsp/`, `docs/`, `scripts/`, and the `toolchain/` build scripts is Apache-2.0.
- `third_party/` holds upstream open-source projects used under their own licenses.
- `vendor/` is git-ignored and holds proprietary Infineon binaries such as `tas_server`, `libmcdxdas.so`, and the DAS `.deb`. We do not redistribute these. You download them yourself, see [`docs/linux-setup.md`](docs/linux-setup.md).
- The GCC and GDB toolchain built under `toolchain/` is GPL under its own license. It runs as a separate program and does not affect the license of anything else in this repo.

## Quick start

Host access first. This brings up the connection to the board and verifies it with the LED demo.

```bash
# 1. Download the Infineon TAS (DAS v8+) Linux package into vendor/ using a free myInfineon login.
#    https://softwaretools.infineon.com/tools/com.ifx.tb.tool.infineontoolaccesssockettas

# 2. Install host-side access, the FTDI driver and a udev rule. Needs sudo.
./scripts/install-tas.sh

# 3. Start the TAS server, which bridges the USB DAP to localhost:24817.
./scripts/run-tas-server.sh &

# 4. Build the upstream client and our tools.
./scripts/build.sh

# 5. Verify against the board.
./build/tools/led-demo/led-demo
```

Then build the compiler and your first program. The toolchain build is separate because it is large and takes 20 to 40 minutes.

```bash
# 6. Build the open-source tricore-elf GCC and GDB toolchain.
sudo ./toolchain/tricore-gcc-toolchain/scripts/install-apt-dependencies   # first time only
./toolchain/build.sh
export PATH="$PWD/toolchain/install/bin:$PATH"
```

From here, [`docs/debugging.md`](docs/debugging.md) walks through compiling a program, running it over UART, flashing it, booting from reset, and a full GDB session with breakpoints. See [`docs/linux-setup.md`](docs/linux-setup.md) for the host setup in detail and [`docs/hardware-notes.md`](docs/hardware-notes.md) for the verified register map.
