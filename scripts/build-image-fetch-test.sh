#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
STOVE=${STOVE:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC=${CC:-"$STOVE/prefix/bin/m68k-amigaos-gcc"}
AMISSL=${AMISSL:-"$HOME/ACNet-compat-lab/toolchain/amiga/AmiSSL/Developer/include"}
OM="$HERE/third_party/openmail"
OUT=${OUT:-"$HERE/build/os3-image-fetch"}
mkdir -p "$OUT"
"$CC" -noixemul -m68040 -m68881 -std=gnu99 -Wall -Werror -Wno-pointer-sign -Wno-format-truncation -O2 -fno-common \
  -I"$HERE/src" -I"$HERE/src/lite" -I"$OM" -I"$AMISSL" \
  "$HERE/src/ob_http.c" "$HERE/src/lite/ob_image_dt.c" "$OM"/*.c \
  "$HERE/tests/test_image_fetch.c" -lamiga -o "$OUT/ImageFetchTest"
echo "$OUT/ImageFetchTest ($(wc -c < "$OUT/ImageFetchTest") bytes)"
sha256sum "$OUT/ImageFetchTest"
