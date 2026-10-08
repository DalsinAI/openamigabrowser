#!/bin/sh
# Builds webbrowser.library and WBGrab with the os32-gcc16 compiler. The
# engine (WebBrowserEngine) is built with WebCore: build-webcore.sh build-browser.
#   OS32_GCC16  compiler root holding prefix/ (default ~/AmigaChrome/stoves/os32-gcc16)
# MIT, Copyright (c) 2026 Dalsin Limited.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
P=${OS32_GCC16:-"$HOME/AmigaChrome/stoves/os32-gcc16"}/prefix
CC="$P/bin/m68k-amigaos-gcc"
OUT=${1:-"$HERE/bin"}
mkdir -p "$OUT/obj"
# The library: its own ROMTag, no C library start-up; wb_start.c is linked first.
for f in wb_start wb_library; do
    "$CC" -m68020 -Os -fno-delete-null-pointer-checks -Wall -fomit-frame-pointer -I"$HERE" -I"$HERE/include" -I"$HERE/../webcore" -c "$HERE/$f.c" -o "$OUT/obj/$f.o"
done
"$CC" -nostartfiles -m68020 -o "$OUT/webbrowser.library" "$OUT/obj/wb_start.o" "$OUT/obj/wb_library.o" -lamiga
"$CC" -m68020 -mcrt=nix20 -Os -fno-delete-null-pointer-checks -Wall -I"$HERE/include" -o "$OUT/WBGrab" "$HERE/wbgrab.c" -lamiga
ls -l "$OUT/webbrowser.library" "$OUT/WBGrab"
