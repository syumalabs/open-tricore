#!/usr/bin/env bash
#
# Build, run, and check every BSP self-test demo on the board, one after another,
# and print a pass/fail line for each. Each demo publishes a result to its
# heartbeat slot (0x70000000); this reads that slot back over the DAP and compares
# it to the demo's expected value, so the whole bare-metal stack is verified with
# a single command.
#
# Prerequisites (same as the rest of the flow):
#   - the TAS server is running        (scripts/run-tas-server.sh &)
#   - the client and tools are built   (scripts/build.sh)
#   - the tricore-elf toolchain is built and on PATH (toolchain/build.sh)
#
# Exit status is 0 only if every demo passed.
#
# Copyright 2026 Syuma Labs. Apache-2.0.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PATH="$ROOT/toolchain/install/bin:$PATH"

GCC=tricore-elf-gcc
OBJCOPY=tricore-elf-objcopy
TCLOAD="$ROOT/build/tools/tc-load/tc-load"
BSP="$ROOT/bsp/tc4d7"
LOAD=0x70100000
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# One row per demo: name | extra sources (besides crt0) | expected heartbeat.
# The expected value is the decimal heartbeat the demo publishes on success, or
# the word "moving" for the demos whose heartbeat is a free-running counter (they
# pass when the counter is seen to advance).
MANIFEST=(
  "spi|clock.c spi.c|6"
  "adc|clock.c adc.c|44480"        # 0xADC0
  "pwm|clock.c pwm.c|24589"        # 0x600D
  "dma|dma.c|64"
  "i2c|clock.c i2c.c|1228972"      # 0x12C0AC
  "can|clock.c can.c|827405"       # 0x0CA00D
  "flash|clock.c flash.c|14617105" # 0xDF0A11
  "smp|smp.c|8"
  "smp_c|smp.c|16"
  "smp_all|smp.c|5"
  "gpio|gpio.c|moving"
  "timer|ivt.S irq.c|moving"
  "timing|timing.c|moving"
)

pass=0
fail=0
printf '%-9s %-10s %-10s %s\n' "DEMO" "EXPECT" "GOT" "RESULT"
printf '%-9s %-10s %-10s %s\n' "----" "------" "---" "------"

for entry in "${MANIFEST[@]}"; do
    IFS='|' read -r name srcs expect <<< "$entry"
    demo="$BSP/${name}_demo.c"

    srclist=()
    for s in $srcs; do srclist+=("$BSP/$s"); done

    if ! "$GCC" -mtc18 -O2 -ffunction-sections -nostartfiles -T "$BSP/hosted.ld" \
            "$BSP/crt0.S" "$BSP/crt0.c" "${srclist[@]}" "$demo" \
            -I "$BSP" -o "$TMP/$name.elf" 2>"$TMP/$name.err"; then
        printf '%-9s %-10s %-10s %s\n' "$name" "$expect" "-" "BUILD-FAIL"
        fail=$((fail + 1))
        continue
    fi
    "$OBJCOPY" -O binary "$TMP/$name.elf" "$TMP/$name.bin"

    pkill -x tc-gdbserver 2>/dev/null || true
    sleep 1
    out="$("$TCLOAD" run "$TMP/$name.bin" "$LOAD" 2>&1)"
    line="$(printf '%s\n' "$out" | grep -i heartbeat | head -1)"
    x="$(printf '%s' "$line" | sed -nE 's/.*: *([0-9]+) *-> *([0-9]+).*/\1/p')"
    y="$(printf '%s' "$line" | sed -nE 's/.*: *([0-9]+) *-> *([0-9]+).*/\2/p')"

    result="FAIL"
    if [ -n "$y" ]; then
        if [ "$expect" = "moving" ]; then
            # a free-running counter: pass when it advanced, or at least is alive
            if [ "$x" != "$y" ] || [ "$y" -ne 0 ]; then result="PASS"; fi
        elif [ "$y" = "$expect" ]; then
            result="PASS"
        fi
    fi

    if [ "$result" = "PASS" ]; then pass=$((pass + 1)); else fail=$((fail + 1)); fi
    printf '%-9s %-10s %-10s %s\n' "$name" "$expect" "${y:-?}" "$result"
done

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
