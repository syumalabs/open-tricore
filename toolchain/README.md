# toolchain

An open-source `tricore-elf-gcc` toolchain for AURIX TriCore that runs on Linux. Built and verified to produce correct TriCore 1.8 code for the TC4x.

## What it provides

Based on the `NoMore201/tricore-gcc-toolchain` build harness, pinned for reproducibility.

- GCC 13.4
- Binutils 2.40
- Newlib (Cygwin newlib)
- GDB 14 with TriCore support
- QEMU 9.2 with TriCore support
- Targets TriCore 1.3, 1.3.1, 1.6, 1.6.1, 1.6.2, and 1.8

## Build

Install the dependencies once with sudo, then run the build script.

```bash
sudo ./toolchain/tricore-gcc-toolchain/scripts/install-apt-dependencies
./toolchain/build.sh
```

The script clones the pinned harness into `toolchain/tricore-gcc-toolchain`, fetches the gcc, binutils, and newlib sources, and builds GCC stage 2. It installs into `toolchain/install`. Expect 20 to 40 minutes.

## Use

```bash
export PATH="$PWD/toolchain/install/bin:$PATH"
tricore-elf-gcc -mtc18 -O2 -ffreestanding -c file.c -o file.o
```

The flag for the TC4x cores is `-mtc18`, which selects TriCore 1.8.

## Verified

A smoke test compiled for `-mtc18` produced genuine Infineon Tricore code. The generated store to `0xF003AC3C` with value `1<<25` matched, by hand, the `P03_OMR` register and bit the `led-demo` tool uses to drive LED1. So the toolchain agrees with the hardware.

## Not committed

The harness sources and the install tree are large and git-ignored. They are reproducible with `toolchain/build.sh`. Only the script and this document are tracked.

## Notes and limits

- The TC4x PPU vector unit is a different ISA and is not a target of this toolchain.
- The built toolchain is GPL under its own license. It runs as a separate program and does not affect the license of the rest of the repo.
