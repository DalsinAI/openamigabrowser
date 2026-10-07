#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC=${CC:-"$STOVE/prefix/bin/m68k-amigaos-gcc"}
OUT=${OUT:-"$HERE/build/os3-datatype"}
mkdir -p "$OUT"
"$CC" -noixemul -m68040 -m68881 -std=gnu99 -Wall -Wextra -Werror -O2 \
  -I"$HERE/src/lite" "$HERE/src/lite/ob_image_dt.c" "$HERE/tests/test_datatype_image.c" \
  -lamiga -o "$OUT/DTImageTest"
echo "$OUT/DTImageTest ($(wc -c < "$OUT/DTImageTest") bytes)"
sha256sum "$OUT/DTImageTest"
