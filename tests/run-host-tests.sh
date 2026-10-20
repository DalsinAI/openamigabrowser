#!/bin/sh
# OpenBrowser Lite's host tests: weblayout's HTML Lite, CSS Lite, address
# resolver and its calls (weblayout.h, drawn by tests/host's stand-in
# graphics.library), and OpenMail's HTML-to-text converter, with the host's C
# compiler, on OpenLayout's portable C.
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
OM="$HERE/third_party/openmail"     # strncasecmp: POSIX
"$CC" $FLAGS -D_DEFAULT_SOURCE -I"$HERE/tests/host" -I"$WL" -I"$OM" -I"$OL/include" \
    "$WL/wl_document.c" "$WL/wl_paint.c" "$WL/wl_html.c" "$WL/wl_css.c" "$WL/wl_url.c" \
    "$OM/oam_text.c" "$OM/oam_buf.c" "$OM/oam_base64.c" "$OL/src/openlayout.c" \
    "$HERE/tests/test_wl_document.c" -o "$OUT/test_wl_document"
"$CC" $FLAGS -D_DEFAULT_SOURCE -I"$OM" "$OM/oam_html.c" "$OM/oam_buf.c" \
    "$HERE/tests/test_oam_html.c" -o "$OUT/test_oam_html"
"$OUT/test_wl_html" > "$OUT/test_wl_html.log" || { cat "$OUT/test_wl_html.log"; exit 1; }
tail -1 "$OUT/test_wl_html.log"
"$OUT/test_wl_css"
"$OUT/test_wl_url"
"$OUT/test_wl_document"
"$OUT/test_oam_html"
