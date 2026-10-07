#!/bin/sh
# OpenBrowser Lite using resident openlayout.library, 68040 + FPU.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC=${CC:-"$STOVE/prefix/bin/m68k-amigaos-gcc"}
AMISSL=${1:-"$HOME/ACNet-compat-lab/toolchain/amiga/AmiSSL/Developer/include"}
OUT=${2:-"$HERE/build/os3-layout-library"}
OPENLAYOUT_ROOT=${OPENLAYOUT_ROOT:-/home/da1ek/openamigalayout}
OM="$HERE/third_party/openmail"

[ -d "$AMISSL" ] || { echo "AmiSSL include directory not found: $AMISSL" >&2; exit 2; }
"$OPENLAYOUT_ROOT/build-library.sh" >/dev/null
mkdir -p "$OUT"

"$CC" -noixemul -m68040 -m68881 -std=gnu99 -Wall -Werror -Wno-pointer-sign -Wno-format-truncation -O2 -fno-common \
    -DOB_USE_OPENLAYOUT_LIBRARY=1 \
    -I"$HERE/src" -I"$HERE/src/lite" -I"$OM" -I"$AMISSL" -I"$OPENLAYOUT_ROOT/include" \
    "$HERE"/src/*.c "$HERE"/src/lite/*.c "$OM"/*.c \
    -lamiga -o "$OUT/OpenBrowser"

# Same HTTP/HTTPS engine as a Shell qualification tool.  This deliberately
# excludes the UI and OpenLayout so TLS/network failures can be separated
# from rendering failures in a guest release gate.
"$CC" -noixemul -m68040 -m68881 -std=gnu99 -Wall -Werror -Wno-pointer-sign -Wno-format-truncation -O2 -fno-common \
    -I"$HERE/src" -I"$OM" -I"$AMISSL" \
    "$HERE/src/ob_http.c" "$OM"/*.c "$HERE/tools/obfetch.c" \
    -o "$OUT/obfetch"

cp "$OPENLAYOUT_ROOT/build-amiga/lib/openlayout.library" "$OUT/openlayout.library"
echo "$OUT/OpenBrowser ($(wc -c < "$OUT/OpenBrowser") bytes)"
echo "$OUT/obfetch ($(wc -c < "$OUT/obfetch") bytes)"
echo "$OUT/openlayout.library ($(wc -c < "$OUT/openlayout.library") bytes)"
sha256sum "$OUT/OpenBrowser" "$OUT/obfetch" "$OUT/openlayout.library"
