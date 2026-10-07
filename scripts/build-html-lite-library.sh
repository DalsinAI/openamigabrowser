#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OPENLAYOUT_ROOT=${OPENLAYOUT_ROOT:-/home/da1ek/openamigalayout}
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC=${CC:-"$STOVE/prefix/bin/m68k-amigaos-gcc"}
OUT=${OUT:-"$HERE/build/os3-lite-library"}
"$OPENLAYOUT_ROOT/build-library.sh" >/dev/null
mkdir -p "$OUT"
"$CC" -m68040 -m68881 -O2 -std=gnu99 -Wall -Wextra -Werror -noixemul \
  -DOB_USE_OPENLAYOUT_LIBRARY=1 -I"$HERE/src/lite" -I"$OPENLAYOUT_ROOT/include" \
  "$HERE/src/lite/ob_openlayout_api.c" "$HERE/src/lite/ob_css_lite.c" "$HERE/src/lite/ob_html_lite.c" \
  "$HERE/tests/test_html_lite.c" -lamiga -o "$OUT/HTMLLiteLibTest"
cp "$OPENLAYOUT_ROOT/build-amiga/lib/openlayout.library" "$OUT/openlayout.library"
echo "$OUT/HTMLLiteLibTest ($(wc -c < "$OUT/HTMLLiteLibTest") bytes)"
sha256sum "$OUT/HTMLLiteLibTest" "$OUT/openlayout.library"
