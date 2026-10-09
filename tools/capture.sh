#!/usr/bin/env bash
# Writes one windowed capture per time given (seconds into the run) to docs/captures/<prefix>-<t>.png.
#   tools/capture.sh <prefix> <t> [<t> ...]
# Invokes the .scr directly with /w: never through ShellExecute, whose default verb is full screen.
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=$1; shift
C=/c/Users/jcaes/AppData/Local/Programs/CLion/bin
export PATH="/c/msys64/ucrt64/bin:$C/ninja/win/x64:$C/cmake/win/x64/bin:$PATH"
cmake --build build | grep -E "warning|error" && exit 1
mkdir -p docs/captures
log="$(cygpath -w "${TEMP:-/tmp}")\the-milky-way-claude-capture.log"
for t in "$@"; do
    out="$(cygpath -w "$PWD/docs/captures/${prefix}-${t}.png")"
    MILKY_WAY_LOG="$log" MILKY_WAY_TIME="$t" MILKY_WAY_CAPTURE="$out" MILKY_WAY_CAPTURE_AT=1 \
        timeout 60 ./build/the-milky-way-claude.scr /w
    echo "$out"
done
grep -E "fps|FAILED" "$(cygpath -u "$log")" | tail -n "$#"
rm -f "$(cygpath -u "$log")"
