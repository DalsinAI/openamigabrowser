#!/bin/sh
# OpenBrowser Lite with OpenLayout viewport, 68040 + FPU baseline.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC=${CC:-"$STOVE/prefix/bin/m68k-amigaos-gcc"}
AMISSL=${1:-"$HOME/ACNet-compat-lab/toolchain/amiga/AmiSSL/Developer/include"}
OUT=${2:-"$HERE/build/os3-layout-lite"}
OPENLAYOUT_ROOT=${OPENLAYOUT_ROOT:-/home/da1ek/openamigalayout}
OM="$HERE/third_party/openmail"

[ -d "$AMISSL" ] || { echo "AmiSSL include directory not found: $AMISSL" >&2; exit 2; }
"$OPENLAYOUT_ROOT/build-amiga.sh" >/dev/null
mkdir -p "$OUT"

"$CC" -noixemul -m68040 -m68881 -std=gnu99 -Wall -Werror -Wno-pointer-sign -Wno-format-truncation -O2 -fno-common \
    -I"$HERE/src" -I"$HERE/src/lite" -I"$OM" -I"$AMISSL" -I"$OPENLAYOUT_ROOT/include" \
    "$HERE"/src/*.c "$HERE"/src/lite/*.c "$OM"/*.c \
    "$OPENLAYOUT_ROOT/build-amiga/libopenlayout.a" -lamiga -o "$OUT/OpenBrowser"
echo "$OUT/OpenBrowser ($(wc -c < "$OUT/OpenBrowser") bytes)"
sha256sum "$OUT/OpenBrowser"
