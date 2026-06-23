# tc-gdbserver

A GDB remote stub for the TC4x over the Infineon MCD API. Connect a TriCore GDB
for source-level debugging on real silicon. Built on the shared `tcmcd` layer.
Apache-2.0.

## Use

```bash
./tc-gdbserver [port]          # default 3333, resets and halts the target, then listens
tricore-elf-gdb your.elf -ex 'target remote :3333'
```

Then debug as usual.

```
(gdb) load          # program the ELF straight to flash over vFlash
(gdb) break main
(gdb) continue
(gdb) stepi
(gdb) info registers
```

Supports register and memory read and write, disassembly via ELF symbols, single
step, continue, asynchronous stop (Ctrl-C), hardware breakpoints and data
watchpoints via MCD triggers, and GDB `load` to flash over vFlash. One thread,
CPU0.

Watchpoints map GDB `watch` (write), `rwatch` (read), and `awatch` (access) to
MCD write, read, and read-write data triggers, so `watch g_var` halts the core
when the location is accessed.

A breakpoint on a function the compiler inlined at `-O2` will not hit, its symbol
address is never executed. Set it on a called function or a source line.

See [`../../docs/debugging.md`](../../docs/debugging.md) for the workflow and
[`../../docs/gdbserver-design.md`](../../docs/gdbserver-design.md) for the design.
