# open-tricore

Open-source tooling for developing on Infineon AURIX TriCore microcontrollers from Linux. The focus is the AURIX TC4x family, validated on the TC4D7 Lite Kit (`KIT_A3G_TC4D7_LITE`).

Infineon's TriCore toolchain is Windows-centric and the device-access layer (DAS) was historically Windows-only. open-tricore assembles a working Linux flow from the open-source pieces that already exist and fills the gaps with original, permissively licensed tools, all verified against real silicon.

Maintained by Syuma Labs.

## Status

Validated on a real TC4D7 over the on-board DAP debugger.

- Connect to the chip from Linux via Infineon's TAS server
- Read and write any memory or peripheral register, with hardware read-back verification
- Reset and halt the cores
- Drive GPIO and LEDs by register, see [`tools/led-demo`](tools/led-demo)

Roadmap.

- Open-source `tricore-elf-gcc` toolchain (GCC, binutils, newlib), see [`toolchain/`](toolchain)
- Board-support package with linker scripts and startup for TC4D7, see [`bsp/`](bsp)
- Load and run code from RAM, validated on silicon via MCD run-control
- Flash programming of PFLASH
- GDB run-control bridge

## Layout

```
open-tricore/
  docs/          setup guides plus reverse-engineered hardware and register notes
  scripts/       host setup automation (udev, FTDI, launch tas_server)
  tools/         our original Apache-2.0 tools
    led-demo/    GPIO and LED control with register read-back verification
  bsp/tc4d7/     linker scripts and startup for the chip (roadmap)
  toolchain/     build scripts for the open-source tricore-elf-gcc (roadmap)
  third_party/   upstream open-source deps as git submodules under their own licenses
  vendor/        git-ignored proprietary Infineon binaries you download yourself
```

## Licensing boundary

Read this before contributing.

- Our code in `tools/`, `bsp/`, `docs/`, `scripts/`, and the `toolchain/` build scripts is Apache-2.0.
- `third_party/` holds upstream open-source projects used under their own licenses.
- `vendor/` is git-ignored and holds proprietary Infineon binaries such as `tas_server` and the DAS `.deb`. We do not redistribute these. You download them yourself, see [`docs/linux-setup.md`](docs/linux-setup.md).
- The GCC-based toolchain built under `toolchain/` is GPL under its own license. It runs as a separate program and does not affect the license of anything else in this repo.

## Quick start

```bash
# 1. Download the Infineon TAS (DAS v8+) Linux package into vendor/ using a free myInfineon login.
#    https://softwaretools.infineon.com/tools/com.ifx.tb.tool.infineontoolaccesssockettas

# 2. Install host-side access, the FTDI driver and a udev rule. Needs sudo.
./scripts/install-tas.sh

# 3. Start the TAS server, which bridges the USB DAP to localhost:24817.
./scripts/run-tas-server.sh &

# 4. Build the upstream client and our tools.
./scripts/build.sh

# 5. Run the LED demo against the board.
./build/tools/led-demo/led-demo
```

See [`docs/linux-setup.md`](docs/linux-setup.md) for the full walkthrough and [`docs/hardware-notes.md`](docs/hardware-notes.md) for the verified register map.
