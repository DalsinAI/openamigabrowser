#!/bin/sh
# OpenBrowser Lite with OpenLayout + native OpenTLS, 68040 + FPU baseline.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC=${CC:-"$STOVE/prefix/bin/m68k-amigaos-gcc"}
OUT=${1:-"$HERE/build/os3-layout-lite"}
OPENLAYOUT_ROOT=${OPENLAYOUT_ROOT:-/home/da1ek/openamigalayout}
OPENTLS_ROOT=${OPENTLS_ROOT:-/home/da1ek/AmigaChrome-dev/openstack-build/openamigatls}
OT="$OPENTLS_ROOT/build-amiga-tls"
W="$OPENTLS_ROOT/third_party/wolfssl"
OM="$HERE/third_party/openmail"

[ -f "$OT/lib/libopentls.a" ] || "$OPENTLS_ROOT/scripts/build-opentls.sh"
"$OPENLAYOUT_ROOT/build-amiga.sh" >/dev/null
mkdir -p "$OUT"

"$CC" -noixemul -m68040 -m68881 -std=gnu99 -Wall -Werror \
    -Wno-pointer-sign -Wno-format-truncation -O2 -fno-common \
    -I"$HERE/src" -I"$HERE/src/lite" -I"$OM" -I"$OPENTLS_ROOT/include" \
    -I"$OPENTLS_ROOT/build-wolfssl-amiga" -I"$W" -I"$OPENLAYOUT_ROOT/include" \
    "$HERE"/src/*.c "$HERE"/src/lite/*.c "$OM"/*.c \
    "$OPENLAYOUT_ROOT/build-amiga/libopenlayout.a" \
    "$OT/lib/libopentls.a" "$OT/lib/libwolfssl.a" "$OT/lib/libopentls.a" -lm -lamiga \
    -o "$OUT/OpenBrowser"
echo "$OUT/OpenBrowser ($(wc -c < "$OUT/OpenBrowser") bytes)"
sha256sum "$OUT/OpenBrowser"
