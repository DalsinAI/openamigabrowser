#!/bin/sh
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 AmigaChrome Team. MIT licence, see LICENSE.
#
# OpenBrowser Lite, the GadTools browser, for AmigaOS 3.x (68040) with the
# os32-gcc16 stove (OpenAmigaGCC 16.2.0-open2, libnix, OpenNDK; the os32
# GCC 6.5 stove builds it too). TLS is OpenTLS: opentls.library's API
# headers are in third_party/opentls, and no AmiSSL SDK is needed or used.
# Soft float: it runs on a full 68040 and on an LC or EC040.
#
#   build-openbrowser.sh [OUT_DIR]     (default build/os3)
#   STOVE   stove root holding prefix/bin/m68k-amigaos-gcc
#
# Until 19 October 2026 the first argument was the AmiSSL SDK's include
# folder; such an argument (or "-") is still taken and skipped.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# Soft float: the specs file links libnix's soft libm020 (maths through the
# mathieee libraries), not libm881, so one build runs on every 68040.
SPECS="-specs=$HERE/toolchain/m68k-soft-m68040.specs"
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC="$STOVE/prefix/bin/m68k-amigaos-gcc"
if [ $# -ge 1 ] && { [ "$1" = "-" ] || [ -f "$1/proto/amissl.h" ]; }; then
    [ "$1" = "-" ] || echo "build-openbrowser.sh: OpenBrowser Lite is OpenTLS only now; $1 (an AmiSSL SDK) is not used"
    shift
fi
OUT=${1:-"$HERE/build/os3"}
OM="$HERE/third_party/openmail"
OT="$HERE/third_party/opentls/include"
mkdir -p "$OUT"
"$CC" -noixemul -m68040 $SPECS -std=gnu99 -Wall -Werror -Wno-pointer-sign -O2 -fno-delete-null-pointer-checks -fno-common \
    -I"$HERE/src" -I"$OM" -I"$OT" \
    "$HERE"/src/*.c "$OM"/*.c -lamiga -o "$OUT/OpenBrowser"
echo "$OUT/OpenBrowser ($(wc -c < "$OUT/OpenBrowser") bytes)"
# obfetch: the same engine from a Shell, for tests.
"$CC" -noixemul -m68040 $SPECS -std=gnu99 -Wall -Werror -Wno-pointer-sign -O2 -fno-delete-null-pointer-checks -fno-common \
    -I"$HERE/src" -I"$OM" -I"$OT" \
    "$HERE/src/ob_http.c" "$OM"/*.c "$HERE/tools/obfetch.c" -o "$OUT/obfetch"
echo "$OUT/obfetch ($(wc -c < "$OUT/obfetch") bytes)"
