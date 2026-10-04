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

