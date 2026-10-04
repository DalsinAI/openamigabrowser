#!/bin/sh
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 Dalsin Limited. MIT licence, see LICENSE.
#
# Build WebCore (PORT=Amiga) and OpenBrowser's WebCore programs for AmigaOS
# 3.2.x (68020+, FPU) with the os32-gcc16 stove:
#   OpenBrowser   the browser window
#   obcore-dump   a local page's render tree, and a PNG of it
#   obcore-view   the browser's page layer without a window (tests)
#
# WebKit itself is not in this repository: check out the pinned upstream
# revision (webkit/UPSTREAM) with Source/WebCore and apply webkit/patches.
#
#   WEBKIT_SOURCE  patched WebKit tree                          (required)
#   WEBKIT_BUILD   build folder                (default ./build/Amiga)
#   OS32_GCC16     stove root (prefix/, compat/) (see toolchain/amigaos3-gcc16.cmake)
#   OAB_DEPS       folder holding icu78-m68k-amigaos/ (scripts/build-icu.sh) and
#                  m68k-amigaos/, the libraries from the OpenAmiga ports
#                  (cairo and pixman, FreeType, HarfBuzz, fontconfig, Expat,
#                  libxml2, SQLite, curl, libpsl, zlib, libpng) in one prefix
#   AMISSL_SDK     the AmiSSL 5 SDK's Developer folder           (required)
#   RUBY           Ruby >= 2.5 for WebKit's offlineasm (default: ruby on PATH)
#   JOBS           parallel jobs (default 2)
#
# usage: build-webcore.sh [all|configure|build]
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
: "${WEBKIT_SOURCE:?set WEBKIT_SOURCE to the patched WebKit tree}"
: "${AMISSL_SDK:?set AMISSL_SDK to the AmiSSL 5 SDK Developer folder}"
: "${OAB_DEPS:?set OAB_DEPS to the folder with icu78-m68k-amigaos and m68k-amigaos}"
BUILD=${WEBKIT_BUILD:-"$PWD/build/Amiga"}
RUBY=${RUBY:-$(command -v ruby || true)}
GMAKE=${GMAKE:-$(command -v gmake || command -v make)}
JOBS=${JOBS:-2}
MODE=${1:-all}

die() { echo "ERROR: $*" >&2; exit 2; }
[ -d "$WEBKIT_SOURCE/Source/WebCore" ] || die "WebKit source with Source/WebCore missing: $WEBKIT_SOURCE"
[ -n "$RUBY" ] || die "Ruby not found; set RUBY"
export OAB_DEPS

configure() {
    cmake -S "$WEBKIT_SOURCE" -B "$BUILD" -G "Unix Makefiles" \
      -DPORT=Amiga \
      -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain/amigaos3-gcc16.cmake" \
      -DCMAKE_BUILD_TYPE=MinSizeRel \
      -DENABLE_WEBCORE=ON \
      -DOPENBROWSER_WEBCORE_DIR="$ROOT/src/webcore" \
      -DOPENSSL_INCLUDE_DIR="$AMISSL_SDK/include" \
      -DOPENSSL_SSL_LIBRARY="$AMISSL_SDK/lib/AmigaOS3/libamisslstubs.a" \
      -DOPENSSL_CRYPTO_LIBRARY="$AMISSL_SDK/lib/AmigaOS3/libamisslstubs.a" \
      -DRuby_EXECUTABLE="$RUBY"
}

build() {
    # CMake's Makefile generator can run one custom command for two targets
    # at once, which breaks the generated sources: make those with one job.
    (cd "$BUILD" && "$GMAKE" -j1 WebCoreBindings)
    (cd "$BUILD" && "$GMAKE" -j"$JOBS" OpenBrowser obcore-dump obcore-view)
    ls -l "$BUILD"/bin/OpenBrowser "$BUILD"/bin/obcore-dump "$BUILD"/bin/obcore-view
}

case "$MODE" in
    configure) configure ;;
    build) build ;;
    all) [ -f "$BUILD/CMakeCache.txt" ] || configure; build ;;
    *) die "usage: $0 [all|configure|build]" ;;
esac
