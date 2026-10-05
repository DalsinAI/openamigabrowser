#!/bin/sh
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 Dalsin Limited. MIT licence, see LICENSE.
#
# Build WebCore (PORT=Amiga) and OpenBrowser's WebCore programs for AmigaOS
# 3.2.x (68020+, FPU) with the os32-gcc16 stove. The build has two halves:
#
#   the engine   WebKit's WebCore, JavaScriptCore, WTF, PAL and bmalloc.
#                Built only when an input in its fingerprint changes: the
#                WebKit tree and its patches, the compiler, the headers of
#                ICU, the OpenAmiga libraries and AmiSSL, the build options.
#   the browser  OpenBrowser's own code (src/webcore): the page layer library
#                OBCore and the programs
#                  OpenBrowser   the browser window
#                  obcore-view   the page layer without a window (tests)
#                  obcore-dump   a local page's render tree, and a PNG of it
#                A change here compiles the changed files and relinks; it
#                never compiles WebCore.
#
# WebKit itself is not in this repository: check out the pinned upstream
# revision (webkit/UPSTREAM) with Source/WebCore and apply webkit/patches.
#
#   WEBKIT_SOURCE   patched WebKit tree                          (required)
#   WEBKIT_BUILD    build folder                 (default ./build/Amiga)
#   OS32_GCC16      stove root (prefix/, compat/)
#                   (default ~/AmigaChrome/stoves/os32-gcc16)
#   OAB_DEPS        folder holding icu78-m68k-amigaos/ (scripts/build-icu.sh)
#                   and m68k-amigaos/, the libraries from the OpenAmiga ports
#                   (cairo and pixman, FreeType, HarfBuzz, fontconfig, Expat,
#                   libxml2, SQLite, curl, libpsl, zlib, libpng) in one prefix
#   AMISSL_SDK      the AmiSSL 5 SDK's Developer folder           (required)
#   RUBY            Ruby >= 2.5 for WebKit's offlineasm (default: ruby on PATH)
#   JOBS            parallel compile jobs (default: one per CPU thread, but
#                   no more than one per 2 GB of free memory, since WebCore's
#                   largest files take up to 2 GB each to compile)
#   GENERATOR       CMake generator for a new build folder (default: Ninja
#                   when ninja is installed, otherwise Unix Makefiles; an
#                   existing folder keeps the generator it was made with)
#   OB_CCACHE       the ccache program, or "none"  (default: ccache on PATH)
#   OB_CCACHE_DIR   ccache's folder for this build
#                   (default ~/.cache/ccache-openbrowser)
#   OB_CCACHE_SIZE  that folder's size limit                     (default 20G)
#   OB_TEST_DIR     folder to stage the test programs and pages in
#                   (test-browser)
#
# usage: build-webcore.sh [COMMAND]
#   configure-engine     make the build folder; for an existing folder, only
#                        set the compiler cache (which recompiles nothing)
#   build-bindings       generate WebCore's derived sources, one job at a time
#   build-engine         build the engine, if its fingerprint has changed
#   build-browser        build OBCore and OpenBrowser only
#   build-tests          build obcore-view and obcore-dump only
#   test-browser         stage the programs, test pages and settings in
#                        OB_TEST_DIR, with an AmigaDOS script that runs them
#   clean-browser        remove the browser's objects and programs
#   clean-engine --yes   remove every object and program: the next
#                        build-engine compiles all of WebKit again (an hour
#                        or more without a warm compiler cache)
#   fingerprint          print the engine's fingerprint and its inputs
#   all                  build-engine, build-browser, build-tests (default)
#   configure, build     the old names: configure-engine, and all
#
# Each build writes its output to BUILD/logs/ and adds a line to
# BUILD/build-report.txt: the time taken, the files compiled, the programs
# and libraries linked, and ccache's hits and misses.
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
MODE=${1:-all}

die() { echo "ERROR: $*" >&2; exit 2; }

BUILD=${WEBKIT_BUILD:-"$PWD/build/Amiga"}
GCC16=${OS32_GCC16:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CCACHE=${OB_CCACHE:-$(command -v ccache || echo none)}
CCACHE_FOLDER=${OB_CCACHE_DIR:-"$HOME/.cache/ccache-openbrowser"}
CCACHE_SIZE=${OB_CCACHE_SIZE:-20G}
GMAKE=${GMAKE:-$(command -v gmake || command -v make || true)}
ENGINE_LIBRARIES="libWebCore.a libJavaScriptCore.a libWTF.a libPAL.a libbmalloc.a"

need_sources() {
    : "${WEBKIT_SOURCE:?set WEBKIT_SOURCE to the patched WebKit tree}"
    : "${AMISSL_SDK:?set AMISSL_SDK to the AmiSSL 5 SDK Developer folder}"
    : "${OAB_DEPS:?set OAB_DEPS to the folder with icu78-m68k-amigaos and m68k-amigaos}"
    [ -d "$WEBKIT_SOURCE/Source/WebCore" ] || die "WebKit source with Source/WebCore missing: $WEBKIT_SOURCE"
    export OAB_DEPS
}

# One job per CPU thread, but no more than one per 2 GB of free memory.
default_jobs() {
    threads=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
    if [ -r /proc/meminfo ]; then
        memory=$(awk '/^MemAvailable:/ { print int($2 / 2097152) }' /proc/meminfo)
        [ "${memory:-0}" -ge 1 ] || memory=1
        [ "$memory" -lt "$threads" ] && threads=$memory
    fi
    echo "$threads"
}
JOBS=${JOBS:-$(default_jobs)}

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum | cut -c1-64; else shasum -a 256 | cut -c1-64; fi
}

# One hash of every file under the given folders, names included.
tree_hash() {
    for dir in "$@"; do
        [ -d "$dir" ] && (cd "$dir" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 -r cat -- 2>/dev/null; find . -type f | LC_ALL=C sort)
    done | sha256
}

generator() {
    if [ -f "$BUILD/CMakeCache.txt" ]; then
        sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$BUILD/CMakeCache.txt"
    elif [ -n "${GENERATOR:-}" ]; then
        echo "$GENERATOR"
    elif command -v ninja >/dev/null 2>&1; then
        echo Ninja
    else
        echo "Unix Makefiles"
    fi
}

# The engine's options from the build folder's cache: what the compiler is
# told, and which WebKit features are built.
engine_options() {
    grep -E '^(CMAKE_BUILD_TYPE|CMAKE_TOOLCHAIN_FILE|PORT|DEVELOPER_MODE|ENABLE_[A-Z0-9_]+|USE_[A-Z0-9_]+|CMAKE_(C|CXX)_FLAGS[A-Z_]*|CMAKE_(C|CXX)_STANDARD_LIBRARIES|OPENSSL_[A-Z_]+|Ruby_EXECUTABLE)(:[A-Z]+)?=' \
        "$BUILD/CMakeCache.txt" | LC_ALL=C sort
}

# Everything that can change what the engine compiles to, one line each.
engine_inputs() {
    gcc="$GCC16/prefix/bin/m68k-amigaos-gcc"
    echo "webkit $(git -C "$WEBKIT_SOURCE" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "webkit-changes $( (git -C "$WEBKIT_SOURCE" diff HEAD -- Source CMakeLists.txt 2>/dev/null
        git -C "$WEBKIT_SOURCE" ls-files --others --exclude-standard -- Source 2>/dev/null | LC_ALL=C sort |
            while IFS= read -r f; do printf '%s %s\n' "$f" "$(sha256 < "$WEBKIT_SOURCE/$f")"; done) | sha256)"
    echo "compiler $("$gcc" --version | head -n 1) $(sha256 < "$("$gcc" -print-prog-name=cc1plus)")"
    echo "binutils $("$GCC16/prefix/bin/m68k-amigaos-as" --version | head -n 1)"
    echo "toolchain-file $(sha256 < "$ROOT/toolchain/amigaos3-gcc16.cmake")"
    echo "icu-headers $(tree_hash "$OAB_DEPS/icu78-m68k-amigaos/include")"
    echo "openamiga-headers $(tree_hash "$OAB_DEPS/m68k-amigaos/include")"
    echo "amissl-headers $(tree_hash "$AMISSL_SDK/include")"
    echo "options $(engine_options | sha256)"
}

# NOW: the engine's current fingerprint, worked out once per run.
NOW=
current_fingerprint() {
    [ -n "$NOW" ] || NOW=$(engine_inputs | sha256)
}

engine_built() {
    for lib in $ENGINE_LIBRARIES; do
        [ -f "$BUILD/lib/$lib" ] || return 1
    done
}

# A launcher that runs the compiler through ccache with OpenBrowser's own
# cache and the settings WebCore's precompiled header needs: without
# pch_defines and time_macros, ccache gives up on every WebCore file.
write_launcher() {
    mkdir -p "$BUILD"
    cat > "$BUILD/ob-ccache" <<EOF
#!/bin/sh
# Written by openamigabrowser's scripts/build-webcore.sh.
export CCACHE_DIR='$CCACHE_FOLDER'
export CCACHE_MAXSIZE='$CCACHE_SIZE'
export CCACHE_BASEDIR='$HOME'
export CCACHE_NOHASHDIR=true
export CCACHE_SLOPPINESS='pch_defines,time_macros,include_file_mtime,include_file_ctime'
exec '$CCACHE' "\$@"
EOF
    chmod +x "$BUILD/ob-ccache"
}

configure_engine() {
    need_sources
    if [ "$CCACHE" = none ]; then
        export WK_USE_CCACHE=NO
        set -- -DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER=
    else
        write_launcher
        set -- -DCMAKE_C_COMPILER_LAUNCHER="$BUILD/ob-ccache" -DCMAKE_CXX_COMPILER_LAUNCHER="$BUILD/ob-ccache"
    fi
    if [ -f "$BUILD/CMakeCache.txt" ]; then
        # An existing folder keeps its options; only the launcher changes,
        # which the Makefile generator does not treat as a reason to rebuild.
        cmake "$BUILD" "$@"
        return
    fi
    RUBY=${RUBY:-$(command -v ruby || true)}
    [ -n "$RUBY" ] || die "Ruby not found; set RUBY"
    cmake -S "$WEBKIT_SOURCE" -B "$BUILD" -G "$(generator)" "$@" \
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

ccache_counts() {
    if [ "$CCACHE" = none ]; then echo "0 0"; return; fi
    CCACHE_DIR=$CCACHE_FOLDER "$CCACHE" --print-stats 2>/dev/null | awk '
        $1 == "direct_cache_hit" || $1 == "preprocessed_cache_hit" { hits += $2 }
        $1 == "cache_miss" { misses += $2 }
        END { print hits + 0, misses + 0 }'
}

# run_build NAME JOBS TARGET...: build the targets in order, log the output
# and add a line to the build report.
run_build() {
    name=$1 jobs=$2
    shift 2
    mkdir -p "$BUILD/logs"
    log="$BUILD/logs/$name-$(date -u +%Y%m%dT%H%M%SZ).log"
    : > "$log"
    before=$(ccache_counts)
    start=$(date +%s)
    status=0
    if [ "$(generator)" = Ninja ]; then
        ninja -C "$BUILD" -j "$jobs" "$@" >> "$log" 2>&1 || status=$?
    else
        for target in "$@"; do
            "$GMAKE" -C "$BUILD" -j"$jobs" "$target" >> "$log" 2>&1 || { status=$?; break; }
        done
    fi
    seconds=$(( $(date +%s) - start ))
    after=$(ccache_counts)
    compiled=$(grep -c 'Building C' "$log" || true)
    linked=$(grep -c 'Linking C' "$log" || true)
    set -- $before $after
    line="$(date -u +%Y-%m-%dT%H:%M:%SZ) $name: $seconds s, $compiled compiled, $linked linked, ccache $(( $3 - $1 )) hits $(( $4 - $2 )) misses, exit $status"
    echo "$line" >> "$BUILD/build-report.txt"
    echo "$line"
    echo "  log: $log"
    if [ "$status" -ne 0 ]; then
        grep -E 'error:|Error [0-9]' "$log" | head -n 20 >&2 || true
        exit "$status"
    fi
}

build_bindings() {
    [ -f "$BUILD/CMakeCache.txt" ] || configure_engine
    # The Makefile generator can run one custom command for two targets at
    # once, which breaks the generated sources: make those with one job.
    run_build bindings 1 WebCoreBindings
}

build_engine() {
    need_sources
    [ -f "$BUILD/CMakeCache.txt" ] || configure_engine
    current_fingerprint
    if engine_built && [ -f "$BUILD/engine-fingerprint" ] && [ "$(cat "$BUILD/engine-fingerprint")" = "$NOW" ]; then
        echo "Engine up to date (fingerprint $(echo "$NOW" | cut -c1-16))"
        return
    fi
    build_bindings
    run_build engine "$JOBS" WebCore
    {
        echo "OpenBrowser engine, built $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "fingerprint $NOW"
        engine_inputs
        echo "patches $(cat "$ROOT"/webkit/patches/*.patch | sha256) ($(cd "$ROOT/webkit/patches" && ls *.patch | tr '\n' ' '))"
        echo "generator $(generator)"
    } > "$BUILD/engine-manifest.txt"
    echo "$NOW" > "$BUILD/engine-fingerprint"
    echo "Engine built (fingerprint $(echo "$NOW" | cut -c1-16)); manifest: $BUILD/engine-manifest.txt"
}

# The browser links against the engine as it was last built: say so when
# the engine's inputs have changed since then.
check_engine() {
    need_sources
    engine_built || die "no engine in $BUILD: run build-engine first"
    current_fingerprint
    if [ ! -f "$BUILD/engine-fingerprint" ] || [ "$(cat "$BUILD/engine-fingerprint")" != "$NOW" ]; then
        echo "NOTE: the engine's inputs have changed since build-engine last ran; this links the engine as it is" >&2
    fi
}

# The browser's targets without WebKit's: Ninja knows the engine is up to
# date; for Makefiles, CMake's "/fast" targets skip the walk through WebKit's
# targets (after letting CMake regenerate if a CMakeLists.txt changed).
browser_build() {
    name=$1
    shift
    check_engine
    if [ "$(generator)" = Ninja ]; then
        run_build "$name" "$JOBS" "$@"
    else
        "$GMAKE" -C "$BUILD" -s cmake_check_build_system
        run_build "$name" "$JOBS" $(for t in "$@"; do echo "$t/fast"; done)
    fi
}

build_browser() {
    browser_build browser OBCore OpenBrowser
    ls -l "$BUILD/bin/OpenBrowser"
}

build_tests() {
    browser_build tests OBCore obcore-view obcore-dump
    ls -l "$BUILD/bin/obcore-view" "$BUILD/bin/obcore-dump"
}

test_browser() {
    : "${OB_TEST_DIR:?set OB_TEST_DIR to the folder to stage the tests in}"
    strip="$GCC16/prefix/bin/m68k-amigaos-strip"
    mkdir -p "$OB_TEST_DIR/pages" "$OB_TEST_DIR/fontconfig"
    for program in OpenBrowser obcore-view obcore-dump; do
        "$strip" -o "$OB_TEST_DIR/$program" "$BUILD/bin/$program"
    done
    cp "$ROOT"/tests/pages/*.html "$OB_TEST_DIR/pages/"
    cp "$ROOT/src/webcore/fontconfig/fonts.conf" "$OB_TEST_DIR/fontconfig/"
    cp "$ROOT/src/webcore/blocklist" "$OB_TEST_DIR/"
    {
        echo "; OpenBrowser's test pages through obcore-view, staged by"
        echo "; scripts/build-webcore.sh test-browser. From this drawer, with the"
        echo "; fonts in Fonts: \"Execute OBTests\"; the results go to OBTests.log"
        echo "; (expected results: tests/pages/README.md)."
        echo "FailAt 1000"
        echo "Stack 400000"
        echo "Echo \"OBTESTS_START\" >OBTests.log"
        for page in "$ROOT"/tests/pages/*.html; do
            page=$(basename "$page")
            case "$page" in
                test1.html) echo "obcore-view pages/$page 800 600 test1.png >>OBTests.log" ;;
                input.html) echo "obcore-view -input pages/$page >>OBTests.log" ;;
                *) echo "obcore-view pages/$page >>OBTests.log" ;;
            esac
        done
        echo "Echo \"OBTESTS_DONE\" >>OBTests.log"
    } > "$OB_TEST_DIR/OBTests"
    echo "Staged in $OB_TEST_DIR; on the Amiga, from that drawer: Execute OBTests"
}

clean_browser() {
    rm -rf "$BUILD"/OpenBrowser/CMakeFiles/OBCore.dir "$BUILD"/OpenBrowser/CMakeFiles/OpenBrowser.dir \
        "$BUILD"/OpenBrowser/CMakeFiles/obcore-view.dir "$BUILD"/OpenBrowser/CMakeFiles/obcore-dump.dir
    rm -f "$BUILD"/lib/libOBCore.a "$BUILD"/bin/OpenBrowser "$BUILD"/bin/obcore-view "$BUILD"/bin/obcore-dump
    echo "Removed the browser's objects and programs from $BUILD"
}

clean_engine() {
    if [ "${1:-}" != --yes ]; then
        echo "clean-engine removes every object in $BUILD: the next build-engine compiles all" >&2
        echo "of WebKit again (an hour or more without a warm compiler cache). To do it:" >&2
        echo "  $0 clean-engine --yes" >&2
        exit 1
    fi
    cmake --build "$BUILD" --target clean
    rm -f "$BUILD/engine-fingerprint"
    echo "Removed every object from $BUILD"
}

case "$MODE" in
    configure-engine|configure) configure_engine ;;
    build-bindings) need_sources; build_bindings ;;
    build-engine) build_engine ;;
    build-browser) build_browser ;;
    build-tests) build_tests ;;
    test-browser) test_browser ;;
    clean-browser) clean_browser ;;
    clean-engine) clean_engine "${2:-}" ;;
    fingerprint)
        need_sources
        [ -f "$BUILD/CMakeCache.txt" ] || die "no build folder: $BUILD"
        current_fingerprint
        echo "fingerprint $NOW"
        engine_inputs ;;
    all|build) build_engine; build_browser; build_tests ;;
    *) die "unknown command $MODE (see the top of $0)" ;;
esac
