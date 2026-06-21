# Linux setup

How to bring up the full Linux access stack for the TC4D7. The chain is our client to `tas_server` on localhost:24817 to `libftd2xx` to the USB DAP to the chip.

## Prerequisites

- A Linux x86_64 host
- Build tools, `git`, `cmake` 3.25 or newer, a C++17 compiler, Python 3, and Conan 2
- The board connected by USB-C to the debug port. It appears in `lsusb` as `058b:0043`.

## 1. Download the proprietary TAS server

`tas_server` is the one closed piece and is not redistributed in this repo. Download the Infineon TAS package (DAS v8 or newer, Linux build) with a free myInfineon login.

https://softwaretools.infineon.com/tools/com.ifx.tb.tool.infineontoolaccesssockettas

Place the downloaded `.deb` into `vendor/`.

## 2. Install host-side access

`scripts/install-tas.sh` extracts the `.deb`, installs the bundled FTDI D2XX driver to `/usr/local/lib`, and installs a udev rule that makes the debugger node writable. It needs sudo.

```bash
./scripts/install-tas.sh
```

The udev rule is the package default.

```
SUBSYSTEM=="usb", ATTR{idVendor}=="058b", ATTR{idProduct}=="0043", MODE="0666"
```

## 3. Start the TAS server

```bash
./scripts/run-tas-server.sh &
```

Confirm it is listening.

```bash
ss -tlnp | grep 24817
```

## 4. Build the client and tools

`scripts/build.sh` builds the upstream `tas_client_api` first, then our tools.

```bash
./scripts/build.sh
```

## 5. Verify against the board

```bash
./build/tools/led-demo/led-demo
```

You should see the target identify as the TC4D7, the cores reset and halt, and the LEDs alternate with each step confirmed by reading `P03_IN` back from the chip.

## Notes

- `tas_server` runs fine after the install. The udev rule lets it open the device without root.
- The UART console is at `/dev/ttyUSB0`. Your user needs the `dialout` group to open it.
