#!/bin/sh
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 AmigaChrome Team. MIT licence, see LICENSE.
#
# Build WebCore (PORT=Amiga) and OpenBrowser's WebCore programs for AmigaOS
# 3.2.x (68040, FPU) with the os32-gcc16 stove. The build has two halves:
#
#   the engine   WebKit's WebCore, JavaScriptCore, WTF, PAL and bmalloc.
#                Built only when an input in its fingerprint changes: the
#                WebKit tree and its patches, the compiler, the headers of
#                ICU, the OpenAmiga libraries and OpenTLS, the build options.
#   the browser  OpenBrowser's own code (src/webcore): the page layer library
#                OBCore and the programs
#                  OpenBrowser   the launcher: a title window, then it loads
#                                and runs OpenBrowser.engine, the browser
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
#                   SQLite, curl, libpsl, zlib, libpng) in one prefix, with
#                   OXP, the XML parser (openamigaxml's oxp/build-amiga.sh with
#                   OUT set to that prefix; WebKit patch 0030).
#                   Its curl must be openamigacurl's main branch, built on
#                   OpenTLS (opentls.library): WebCore links no OpenSSL and no
#                   AmiSSL SDK is used (WebKit patch 0022). openamigacurl
#                   builds curl's HTTP/2 with nghttp2: copy its libnghttp2.a
#                   into m68k-amigaos/lib beside libcurl.a, and the programs
#                   link it after curl (WebKit patch 0023). At run time the
#                   Amiga needs opentls.library 1.2 or newer in LIBS: and its
#                   CA bundle in S:OpenTLS/ca-bundle.pem for https://.
#
# No Ruby: WebKit's Ruby generators run as Python ports (patches 0019 and
# 0021), with the python3 CMake finds.
#   JOBS            parallel compile jobs (default: one per CPU thread, but
#                   no more than one per 2 GB of free memory, since WebCore's
#                   largest files take up to 2 GB each to compile)
#   GENERATOR       CMake generator for a new build folder (default: Ninja
#                   when ninja is installed, otherwise Unix Makefiles; an
#                   existing folder keeps the generator it was made with)
#   OB_OPTIMIZE     "size" (-Os, CMake's MinSizeRel), "js" (-Os, but -O2
#                   for JavaScriptCore and its interpreter), "layout" (-Os,
#                   but -O2 for WebCore's css, dom, layout, rendering and
#                   style folders) or "speed" (-O2 throughout: Release with
#                   -O2 rather than CMake's -O3)
#                   for a new build folder; an existing folder keeps its
#                   choice. Use a separate WEBKIT_BUILD to compare them.
#                   "js" costs about 6 MB of code and runs pages 1.1-1.65x
#                   faster on a 68040 than "size".          (default js)
#   OB_GC_SECTIONS  1: each function and variable in its own section, and the
#                   linker drops the ones nothing uses (-ffunction-sections
#                   -fdata-sections, --gc-sections). For a new build folder;
#                   an existing folder keeps its choice.       (default 0)
#   OB_PCREL        1: pc-relative code (-mpcrel -fPIC: bsr.l and (bd,pc),
#                   32-bit displacements on the 68040) linked as one hunk
#                   (toolchain/amigaos3-onehunk.ld), so calls and addresses
#                   inside the program need no relocation at load time;
#                   .data and .bss move into the code hunk. For a new build
#                   folder; an existing folder keeps its choice. (default 0)
#   OB_FOLD         1: after a program links, fold its duplicate template
#                   copies (scripts/hunkfold.py) and link it again: about
#                   6 MB less per program. Our ld keeps every object's copy
#                   of a template function that the object calls itself;
#                   folding rewrites copies of the archives in
#                   BUILD/lib-folded/PROGRAM/ so that only the copy the
#                   plain link used still defines the name, and
#                   --gc-sections drops the rest. Needs a folder that links
#                   with --gc-sections. ninja's own archives and programs'
#                   build state are untouched, so builds stay incremental;
#                   the plain link's map is kept as bin/NAME.plain.map and
#                   bin/NAME.map is the folded program's. 0: programs as
#                   ninja links them.
#                   (default 1 when the folder links with --gc-sections)
#   OB_ICU_DATA    "file": the programs read ICU's data (icudt78b.dat) at
#                   start from LOCALE:Unicode/ or PROGDIR:Unicode/, which
#                   keeps 5 MB of tables out of each program; "linked": the
#                   tables are linked in. A CMake option of src/webcore, so
#                   an existing folder takes "file" at its next configure.
#                   test-browser stages the file in Unicode/.  (default file)
#   OB_CCACHE       the ccache program, or "none"  (default: ccache on PATH)
#   OB_CCACHE_DIR   ccache's folder for this build
#                   (default ~/.cache/ccache-openbrowser)
#   OB_CCACHE_SIZE  that folder's size limit                     (default 20G)
#   OB_DISTCC_HOSTS other PCs to compile on with distcc, in distcc's form,
#                   such as "localhost/8 @192.168.0.168/10,lzo" ("@" reaches
#                   a PC over SSH, where the same compiler must be at the
#                   same path; at most 10 jobs each, SSH's default sessions
#                   per connection). Each host's /N is its number of jobs;
#                   JOBS then defaults to their sum. Needs ccache and distcc.
#   OB_TEST_DIR     folder to stage the test programs and pages in
#                   (test-browser)
#   OB_FONTS        folder holding the TrueType fonts the engine draws with
#                   (the Liberation and DejaVu .ttf files); test-browser copies
#                   them to Fonts/, which fontconfig reads as PROGDIR:Fonts.
#                   The engine stops at its first page without them.
#
# usage: build-webcore.sh [COMMAND]
#   configure-engine     make the build folder; for an existing folder, only
#                        set the compiler cache (which recompiles nothing)
#   build-bindings       generate WebCore's derived sources, one job at a time
#   build-engine         build the engine, if its fingerprint has changed
#   build-browser        build OBCore and OpenBrowser only
#   build-tests          build obcore-view and obcore-dump only
#   fold [PROGRAM...]    fold and link again the programs ninja has linked
#                        since they were last folded (OpenBrowser.engine,
#                        obcore-view, obcore-dump, WebBrowserEngine; or the
#                        ones named). build-browser and build-tests fold
#                        their programs when OB_FOLD is 1; run this after
#                        "ninja WebBrowserEngine"
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
# With distcc, the jobs of every host together.
distcc_jobs() {
    echo "$OB_DISTCC_HOSTS" | tr ' ' '\n' | sed -n 's|.*/\([0-9][0-9]*\).*|\1|p' | awk '{ n += $1 } END { print n + 0 }'
}
if [ -n "${OB_DISTCC_HOSTS:-}" ] && [ -z "${JOBS:-}" ]; then
    JOBS=$(distcc_jobs)
fi
JOBS=${JOBS:-$(default_jobs)}

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum | cut -c1-64; else shasum -a 256 | cut -c1-64; fi
}

# One hash of every file under the given folders, names included.
# A folder's files and links, each by name with its own digest or target,
# so moving bytes between files or retargeting a link changes the result.
file_hashes() {
    (cd "$1" && { find . -type f -print0 | LC_ALL=C sort -z | xargs -0 -r sha256sum --
        find . -type l -printf '%p -> %l\n' | LC_ALL=C sort; }) | sha256
}
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
    grep -E '^(CMAKE_BUILD_TYPE|CMAKE_TOOLCHAIN_FILE|PORT|DEVELOPER_MODE|ENABLE_[A-Z0-9_]+|USE_[A-Z0-9_]+|CMAKE_(C|CXX)_FLAGS[A-Z_]*|CMAKE_(C|CXX)_STANDARD_LIBRARIES|Ruby_EXECUTABLE)(:[A-Z]+)?=' \
        "$BUILD/CMakeCache.txt" | LC_ALL=C sort
}

# Everything that can change what the engine compiles to, one line each.
engine_inputs() {
    gcc="$GCC16/prefix/bin/m68k-amigaos-gcc"
    if [ "$(git -C "$WEBKIT_SOURCE" rev-parse --show-toplevel 2>/dev/null)" = "$(cd "$WEBKIT_SOURCE" && pwd -P)" ]; then
        echo "webkit $(git -C "$WEBKIT_SOURCE" rev-parse HEAD)"
        echo "webkit-changes $( (git -C "$WEBKIT_SOURCE" diff HEAD -- Source CMakeLists.txt 2>/dev/null
            git -C "$WEBKIT_SOURCE" ls-files --others --exclude-standard -- Source 2>/dev/null | LC_ALL=C sort |
                while IFS= read -r f; do printf '%s %s\n' "$f" "$(sha256 < "$WEBKIT_SOURCE/$f")"; done) | sha256)"
    else
        # Not a git tree (a patched copy): every file decides, as nothing
        # else shows which patches it carries.
        echo "webkit-tree $(file_hashes "$WEBKIT_SOURCE/Source") $(sha256 < "$WEBKIT_SOURCE/CMakeLists.txt")"
    fi
    echo "patches $(tree_hash "$ROOT/webkit/patches")"
    echo "compiler $("$gcc" --version | head -n 1) $(sha256 < "$("$gcc" -print-prog-name=cc1plus)")"
    echo "binutils $("$GCC16/prefix/bin/m68k-amigaos-as" --version | head -n 1)"
    echo "toolchain-file $(sha256 < "$ROOT/toolchain/amigaos3-gcc16.cmake")"
    echo "icu-headers $(tree_hash "$OAB_DEPS/icu78-m68k-amigaos/include")"
    echo "openamiga-headers $(tree_hash "$OAB_DEPS/m68k-amigaos/include")"
    # opentls.library's API, as curl's OpenTLS backend was built against it
    # (curl's own headers are in openamiga-headers).
    echo "opentls-headers $(tree_hash "$ROOT/third_party/opentls/include")"
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
# A cache miss reads the compiler's -MD output instead of preprocessing the
# file a second time.
export CCACHE_DEPEND=true
EOF
    if [ -n "${OB_DISTCC_HOSTS:-}" ]; then
        command -v distcc >/dev/null 2>&1 || die "OB_DISTCC_HOSTS is set but distcc is not installed"
        # distcc reaches "@" hosts with ssh (and asks their distccd to run
        # a compiler by its path, which only this PC's SSH key can reach).
        # All jobs share one connection per host: a burst of new logins
        # trips the host's MaxStartups limit, and distcc then leaves the
        # host alone for a minute. BatchMode: a host that wants a password
        # fails instead of waiting.
        cat > "$BUILD/ob-distcc-ssh" <<'EOF'
#!/bin/sh
# Written by openamigabrowser's scripts/build-webcore.sh: ssh for distcc.
exec ssh -o BatchMode=yes -o ControlMaster=auto -o "ControlPath=$HOME/.ssh/ob-distcc-%C" -o ControlPersist=600 "$@"
EOF
        chmod +x "$BUILD/ob-distcc-ssh"
        # ccache adds -fpch-preprocess for files that use WebCore's
        # precompiled header; distcc's preprocessed copy then names the .gch
        # file, which the other PC does not have, and every remote compile
        # fails. Without it, compiles here still use the precompiled header
        # and the other PC compiles the headers in full.
        cat > "$BUILD/ob-distcc" <<'EOF'
#!/bin/sh
# Written by openamigabrowser's scripts/build-webcore.sh: distcc for ccache.
for a; do shift; [ "$a" = -fpch-preprocess ] || set -- "$@" "$a"; done
exec distcc "$@"
EOF
        chmod +x "$BUILD/ob-distcc"
        cat >> "$BUILD/ob-ccache" <<EOF
export CCACHE_PREFIX='$BUILD/ob-distcc'
export DISTCC_HOSTS='$OB_DISTCC_HOSTS'
export DISTCC_SSH='$BUILD/ob-distcc-ssh'
EOF
    fi
    # The precompiled header itself is compiled directly: a .gch made through
    # ccache's depend mode left WebCore's headers to be read twice in some
    # files (multiple definition errors with -m68020-60).
    cat >> "$BUILD/ob-ccache" <<'EOF'
for a; do case "$a" in *.gch) exec "$@" ;; esac; done
EOF
    echo "exec '$CCACHE' \"\$@\"" >> "$BUILD/ob-ccache"
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
    # WebKit uses no C++ modules. With Ninja, CMake 3.28 and later would
    # otherwise preprocess every C++ file a second time, uncached, to look
    # for them.
    set -- "$@" -DCMAKE_CXX_SCAN_FOR_MODULES=OFF
    if [ -f "$BUILD/CMakeCache.txt" ]; then
        # An existing folder keeps its options; only the launcher changes,
        # which the Makefile generator does not treat as a reason to rebuild.
        cmake "$BUILD" "$@"
        return
    fi
    case "${OB_OPTIMIZE:-js}" in
        size) set -- "$@" -DCMAKE_BUILD_TYPE=MinSizeRel ;;
        js) set -- "$@" -DCMAKE_BUILD_TYPE=MinSizeRel \
            -DCMAKE_PROJECT_INCLUDE="$ROOT/scripts/cmake/ob-optimize-js.cmake" ;;
        layout) set -- "$@" -DCMAKE_BUILD_TYPE=MinSizeRel \
            -DCMAKE_PROJECT_INCLUDE="$ROOT/scripts/cmake/ob-optimize-layout.cmake" ;;
        speed) set -- "$@" -DCMAKE_BUILD_TYPE=Release \
            "-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG" "-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG" ;;
        *) echo "OB_OPTIMIZE must be size, js, layout or speed" >&2; exit 2 ;;
    esac
    # Through CFLAGS, CXXFLAGS and LDFLAGS, which CMake puts before the
    # toolchain file's flags in a new folder's cache (so the engine's
    # fingerprint, which reads the cache, sees them).
    case "${OB_GC_SECTIONS:-0}" in
        0) ;;
        1) export CFLAGS="${CFLAGS:+$CFLAGS }-ffunction-sections -fdata-sections"
           export CXXFLAGS="${CXXFLAGS:+$CXXFLAGS }-ffunction-sections -fdata-sections"
           export LDFLAGS="${LDFLAGS:+$LDFLAGS }-Wl,--gc-sections" ;;
        *) echo "OB_GC_SECTIONS must be 0 or 1" >&2; exit 2 ;;
    esac
    # -mpcrel alone gives 16-bit (d16,pc) data references, which cannot
    # reach across a 70 MB program; with -fPIC they are 32-bit (bd,pc). Our
    # ld resolves pc-relative references only inside one hunk, hence the
    # one-hunk script.
    case "${OB_PCREL:-0}" in
        0) ;;
        1) export CFLAGS="${CFLAGS:+$CFLAGS }-mpcrel -fPIC"
           export CXXFLAGS="${CXXFLAGS:+$CXXFLAGS }-mpcrel -fPIC"
           export LDFLAGS="${LDFLAGS:+$LDFLAGS }-Wl,-T,$ROOT/toolchain/amigaos3-onehunk.ld"
           # the files GCC cannot build pc-relative stay absolute
           set -- "$@" -DCMAKE_PROJECT_WebKit_INCLUDE="$ROOT/scripts/cmake/ob-pcrel.cmake" ;;
        *) echo "OB_PCREL must be 0 or 1" >&2; exit 2 ;;
    esac
    cmake -S "$WEBKIT_SOURCE" -B "$BUILD" -G "$(generator)" "$@" \
      -DPORT=Amiga \
      -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain/amigaos3-gcc16.cmake" \
      -DENABLE_WEBCORE=ON \
      -DOPENBROWSER_WEBCORE_DIR="$ROOT/src/webcore"
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
    for target in "$@"; do
        if [ "$(generator)" = Ninja ]; then
            ninja -C "$BUILD" -j "$jobs" "$target" >> "$log" 2>&1 || { status=$?; break; }
        else
            "$GMAKE" -C "$BUILD" -j"$jobs" "$target" >> "$log" 2>&1 || { status=$?; break; }
        fi
    done
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
    # WebCore's bindings read WTF's preferences file, which CMake does not
    # list as their input: copy it first.
    run_build bindings 1 WTF_CopyPreferences WebCoreBindings
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

# curl built with HTTP/2 needs nghttp2, which the programs link only when
# libnghttp2.a is in the prefix (a curl without HTTP/2 links without it):
# say so here rather than with nghttp2_* undefined at the end of the link,
# and let CMake look again when libnghttp2.a has come or gone since it last
# did (src/webcore/CMakeLists.txt keeps what it found as OB_NGHTTP2).
check_curl() {
    lib="$OAB_DEPS/m68k-amigaos/lib"
    [ -f "$lib/libcurl.a" ] || die "no libcurl.a in $lib (openamigacurl's out/lib)"
    if grep -q nghttp2_session_ "$lib/libcurl.a" && [ ! -f "$lib/libnghttp2.a" ]; then
        die "$lib/libcurl.a is built with HTTP/2: copy libnghttp2.a from openamigacurl's out/lib beside it"
    fi
    want=
    [ -f "$lib/libnghttp2.a" ] && want=nghttp2
    if [ "$(sed -n 's/^OB_NGHTTP2:INTERNAL=//p' "$BUILD/CMakeCache.txt")" != "$want" ]; then
        cmake "$BUILD" > /dev/null
    fi
}

# The browser's targets without WebKit's: Ninja knows the engine is up to
# date; for Makefiles, CMake's "/fast" targets skip the walk through WebKit's
# targets (after letting CMake regenerate if a CMakeLists.txt changed).
browser_build() {
    name=$1
    shift
    check_engine
    check_curl
    if [ "$(generator)" = Ninja ]; then
        run_build "$name" "$JOBS" "$@"
    else
        "$GMAKE" -C "$BUILD" -s cmake_check_build_system
        run_build "$name" "$JOBS" $(for t in "$@"; do echo "$t/fast"; done)
    fi
}

# Folding (OB_FOLD, scripts/hunkfold.py). Our ld keeps every object's copy
# of a template function the object calls itself: bfd/amigaos.c makes a hunk
# link-once only when its HUNK_EXT holds weak definitions alone, and
# --gc-sections keeps every hunk that defines a name something uses, not
# only the one the name resolved to. hunkfold rewrites copies of the
# archives so that only the copy the plain link used still defines each
# name; linked again, --gc-sections drops the others.
FOLD_PROGRAMS="OpenBrowser.engine obcore-view obcore-dump WebBrowserEngine"

links_with_gc() {
    sed -n 's/^CMAKE_EXE_LINKER_FLAGS:[A-Z]*=//p' "$BUILD/CMakeCache.txt" 2>/dev/null | grep -q -- --gc-sections
}

# Whether build-browser and build-tests fold their programs.
fold_wanted() {
    case "${OB_FOLD:-}" in
        0) return 1 ;;
        1) return 0 ;;
        "") links_with_gc && [ "$(generator)" = Ninja ] ;;
        *) die "OB_FOLD must be 0 or 1" ;;
    esac
}

file_size() { wc -c < "$1" | tr -d ' '; }

# fold_program NAME: fold the archives for one program ninja has linked, and
# link it again with its own link command, pointed at the folded archives.
# The winners come from that program's own map, so each program gets its
# own folded archives (a winner from another program's link could pull in a
# member this one never linked); "hunkfold.py members" checks that the
# folded link took exactly the plain link's archive members.
fold_program() {
    f_prog=$1
    f_bin="$BUILD/bin/$f_prog"
    f_out="$BUILD/lib-folded/$f_prog"
    if [ ! -f "$f_bin" ]; then
        echo "fold: no $f_bin, skipped"
        return
    fi
    f_sum=$(cksum < "$f_bin")
    if [ -f "$f_out/stamp" ] && [ "$(cat "$f_out/stamp")" = "$f_sum" ]; then
        echo "fold: $f_prog already folded"
        return
    fi
    f_cmd=$(cd "$BUILD" && ninja -t commands -s "bin/$f_prog") || die "fold: ninja has no link command for bin/$f_prog"
    f_map=$(printf '%s\n' "$f_cmd" | sed -n 's/.*-Wl,-Map=\([^ ]*\).*/\1/p')
    [ -n "$f_map" ] || die "fold: the link command of $f_prog writes no -Map"
    f_map="$BUILD/bin/$(basename "$f_map")"
    f_plain="${f_map%.map}.plain.map"
    [ -f "$f_map" ] || die "fold: no link map $f_map"
    f_strip=$(printf '%s\n' "$f_cmd" | sed -n 's/^: && \([^ ]*\)g++ .*/\1strip/p')
    [ -x "$f_strip" ] || f_strip="$GCC16/prefix/bin/m68k-amigaos-strip"
    f_log="$BUILD/logs/fold-$f_prog-$(date -u +%Y%m%dT%H%M%SZ).log"
    f_start=$(date +%s)
    hunkfold() { python3 "$HERE/hunkfold.py" "$@" >> "$f_log" 2>&1; }

    # The map of ninja's (plain) link names the winners.
    cp "$f_map" "$f_plain"
    rm -rf "$f_out"
    mkdir -p "$f_out"
    echo "== winners from $f_plain" > "$f_log"
    hunkfold winners "$f_plain" "$f_out/winners.txt" || die "fold: no winners from $f_plain (log: $f_log)"
    for f_lib in "$BUILD"/lib/lib*.a; do
        f_name=$(basename "$f_lib")
        echo "== $f_name" >> "$f_log"
        hunkfold fold "$f_lib" "$f_out/$f_name" --name "lib/$f_name" --winners "$f_out/winners.txt" \
            || die "fold: $f_name failed (log: $f_log)"
    done

    # The same link, with the folded archives, into bin/NAME.folding.
    f_cmd=$(printf '%s\n' "$f_cmd" | sed \
        -e "s# lib/\(lib[^ /]*\.a\)# lib-folded/$f_prog/\1#g" \
        -e "s#$BUILD/lib/\(lib[^ /]*\.a\)#$BUILD/lib-folded/$f_prog/\1#g" \
        -e "s#-Wl,-Map=[^ ]*#-Wl,-Map=$f_map#" \
        -e "s# -o bin/$f_prog # -o bin/$f_prog.folding #")
    case "$f_cmd" in
        *"lib-folded/$f_prog/"*"-o bin/$f_prog.folding "*|*"-o bin/$f_prog.folding "*"lib-folded/$f_prog/"*) ;;
        *) die "fold: cannot point the link command of $f_prog at the folded archives" ;;
    esac
    echo "== link" >> "$f_log"
    printf '%s\n' "$f_cmd" >> "$f_log"
    rm -f "$f_bin.folding"
    if ! (cd "$BUILD" && sh -c "$f_cmd") >> "$f_log" 2>&1; then
        cp "$f_plain" "$f_map"
        rm -f "$f_bin.folding"
        die "fold: linking $f_prog failed (log: $f_log); bin/$f_prog is ninja's"
    fi
    echo "== members" >> "$f_log"
    if ! hunkfold members "$f_plain" "$f_map"; then
        cp "$f_plain" "$f_map"
        rm -f "$f_bin.folding"
        die "fold: the folded $f_prog links other archive members (log: $f_log); bin/$f_prog is ninja's"
    fi
    echo "== copies, plain then folded" >> "$f_log"
    hunkfold copies "$f_plain"
    hunkfold copies "$f_map"

    f_before=$(file_size "$f_bin")
    f_after=$(file_size "$f_bin.folding")
    "$f_strip" -o "$f_out/stripped" "$f_bin"
    f_sbefore=$(file_size "$f_out/stripped")
    "$f_strip" -o "$f_out/stripped" "$f_bin.folding"
    f_safter=$(file_size "$f_out/stripped")
    rm -f "$f_out/stripped"
    mv "$f_bin.folding" "$f_bin"
    cksum < "$f_bin" > "$f_out/stamp"
    f_line="$(date -u +%Y-%m-%dT%H:%M:%SZ) fold $f_prog: $f_before -> $f_after bytes, stripped $f_sbefore -> $f_safter ($(( f_sbefore - f_safter )) less), $(( $(date +%s) - f_start )) s"
    echo "$f_line" >> "$BUILD/build-report.txt"
    echo "$f_line"
    echo "  log: $f_log"
}

fold_programs() {
    [ -f "$BUILD/build.ninja" ] || die "fold needs a build folder made with Ninja: $BUILD"
    links_with_gc || die "fold: $BUILD does not link with --gc-sections (OB_GC_SECTIONS=1 for a new folder)"
    command -v python3 >/dev/null 2>&1 || die "fold needs python3"
    mkdir -p "$BUILD/logs"
    for prog; do
        fold_program "$prog"
    done
}

build_browser() {
    browser_build browser OBCore OpenBrowser OpenBrowserLauncher
    if fold_wanted; then fold_programs OpenBrowser.engine; fi
    ls -l "$BUILD/bin/OpenBrowser" "$BUILD/bin/OpenBrowser.engine"
}

build_tests() {
    browser_build tests OBCore obcore-view obcore-dump
    if fold_wanted; then fold_programs obcore-view obcore-dump; fi
    ls -l "$BUILD/bin/obcore-view" "$BUILD/bin/obcore-dump"
}

# icu_data_file: the path of ICU's common data file, the one the programs
# read at start (OB_ICU_DATA=file). scripts/build-icu.sh installs it in
# share/; for a dependency folder made before that, it is taken out of
# libicudata.a, whose one object holds it byte for byte as its data.
icu_data_file() {
    icu="$OAB_DEPS/icu78-m68k-amigaos"
    dat="$icu/share/icudt78b.dat"
    if [ ! -f "$dat" ]; then
        tmp=$(mktemp -d)
        (cd "$tmp" && "$GCC16/prefix/bin/m68k-amigaos-ar" x "$icu/lib/libicudata.a") \
            || die "cannot read $icu/lib/libicudata.a"
        set -- "$tmp"/*.o
        [ $# -eq 1 ] || die "$icu/lib/libicudata.a holds $# objects, not one"
        mkdir -p "$icu/share"
        "$GCC16/prefix/bin/m68k-amigaos-objcopy" -O binary -j .data "$1" "$dat.tmp" \
            || die "cannot take the data out of $1"
        rm -rf "$tmp"
        [ "$(od -An -tx1 -j2 -N2 "$dat.tmp" | tr -d ' ')" = da27 ] \
            || die "$icu/lib/libicudata.a does not hold ICU data"
        mv "$dat.tmp" "$dat"
    fi
    echo "$dat"
}

test_browser() {
    : "${OB_TEST_DIR:?set OB_TEST_DIR to the folder to stage the tests in}"
    : "${OB_FONTS:?set OB_FONTS to the folder of the Liberation and DejaVu .ttf files}"
    set -- "$OB_FONTS"/*.ttf
    [ -f "$1" ] || { echo "no .ttf files in $OB_FONTS" >&2; exit 1; }
    strip="$GCC16/prefix/bin/m68k-amigaos-strip"
    mkdir -p "$OB_TEST_DIR/pages" "$OB_TEST_DIR/fontconfig" "$OB_TEST_DIR/Fonts"
    cp "$@" "$OB_TEST_DIR/Fonts/"
    for program in OpenBrowser OpenBrowser.engine obcore-view obcore-dump; do
        "$strip" -o "$OB_TEST_DIR/$program" "$BUILD/bin/$program"
    done
    cp "$ROOT"/tests/pages/*.html "$OB_TEST_DIR/pages/"
    cp "$ROOT/src/webcore/fontconfig/fonts.conf" "$OB_TEST_DIR/fontconfig/"
    cp "$ROOT/src/webcore/blocklist" "$OB_TEST_DIR/"
    if [ "$(sed -n 's/^OB_ICU_DATA:STRING=//p' "$BUILD/CMakeCache.txt")" != linked ]; then
        mkdir -p "$OB_TEST_DIR/Unicode"
        cp "$(icu_data_file)" "$OB_TEST_DIR/Unicode/"
    fi
    {
        echo "; OpenBrowser's test pages through obcore-view, staged by"
        echo "; scripts/build-webcore.sh test-browser. From this drawer:"
        echo "; \"Execute OBTests\"; the results go to OBTests.log"
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
        "$BUILD"/OpenBrowser/CMakeFiles/OpenBrowserLauncher.dir \
        "$BUILD"/OpenBrowser/CMakeFiles/obcore-view.dir "$BUILD"/OpenBrowser/CMakeFiles/obcore-dump.dir
    rm -f "$BUILD"/lib/libOBCore.a "$BUILD"/bin/OpenBrowser "$BUILD"/bin/OpenBrowser.engine \
        "$BUILD"/bin/obcore-view "$BUILD"/bin/obcore-dump "$BUILD"/bin/*.plain.map
    rm -rf "$BUILD"/lib-folded
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
    rm -f "$BUILD/engine-fingerprint" "$BUILD"/bin/*.plain.map
    rm -rf "$BUILD"/lib-folded
    echo "Removed every object from $BUILD"
}

case "$MODE" in
    configure-engine|configure) configure_engine ;;
    build-bindings) need_sources; build_bindings ;;
    build-engine) build_engine ;;
    build-browser) build_browser ;;
    build-tests) build_tests ;;
    fold) shift; [ $# -gt 0 ] || set -- $FOLD_PROGRAMS; fold_programs "$@" ;;
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
