#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OPENLAYOUT_ROOT=${OPENLAYOUT_ROOT:-/home/da1ek/openamigalayout}
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC=${CC:-"$STOVE/prefix/bin/m68k-amigaos-gcc"}
OUT=${OUT:-"$HERE/build/os3-lite"}
"$OPENLAYOUT_ROOT/build-amiga.sh" >/dev/null
mkdir -p "$OUT"
"$CC" -m68040 -m68881 -O2 -std=c99 -Wall -Wextra -Werror -pedantic -noixemul \
  -I"$HERE/src/lite" -I"$OPENLAYOUT_ROOT/include" \
  "$HERE/src/lite/ob_html_lite.c" "$HERE/tests/test_html_lite.c" \
  "$OPENLAYOUT_ROOT/build-amiga/libopenlayout.a" -o "$OUT/HTMLLiteTest"
echo "$OUT/HTMLLiteTest ($(wc -c < "$OUT/HTMLLiteTest") bytes)"
sha256sum "$OUT/HTMLLiteTest"
