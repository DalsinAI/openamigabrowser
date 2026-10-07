#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OPENLAYOUT_ROOT=${OPENLAYOUT_ROOT:-/home/da1ek/openamigalayout}
HOST_CC=${HOST_CC:-cc}
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
AMIGA_CC=${AMIGA_CC:-"$STOVE/prefix/bin/m68k-amigaos-gcc"}
OUT=${OUT:-"$HERE/build/css-lite"}
mkdir -p "$OUT"
make -C "$OPENLAYOUT_ROOT" all >/dev/null
"$OPENLAYOUT_ROOT/build-amiga.sh" >/dev/null
"$HOST_CC" -O2 -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$HERE/src/lite" -I"$OPENLAYOUT_ROOT/include" \
  "$HERE/src/lite/ob_css_lite.c" "$HERE/tests/test_css_lite.c" \
  "$OPENLAYOUT_ROOT/build/libopenlayout.a" -o "$OUT/test-css-lite"
"$OUT/test-css-lite"
"$AMIGA_CC" -m68040 -m68881 -O2 -std=gnu99 -Wall -Wextra -Werror -noixemul \
  -I"$HERE/src/lite" -I"$OPENLAYOUT_ROOT/include" \
  "$HERE/src/lite/ob_css_lite.c" "$HERE/tests/test_css_lite.c" \
  "$OPENLAYOUT_ROOT/build-amiga/libopenlayout.a" -o "$OUT/CSSLiteTest"
echo "$OUT/CSSLiteTest ($(wc -c < "$OUT/CSSLiteTest") bytes)"
sha256sum "$OUT/CSSLiteTest"
