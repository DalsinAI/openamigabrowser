#!/bin/sh
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 AmigaChrome Team. MIT licence, see LICENSE.
#
# OpenBrowser Lite, the GadTools browser, for AmigaOS 3.x (68040) with the
# os32 stove (bebbo's m68k-amigaos-gcc 6.5, NDK 3.2) and the AmiSSL 5 SDK's
# headers. Soft float: it runs on a full 68040 and on an LC or EC040.
#
#   build-openbrowser.sh AMISSL_INCLUDE [OUT_DIR]     (default build/os3)
#   STOVE   stove root holding prefix/bin/m68k-amigaos-gcc
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# Soft float: the specs file links libnix's soft libm020 (maths through the
# mathieee libraries), not libm881, so one build runs on every 68040.
SPECS="-specs=$HERE/toolchain/m68k-soft-m68040.specs"
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32"}
CC="$STOVE/prefix/bin/m68k-amigaos-gcc"
AMISSL=${1:?give the include folder of the AmiSSL SDK}
OUT=${2:-"$HERE/build/os3"}
OM="$HERE/third_party/openmail"
mkdir -p "$OUT"
"$CC" -noixemul -m68040 $SPECS -std=gnu99 -Wall -Werror -Wno-pointer-sign -O2 -fno-delete-null-pointer-checks -fno-common \
    -I"$HERE/src" -I"$OM" -I"$AMISSL" \
    "$HERE"/src/*.c "$OM"/*.c -lamiga -o "$OUT/OpenBrowser"
echo "$OUT/OpenBrowser ($(wc -c < "$OUT/OpenBrowser") bytes)"
# obfetch: the same engine from a Shell, for tests.
"$CC" -noixemul -m68040 $SPECS -std=gnu99 -Wall -Werror -Wno-pointer-sign -O2 -fno-delete-null-pointer-checks -fno-common \
    -I"$HERE/src" -I"$OM" -I"$AMISSL" \
    "$HERE/src/ob_http.c" "$OM"/*.c "$HERE/tools/obfetch.c" -o "$OUT/obfetch"
echo "$OUT/obfetch ($(wc -c < "$OUT/obfetch") bytes)"
