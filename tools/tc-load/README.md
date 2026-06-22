# tc-load

Load, run, flash, and inspect TC4x code over the Infineon MCD API, the same
backend as `tas_server`. Built on the shared `tcmcd` layer. Apache-2.0.

Every command connects to CPU0 and resets and halts it first.

## Commands

```
tc-load run   <bin> <load-hex> [free | dump <buf-hex>]
    Load a flat binary into RAM (skipped if the address is in flash), set the
    PC, and run. free leaves it running after the loader exits. dump reads back
    a captured text buffer instead of the selftest proofs.

tc-load flash <target-hex> [file.bin]
    Erase the PFLASH sectors covering the image and program it, then verify by
    read-back. With no file it writes a test pattern. Refuses any target outside
    PFLASH, so the UCB region (passwords and protection) is never touched.

tc-load peek  <addr-hex> [count]
    Read one or more 32-bit words from any address.

tc-load watch <bin> <load-hex> <counter-hex> <gap-ms>
    Run the binary and sample a RAM counter twice to measure its rate.

tc-load boot
    Reset and release so the boot ROM reads the boot mode header and runs
    flashed code, no debugger control of the PC.
```

See [`../../docs/debugging.md`](../../docs/debugging.md) for the full flow.
