#!/bin/sh
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 AmigaChrome Team. MIT licence, see LICENSE.
#
# OpenBrowser's drawer and archive, ready to copy to an Amiga and install
# with its own Install script (dist/OpenBrowser/Install).
#
#   package-browser.sh OUT_DIR
#
# Environment:
#   WEBKIT_BUILD   the WebCore build folder (scripts/build-webcore.sh); its
#                  bin/ holds OpenBrowser, OpenBrowser.engine and
#                  WebBrowserEngine (ninja WebBrowserEngine)
#   OS32_GCC16     the os32-gcc16 stove (for m68k-amigaos-strip)
#   OAB_DEPS       the dependency folder; ICU's data file comes from
#                  icu78-m68k-amigaos/share/icudt78b.dat (ICU_DATA overrides)
#   OB_FONTS       the Liberation and DejaVu .ttf files
#   OPENTLS_LIBS   folder with opentls.library and opencrypto.library
#                  (openamigatls build)
#   CA_BUNDLE      folder with ca-bundle.pem and ca-bundle.LICENSE
#                  (openamigatls; S:OpenTLS/ on an installed system)
#   WEBBROWSER_LIB webbrowser.library (default src/webbrowser/bin/, where
#                  src/webbrowser/build-library.sh puts it)
#   JSON_LIB       json.library (default build/json-library/, scripts/build-json-library.sh);
#                  the Installer puts it in LIBS:. Required: the engine's
#                  JSON.parse parses through it (WebKit patch 0058)
#   OPENFONT_LIB   openfont.library 1.4 or newer (openamigafont's
#                  build/amiga/openfont.library); the Installer puts it in
#                  LIBS:. Required: the engine draws and shapes all its text
#                  with it (WebKit patch 0090)
#   EXTRAS         optional: a folder with Libs/javascript.library,
#                  Libs/JavaScript/JavaScriptEngine, C/JS and
#                  Libs/json.library, which the Installer offers
#
# OUT_DIR/OpenBrowser/            the drawer: OpenBrowser, OpenBrowser.engine,
#                                 Unicode/, Network/, Fonts/, Libs/
#                                 (webbrowser.library, WebBrowser/, OpenTLS,
#                                 json and openfont),
#                                 S/OpenTLS/, Install, icons and readme
# OUT_DIR/OpenBrowser.info        the drawer's icon
# OUT_DIR/Install-OpenBrowser     the Installer script (SYS:System/Installer),
# OUT_DIR/Install-OpenBrowser.info  and its icon: double-click to install
# OUT_DIR/Extras/                 when EXTRAS is set
# OUT_DIR.lha                     the archive, beside OUT_DIR
#
# The engine reads ICU's data at start (OB_ICU_DATA=file), so the drawer
# carries it in Unicode/ and Install puts it in LOCALE:Unicode/. When
# webbrowser.library is installed, the launcher runs LIBS:WebBrowser's
# engine, not the drawer's, so Install replaces that one (keeping the old).
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${1:?usage: package-browser.sh OUT_DIR}
: "${WEBKIT_BUILD:?set WEBKIT_BUILD}" "${OS32_GCC16:?set OS32_GCC16}" "${OB_FONTS:?set OB_FONTS}"
: "${OPENTLS_LIBS:?set OPENTLS_LIBS}" "${CA_BUNDLE:?set CA_BUNDLE}"
ICU_DATA=${ICU_DATA:-"${OAB_DEPS:?set OAB_DEPS or ICU_DATA}/icu78-m68k-amigaos/share/icudt78b.dat"}
STRIP="$OS32_GCC16/prefix/bin/m68k-amigaos-strip"
BIN="$WEBKIT_BUILD/bin"
for f in "$BIN/OpenBrowser" "$BIN/OpenBrowser.engine" "$BIN/WebBrowserEngine" "$ICU_DATA" \
         "$OPENTLS_LIBS/opentls.library" "$OPENTLS_LIBS/opencrypto.library" "$CA_BUNDLE/ca-bundle.pem"; do
    [ -f "$f" ] || { echo "missing: $f" >&2; exit 1; }
done
rm -rf "$OUT"; mkdir -p "$OUT"
D="$OUT/OpenBrowser"; E="$D/Libs/WebBrowser"
mkdir -p "$D/Fonts" "$D/Unicode" "$D/S/OpenTLS" "$E/Fonts" "$E/Unicode"
"$STRIP" -o "$D/OpenBrowser" "$BIN/OpenBrowser"
"$STRIP" -o "$D/OpenBrowser.engine" "$BIN/OpenBrowser.engine"
"$STRIP" -o "$E/WebBrowserEngine" "$BIN/WebBrowserEngine"
WBLIB=${WEBBROWSER_LIB:-"$HERE/src/webbrowser/bin/webbrowser.library"}
if [ -f "$WBLIB" ]; then cp "$WBLIB" "$D/Libs/"; fi
for t in "$D" "$E"; do
    cp "$OB_FONTS"/*.ttf "$t/Fonts/"
    cp "$HERE/src/webcore/blocklist" "$t/"
    cp "$ICU_DATA" "$t/Unicode/icudt78b.dat"
    # the Public Suffix List for cookies and sites (WebKit patch 0070)
    mkdir -p "$t/Network"
    cp "$HERE/data/public-suffixes" "$t/Network/"
done
cp "$OPENTLS_LIBS/opentls.library" "$OPENTLS_LIBS/opencrypto.library" "$D/Libs/"
JSONLIB=${JSON_LIB:-"$HERE/build/json-library/json.library"}
[ -f "$JSONLIB" ] || { echo "package-browser.sh: no $JSONLIB (scripts/build-json-library.sh); the engine's JSON.parse needs it" >&2; exit 2; }
cp "$JSONLIB" "$D/Libs/"
OPENFONTLIB=${OPENFONT_LIB:?set OPENFONT_LIB to openfont.library 1.4 (openamigafont); the engine draws its text with it}
[ -f "$OPENFONTLIB" ] || { echo "package-browser.sh: no $OPENFONTLIB" >&2; exit 2; }
cp "$OPENFONTLIB" "$D/Libs/openfont.library"
cp "$CA_BUNDLE/ca-bundle.pem" "$D/S/OpenTLS/"
[ -f "$CA_BUNDLE/ca-bundle.LICENSE" ] && cp "$CA_BUNDLE/ca-bundle.LICENSE" "$D/S/OpenTLS/"
cp "$HERE/dist/OpenBrowser/Install" "$D/Install"
cp "$HERE/icons/OpenBrowser.info" "$OUT/OpenBrowser.info"
cp "$HERE/dist/Install-OpenBrowser" "$HERE/dist/Install-OpenBrowser.info" "$OUT/"
if [ -n "${EXTRAS:-}" ]; then cp -R "$EXTRAS" "$OUT/Extras"; fi
cp "$HERE/icons/OpenBrowser/OpenBrowser.info" "$HERE/icons/OpenBrowser/OpenBrowser.readme.info" "$D/"
cp "$HERE/openbrowser.readme" "$D/OpenBrowser.readme"
cp "$HERE/THIRD_PARTY_NOTICES.md" "$HERE/LICENSE" "$D/"
[ -f "$D/Libs/webbrowser.library" ] || echo "note: no $WBLIB (src/webbrowser/build-library.sh); the drawer runs its own engine without it"
if command -v jlha >/dev/null 2>&1; then
    (cd "$(dirname "$OUT")" && rm -f "$(basename "$OUT").lha" && jlha ao2q "$(basename "$OUT").lha" "$(basename "$OUT")" >/dev/null)
    echo "$OUT.lha"
else
    echo "no jlha (Ubuntu: jlha-utils): the drawer is in $OUT, unpacked" >&2
fi
