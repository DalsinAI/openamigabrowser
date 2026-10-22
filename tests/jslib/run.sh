#!/bin/sh
# javascript.library engine core tests (tests/jslib/core_test.c) on Linux
# under qemu-m68k (68040), linked with an m68k-linux JavaScriptCore build of
# our patched WebKit (the JIT lab build: see tools/jsjit/lab).
#
#   JSC_BUILD   the lab's WebKit build folder (default /home/user/jsjit/build)
#
# MIT, Copyright (c) 2026 AmigaChrome Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
B=${JSC_BUILD:-/home/user/jsjit/build}
OUT=${OUT:-"$B/jslib-test"}
CXX=${CXX:-m68k-linux-gnu-g++-14}
CC=${CC:-m68k-linux-gnu-gcc-14}
[ -f "$B/lib/libJavaScriptCore.a" ] || { echo "run.sh: no JavaScriptCore build in $B" >&2; exit 2; }
mkdir -p "$OUT"
INC="-I$ROOT/src/jslib -I$ROOT/src/jslib/sdk/Include_H -I$HERE/host"
"$CXX" -m68040 -O2 -std=gnu++23 -fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter \
    $INC -I"$B/JavaScriptCore/Headers" -I"$B/JavaScriptCore/PrivateHeaders" \
    -c "$ROOT/src/jslib/jsl_core.cpp" -o "$OUT/jsl_core.o"
"$CC" -m68040 -O2 -Wall -Wextra $INC -c "$ROOT/src/jslib/jsl_os_posix.c" -o "$OUT/jsl_os_posix.o"
"$CC" -m68040 -O2 -Wall -Wextra -Wno-unused-parameter $INC -c "$HERE/core_test.c" -o "$OUT/core_test.o"
# jsc's own link line from the build (the offlineasm symbols and libraries)
LIBS=$(awk '/^build bin\/jsc:/ {f = 1} f && /LINK_LIBRARIES =/ {sub(/^ *LINK_LIBRARIES = /, ""); print; exit}' "$B/build.ninja")
(cd "$B" && "$CXX" -m68040 -static -Wl,--gc-sections "$OUT/core_test.o" "$OUT/jsl_core.o" "$OUT/jsl_os_posix.o" \
    $LIBS -lpthread -o "$OUT/core_test")
QEMU_CPU=m68040 qemu-m68k -cpu m68040 "$OUT/core_test"
