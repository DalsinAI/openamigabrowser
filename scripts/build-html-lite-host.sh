#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OPENLAYOUT_ROOT=${OPENLAYOUT_ROOT:-/home/da1ek/openamigalayout}
CC=${HOST_CC:-cc}
OUT=${OUT:-"$HERE/build/host-lite"}
make -C "$OPENLAYOUT_ROOT" all >/dev/null
mkdir -p "$OUT"
"$CC" -O2 -std=c99 -Wall -Wextra -Werror -pedantic \
  -I"$HERE/src/lite" -I"$OPENLAYOUT_ROOT/include" \
  "$HERE/src/lite/ob_css_lite.c" "$HERE/src/lite/ob_html_lite.c" "$HERE/tests/test_html_lite.c" \
  "$OPENLAYOUT_ROOT/build/libopenlayout.a" -o "$OUT/test-html-lite"
"$OUT/test-html-lite"
