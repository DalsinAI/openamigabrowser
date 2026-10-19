#!/bin/sh
# OpenBrowser Lite's host tests: weblayout's HTML Lite, CSS Lite and address
# resolver with the host's C compiler, on OpenLayout's portable C.
#
#   OPENLAYOUT_SRC  an openamigalayout checkout (default ../openamigalayout)
#   HOST_CC         the compiler (default cc)
#   SANITIZE        1: with AddressSanitizer and UBSan
#
# MIT, Copyright (c) 2026 AmigaChrome Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OL=${OPENLAYOUT_SRC:-"$HERE/../openamigalayout"}
CC=${HOST_CC:-cc}
OUT=${OUT:-"$HERE/build/host-tests"}
WL="$HERE/src/weblayout"
FLAGS="-O2 -std=c99 -Wall -Wextra -Werror -pedantic"
[ "${SANITIZE:-0}" = 1 ] && FLAGS="$FLAGS -g -fsanitize=address,undefined -fno-sanitize-recover=all"
mkdir -p "$OUT"
"$CC" $FLAGS -I"$WL" -I"$OL/include" "$WL/wl_html.c" "$WL/wl_css.c" "$OL/src/openlayout.c" \
    "$HERE/tests/test_wl_html.c" -o "$OUT/test_wl_html"
"$CC" $FLAGS -I"$WL" -I"$OL/include" "$WL/wl_css.c" "$OL/src/openlayout.c" \
    "$HERE/tests/test_wl_css.c" -o "$OUT/test_wl_css"
"$CC" $FLAGS -I"$WL" "$WL/wl_url.c" "$HERE/tests/test_wl_url.c" -o "$OUT/test_wl_url"
"$OUT/test_wl_html" > "$OUT/test_wl_html.log" || { cat "$OUT/test_wl_html.log"; exit 1; }
tail -1 "$OUT/test_wl_html.log"
"$OUT/test_wl_css"
"$OUT/test_wl_url"
