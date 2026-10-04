#!/bin/sh
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 Dalsin Limited. MIT licence, see LICENSE.
#
# Build WebKit's JSCOnly `jsc` shell for AmigaOS 3.2.x (68020+, FPU) with the
# os32-gcc16 stove. WebKit itself is not in this repository: check out the
# pinned upstream revision (webkit/UPSTREAM) and apply webkit/patches first.
#
#   WEBKIT_SOURCE  patched WebKit tree               (required)
#   WEBKIT_BUILD   build folder                      (default ./build/JSCOnly)
#   OS32_GCC16     stove root (prefix/, compat/)     (see toolchain/amigaos3-gcc16.cmake)
#   OAB_DEPS       folder holding icu78-m68k-amigaos (see scripts/build-icu.sh)
#   RUBY           Ruby >= 2.5 for WebKit's offlineasm (default: ruby on PATH)
#   JOBS           parallel jobs (default 2)
#
# usage: build-jsc.sh [all|configure|build|verify]
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
: "${WEBKIT_SOURCE:?set WEBKIT_SOURCE to the patched WebKit tree}"
BUILD=${WEBKIT_BUILD:-"$PWD/build/JSCOnly"}
RUBY=${RUBY:-$(command -v ruby || true)}
GMAKE=${GMAKE:-$(command -v gmake || command -v make)}
JOBS=${JOBS:-2}
MODE=${1:-all}
JSC="$BUILD/bin/jsc"

die() { echo "ERROR: $*" >&2; exit 2; }
[ -f "$WEBKIT_SOURCE/CMakeLists.txt" ] || die "WebKit source missing: $WEBKIT_SOURCE"
[ -n "$RUBY" ] || die "Ruby not found; set RUBY"

configure() {
    cmake -S "$WEBKIT_SOURCE" -B "$BUILD" -G "Unix Makefiles" \
      -DPORT=JSCOnly \
      -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain/amigaos3-gcc16.cmake" \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_JIT=OFF -DENABLE_DFG_JIT=OFF -DENABLE_FTL_JIT=OFF \
      -DENABLE_WEBASSEMBLY=OFF -DUSE_SYSTEM_MALLOC=ON \
      -DEVENT_LOOP_TYPE=Generic -DENABLE_STATIC_JSC=ON -DENABLE_API_TESTS=OFF \
      -DRuby_EXECUTABLE="$RUBY"
}

build() {
    (cd "$BUILD" && "$GMAKE" -j"$JOBS" jsc)
}

verify() {
    [ -f "$JSC" ] || die "jsc missing: $JSC"
    stat -c 'bytes=%s' "$JSC"
    sha256sum "$JSC"
    file "$JSC"
}

case "$MODE" in
    configure) configure ;;
    build) build; verify ;;
    verify) verify ;;
    all) [ -f "$BUILD/CMakeCache.txt" ] || configure; build; verify ;;
    *) die "usage: $0 [all|configure|build|verify]" ;;
esac
