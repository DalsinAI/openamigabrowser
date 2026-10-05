# WebCore on AmigaOS 3.2: port notes

Copyright (c) 2026 Dalsin Limited. MIT licence for this document.

Phase 2 of the [phase plan](PHASE_PLAN.md): WebCore, the engine inside
WebKit, built for AmigaOS 3.2 on 68k and run headless. These notes record
the decisions and the Amiga-specific findings, so later work does not have to
rediscover them.

## The port

WebKit's own ports split work over several processes (WebKit2), and
WebKitLegacy, the single-process API, is Mac-only now. OpenBrowser is a new
**`PORT=Amiga`** build of WebCore in one process, with its own small embedding
layer, the way MorphOS's and Haiku's browsers did it:

| Part | Choice | Why |
| --- | --- | --- |
| Drawing | cairo image surfaces with pixman | Proven on big-endian CPUs; Skia refuses to build for them |
| Text | FreeType, HarfBuzz and fontconfig | WebCore's Cairo backend uses all three |
| Fonts | Liberation and DejaVu, in `PROGDIR:Fonts` | Free licences; Liberation has the metrics of Arial, Times and Courier |
| Network | libcurl with AmiSSL 5 | WebCore's curl backend handles cookies, redirects and TLS |
| Images | the system's datatypes | Dale's rule: the formats are the ones the user has datatypes for |
| JavaScript | JavaScriptCore's C-loop interpreter | No JIT on 68k |

The port lives in the WebKit tree as a patch series (see `webkit/`). The
library ports are in their own repositories, listed in
[OpenAmiga](https://github.com/DalsinAI/openamiga).

## Findings

**FPU precision.** AmigaOS 3.2's ROM `mathieeesingbas.library` puts the FPU
in single precision (FPCR $40) in every task that opens it, and libraries such
as datatypes may open it while they work. cairo's fixed-point conversion then
fails silently: nothing is drawn. FPCR must be 0 after libraries are opened,
after datatype calls and in every thread. WTF does this at start and in each
thread; the datatypes decoder does it after each call.

**NDK macros.** The NDK's `exec/types.h` defines `IMPORT`, `STATIC`,
`GLOBAL` and `REGISTER` as macros, which break WebCore and JavaScriptCore
names (the parser's `IMPORT` token, for one). AmiSSL's and curl's headers
include it, so WebCore's prefix header includes it first and removes those
macros; `VOID` and `CONST` stay because later NDK headers use them.

**libpthread's macros.** Szilard Biro's `pthread.h` turns `read()`,
`close()`, `open()` and others into macros when `unistd.h` came first. That
breaks member functions such as `Document::close()`. Including `pthread.h`
before `unistd.h` keeps them off.

**`size_t` is `unsigned long`.** On AmigaOS it is a type of its own beside
`uint32_t` (`unsigned int`), so WTF's persistent coders gained an
`unsigned long` case.

**Sizes and alignment.** 68k aligns `int` and pointers to 2 bytes, so some of
WebCore's "this struct must stay this size" checks see a smaller struct. They
accept smaller-or-equal on 68k.

**AmiSSL from C++.** AmiSSL's headers turn every OpenSSL function into an
inline library call, which C++ such as `::BIO_new()` cannot use. WebCore is
built with `PROTO_AMISSL_H` defined, so the plain prototypes stay and calls go
through AmiSSL's stub library. Each task that uses AmiSSL opens it
(`OpenAmiSSLTags`, or `InitAmiSSL` in a subprocess).

**Sockets are not file descriptors.** libcurl's configure finds libnix's
`fcntl()` and `pipe()`; on bsdsocket sockets they fail. curl is built to use
`IoctlSocket(FIONBIO)` and a loopback socket pair.

**Datatypes read files.** OS 3.2's picture datatypes reject memory sources
(`DTST_MEMORY` gives `ERROR_OBJECT_WRONG_TYPE`), so the decoder passes each
picture through a file in `T:`. PNG, GIF and BMP decode exactly as on Linux,
including alpha and GIF transparency.

**AmigaDOS paths.** fontconfig and SQLite treat names without a leading `/`
as relative and rebuild them as POSIX paths. Both are patched to keep names
such as `PROGDIR:Fonts` and `T:a.db` as given.

**Shared memory.** AmigaOS has one address space, so WebCore's
`SharedMemory` is plain memory and never makes cross-process handles.

**Build parallelism.** CMake's Makefile generator can run one custom
command for two targets at once, which broke the CSS property generator. The
derived sources are generated first with one job (`make -j1
WebCoreBindings`), then the rest builds in parallel.

**Adding a WebCore source rebuilds all of WebCore.** CMake's Makefile
generator lists every object's precompiled-header options in the target's
`flags.make`, and every object (and the precompiled header) depends on that
file. A new file in `PlatformAmiga.cmake` therefore recompiles all of WebCore,
about an hour on the build PC. New Amiga code goes into a file that is
already listed (by `#include`), or waits for a change that needs a full
rebuild anyway.

**AmigaDOS names in fontconfig.** fontconfig joins its configuration
directory and file name with a slash, so an empty directory turned
`PROGDIR:fontconfig/fonts.conf` into `/PROGDIR:...`, and AmigaDOS asked for
a disk called `/PROGDIR`. The fontconfig patch keeps names with a volume as
they are.

**No volume requesters.** A program that asks AmigaDOS for a name that is
not mounted gets a "Please insert volume" requester, and waits until someone
answers it. OpenBrowser's programs set `pr_WindowPtr` to -1 at start, so
such a name just fails.

**TLS handshakes take seconds.** On the test instance's emulated 68k a TLS
handshake is 4 to 10 seconds of public-key arithmetic: an X25519 key exchange
takes about 1.2 s, P-256 about 2 s and P-384 about 7 s, and checking an ECDSA
certificate chain such as Cloudflare's about 7 s more. Servers hang up on a
client that takes too long (Cloudflare accepted 10 seconds and refused 20),
and curl then reports "Failed sending data to the peer" when it sends the
request. So the network thread runs one priority above the browser's, the
key exchange offers X25519 and P-256 only (login.live.com picks P-384 when it
is offered), curl keeps one connection per host, reused for the next request,
and four in all, so few handshakes run side by side, and the connect timeout,
which includes the handshake, is two minutes.

**UTF-16 in the CPU's byte order.** WebKit keeps 16-bit strings in the
CPU's byte order, but three places assumed little-endian: converting them to
UTF-8 (simdutf's `utf16le` functions in WTF), the HTML fast-path parser's SIMD
scan of 16-bit text, and JavaScriptCore's hex decoder. On a 68k the first
garbled every page holding a character beyond Latin-1, and the second turned
markup given to `innerHTML` or `insertAdjacentHTML` with such text into
literal text. WTF now uses simdutf's native-endian functions, and the two
scans take each character's low byte from the right half. The proper place
for that is `SIMD::findInterleaved()` itself, which is in WTF's prefix header,
so it waits for a change that rebuilds everything anyway.

**WebKit's network thread never ends.** Upstream keeps curl's thread for
the life of the program. An AmigaOS program does not end while one of its
threads runs, so the browser stops it (`stopAmigaNetwork()`) before it exits.

**`getenv()` sees Shell variables.** libnix's `getenv()` reads the
variables of the Shell that started the program (`Set`), not global ones
(`SetEnv`). WebKit's `WEBKIT_CURL_*` settings are given with `Set`.

## Speed on a 68k

Measured on the test instance (AmigaChrome's AC090, a 68040 with FPU, 256 MB),
4 October 2026:

| Step | Time | What it is |
| --- | --- | --- |
| Loading the program | about 40 s | 122 MB: 82 MB of code, 33 MB of ICU data, 1.6 million relocations, read through the emulated disk |
| WebCore starting | 2 to 3 s | after `main()` |
| The first text on a page | about 25 s | fontconfig scanning every font again, as its cache was never saved |
| A page with a script and no text | 4 to 9 s | local file |
| Each new https site | 4 to 10 s | the TLS handshake (see above) |

What is done about it:

- **The font cache works.** fontconfig's cache was never written under
  libnix (openamigafontconfig's cache patch), so every start scanned all the
  fonts. It now lives in `PROGDIR:fontconfig/cache`, which lasts across
  reboots, with `T:fontconfig` when the program's drawer cannot be written.
- **Less ICU data.** `scripts/build-icu.sh` keeps the items listed in
  `icu/data-keep.lst`: the root and English locales, every break rule and
  dictionary, normalisation, properties and converters. The data goes from
  33 MB to under 13 MB, and the program from 122 MB to 101 MB.
- **Fewer rendering updates while a page loads.** WebCore asks for a
  rendering update (style and layout of the whole page) after each piece of a
  page arrives. Until the page has loaded, OpenBrowser lets one through at
  most every second, or every twice as long as the last one took.
- **Cookies stay off the disk until needed.** The cookie database keeps its
  journal in memory and does not sync, instead of writing, syncing and
  deleting a journal file for every cookie.
- **Scripts and pictures can be switched off** (Settings menu, or the tool
  types `JAVASCRIPT=NO` and `PICTURES=NO`), the quickest way through a heavy
  page.
- **Fonts come from memory, unhinted.** FreeType read each glyph from the
  disk through stdio and hinted every glyph as it was drawn; a window sat at
  31% while its first text was laid out. On AmigaOS FreeType now reads a font
  file into memory once (openamigafreetype), and `fonts.conf` turns hinting
  off.
- **Two connections per host** (`0007` in `webkit/`), and the C-loop
  interpreter built with `-O2` while the rest is built for size.

**Infinity on AmigaChrome's AC090 FPU (fixed in AmigaChrome 0.30.0).** On
older AC090 builds an infinity compared unequal to itself. WTF's hash tables
mark empty double keys with +infinity, so a table keyed by a double never
found an empty slot and looped: www.bbc.co.uk stopped in
`Style::Resolver::keyframeRulesForName()`, found by sampling the stuck
task's stack (`tests` programs `fpinf` and `fpedge`, in the build tree).
Real 68881, 68882 and 68040 FPUs were never affected.

## Open problems

**login.live.com stalls after its page arrives (4 October 2026).** The page
comes back (HTTP 200, 33 KB), and then WebCore's main thread makes no more
progress: the page never reports its document finished, asks for none of its
scripts or preloads (all on logincdn.msauth.net), and the run loop's 15-second
heartbeat in `obcore-view` stops, so the thread is stuck inside one run-loop
turn, in parsing or script. The network thread is not the cause: polling
idle at its higher priority leaves the main task its time. example.com, whose
script adds paragraphs in several languages, loads and draws. Next steps:
sample the main task's program counter while it is stuck, or load a saved
copy of the page from disk and remove its inline scripts one at a time.

