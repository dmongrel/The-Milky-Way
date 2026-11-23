#!/usr/bin/env bash
# Runs the saver offscreen (unthrottled) in a 1600x900 /w window for about 12 s and prints the
# frame rate and the per-pass GPU times it logs. Builds first.
#   tools/bench.sh [time offset in seconds]
set -euo pipefail
cd "$(dirname "$0")/.."
# CLion's bundled CMake and Ninja, normally C:\Users\<username>\AppData\Local\Programs\CLion\bin;
# set CLION_BIN to use another install.
C="${CLION_BIN:-$(cygpath -u "$LOCALAPPDATA")/Programs/CLion/bin}"
export PATH="/c/msys64/ucrt64/bin:$C/ninja/win/x64:$C/cmake/win/x64/bin:$PATH"
cmake --build build | grep -E "warning|error" && exit 1
log="${TEMP:-/tmp}/the-milky-way-bench.log"
scratch="${TEMP:-/tmp}/the-milky-way-bench.png"
rm -f "$log"
MILKY_WAY_OFFSCREEN=1 MILKY_WAY_LOG="$(cygpath -w "$log")" MILKY_WAY_TIME="${1:-0}" \
    MILKY_WAY_CAPTURE="$(cygpath -w "$scratch")" MILKY_WAY_CAPTURE_AT=12 \
    timeout 60 ./build/the-milky-way.scr /w
grep -E "gpu ms|fps over" "$log" | tail -n 3
rm -f "$log" "$scratch"
