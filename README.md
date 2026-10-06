# OpenBrowser

A WebKit-based web browser for AmigaOS 3.2 (68020+ with FPU).

**Status, 4 October 2026:** WebKit's engine, WebCore, draws real pages on
AmigaOS 3.2.3. It lays out HTML and CSS, runs the page's JavaScript
(JavaScriptCore) and paints with cairo, FreeType and HarfBuzz into a 32-bit
bitmap; pictures are decoded by the system's datatypes. The browser window,
**OpenBrowser** (GadTools), and its network layer (libcurl with AmiSSL 5) are
built and being brought up on the test instance. See
[docs/WEBCORE_PORT.md](docs/WEBCORE_PORT.md).

JavaScriptCore on its own runs in a Shell:

```
1> Stack 4194304
1> jsc -e "print(1+1)"
2
```

**OpenBrowser Lite**, the first browser, is a GadTools window that opens
http:// and https:// pages (bsdsocket.library and AmiSSL 5) and shows them as
text, with numbered links, Back and Reload. Other programs open pages in it
through its ARexx port, `AMIGACHROME.BROWSER`:

```
1> rx "address AMIGACHROME.BROWSER 'OPENURL http://info.cern.ch/'"
```

See [docs/PHASE_PLAN.md](docs/PHASE_PLAN.md) for what comes next.

## What is here

| Folder | What |
| --- | --- |
| `src/` | OpenBrowser Lite: the GadTools window (`ob_main.c`) and HTTP fetching (`ob_http.c`) |
| `src/webcore/` | OpenBrowser on WebCore: the window (`ob_browser.c`), the page layer (WebCore's clients, `ob_webview.h`), cookies and the network loader, and the test programs `obcore-dump` and `obcore-view` |
| `third_party/openmail/` | Network, HTML-to-text and stack code shared with OpenMail (MIT, Dalsin Limited) |
| `webkit/` | The pinned upstream revision and the AmigaOS/m68k patches for WebKit (JavaScriptCore and WebCore) |
| `toolchain/` | CMake toolchain file for the os32-gcc16 compiler |
| `scripts/` | `build-openbrowser.sh`, `build-icu.sh`, `build-jsc.sh` and `build-webcore.sh` |
| `icu/` | ICU 78.3 patch for AmigaOS 3.x |
| `stove/` | Notes and patches for the GCC 16.2 AmigaOS compiler, plus the compat library and headers it needs |
| `tools/` | `obfetch` (fetch a page from a Shell, for tests), probe runner for an AmigaChrome test instance, and stack/map helpers |
| `tests/` | JavaScript and C test programs |
| `docs/` | Porting notes and the phase plan |

No binaries are kept here, and no WebKit, ICU or AmigaOS files.

## Building

OpenBrowser Lite needs bebbo's amiga-gcc (GCC 6.5, NDK 3.2) and the AmiSSL 5
SDK's headers, and runs on a 68020 or better without an FPU:

```
STOVE=/path/to/amiga-gcc scripts/build-openbrowser.sh /path/to/AmiSSL/Developer/include
```

It needs a TCP/IP stack (bsdsocket.library) and, for https://, AmiSSL 5.

The JavaScript engine:

1. Build the os32-gcc16 compiler (see `stove/STOVE-NOTES.txt` and
   `stove/gcc-patches/`), then build `stove/compat` into
   `libos32compat.a`. Copy `stove/include/*.h` into the compiler's
   `m68k-amigaos/sys-include`.
2. Build ICU 78.3 for AmigaOS with `scripts/build-icu.sh`.
3. Check out WebKit at the pinned revision and apply the patch (see
   `webkit/README.md`).
4. Run `WEBKIT_SOURCE=... scripts/build-jsc.sh`.

OpenBrowser on WebCore needs, besides the compiler and ICU above, the
libraries from the OpenAmiga ports (cairo and pixman, FreeType, HarfBuzz,
fontconfig, Expat, libxml2, SQLite, curl, libpsl, zlib and libpng; see
https://github.com/DalsinAI/openamiga) built into one prefix, and the AmiSSL 5
SDK. Then apply the patches in `webkit/` and run
`WEBKIT_SOURCE=... OAB_DEPS=... AMISSL_SDK=... scripts/build-webcore.sh`.
The programs are large (about 100 MB each, stripped) and need a 68020 or
better with an FPU and plenty of Fast RAM.

The build has two halves. The engine (WebKit's WebCore, JavaScriptCore,
WTF, PAL and bmalloc) is built by `build-webcore.sh build-engine`, and only
when its fingerprint changes: the WebKit tree, the compiler, the libraries'
headers or the build options. OpenBrowser's own code is built by
`build-webcore.sh build-browser` (and `build-tests` for `obcore-view` and
`obcore-dump`), which compiles only OpenBrowser's files and relinks: about
15 seconds for a one-file change. The compiler runs through ccache when it is
installed, and a new build folder uses Ninja when it is installed. Every
build adds a line to `build-report.txt` in the build folder. The commands
and settings are listed at the top of `scripts/build-webcore.sh`.

## Licence

OpenBrowser's own code and documents are MIT, Copyright (c) 2026 Dalsin
Limited (see [LICENSE](LICENSE)). WebKit/JavaScriptCore, ICU and GCC parts keep
their own licences (see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).

If you fork this or base work on it, please keep the credit and say what your
work is based on.
