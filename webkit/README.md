# WebKit for OpenBrowser

WebKit is not copied into this repository. Use the pinned upstream revision
in `UPSTREAM` and apply the patches in `patches/` in order.

```sh
git clone --filter=blob:none --no-checkout https://github.com/WebKit/WebKit.git
cd WebKit
git sparse-checkout set --cone CMakeLists.txt Source/cmake Source/WTF Source/bmalloc Source/JavaScriptCore Source/WebCore Source/ThirdParty Tools
git checkout 54fe1539718eef2f24d813019949ca8cd4b9e89e
git apply /path/to/openamigabrowser/webkit/patches/0001-amiga-m68k-jsconly.patch
git apply /path/to/openamigabrowser/webkit/patches/0002-amiga-webcore.patch
git apply /path/to/openamigabrowser/webkit/patches/0003-jsc-llint-prototype-cache-big-endian.patch
git apply /path/to/openamigabrowser/webkit/patches/0004-amiga-webcore-network.patch
git apply /path/to/openamigabrowser/webkit/patches/0005-utf16-big-endian.patch
git apply /path/to/openamigabrowser/webkit/patches/0006-amiga-disk-webp-quiet.patch
git apply /path/to/openamigabrowser/webkit/patches/0007-amiga-host-connections-llint-o2.patch
git apply /path/to/openamigabrowser/webkit/patches/0008-jsc-for-in-big-endian.patch
git apply /path/to/openamigabrowser/webkit/patches/0009-amiga-stop-run-loops-at-exit.patch
git apply /path/to/openamigabrowser/webkit/patches/0010-amiga-fetch-proxy-experiment.patch
git apply /path/to/openamigabrowser/webkit/patches/0011-service-decoder-and-svg-pictures.patch
git apply /path/to/openamigabrowser/webkit/patches/0012-amiga-font-match-memo.patch
git apply /path/to/openamigabrowser/webkit/patches/0013-wtf-compactvariant-big-endian.patch
git apply /path/to/openamigabrowser/webkit/patches/0014-jsc-weakhandleowner-align4.patch
git apply /path/to/openamigabrowser/webkit/patches/0015-jsc-m68k-interpreter.patch
git apply /path/to/openamigabrowser/webkit/patches/0016-amiga-thread-names-and-clean-exit.patch
git apply /path/to/openamigabrowser/webkit/patches/0017-jsc-m68k-interpreter-registers.patch
git apply /path/to/openamigabrowser/webkit/patches/0018-amiga-visit-cache.patch
```

Applied in order, 0001 to 0017 give exactly the tree the Team builds from
(checked on 7 October 2026 against the working WebKit tree). 0018 is work
in progress.

The patches, in order:

- `0002-amiga-webcore.patch`: WebCore's Amiga port (`PORT=Amiga`): build
  options, the platform files (screen, keyboard, pasteboard, MIME types,
  user agent, fonts), pictures through the system's datatypes, cairo
  drawing without accelerated compositing, and the network layer over
  WebCore's curl backend. Notes are in `../docs/WEBCORE_PORT.md`.
- `0003-jsc-llint-prototype-cache-big-endian.patch`: a JavaScriptCore fix
  for big-endian 32-bit CPUs; without it, methods found on a prototype
  (`a.push`, `s.concat`) read back as `undefined` after their first cached
  call.
- `0004-amiga-webcore-network.patch`: the network layer running on AmigaOS:
  the curl delegate's type, no large-file requirement (libnix's `off_t` is
  32-bit), stopping curl's thread before the program exits, and settings
  for slow TLS handshakes on a 68k (X25519 and P-256 key exchange, one
  connection per host and four in all, a 120 s connect timeout).
- `0005-utf16-big-endian.patch`: fixes for big-endian CPUs, where 16-bit
  strings were converted to UTF-8 as if little-endian (garbling any text
  beyond Latin-1) and the HTML fast-path parser could not find tags in
  16-bit text.
- `0006-amiga-disk-webp-quiet.patch`: the cookie database keeps its journal
  in memory instead of on the disk, WebP sizes come from the file's header
  so a picture is decoded only when drawn, and a missing picture no longer
  writes a warning to stderr.
- `0007-amiga-host-connections-llint-o2.patch`: two connections per host
  instead of one (one left pictures queued long enough to time out), and
  the C-loop interpreter, which runs every line of JavaScript, built with
  `-O2`.
- `0008-jsc-for-in-big-endian.patch`: `for (k in o)` loops end. The
  interpreter's pointer comparisons (`op_jeq_ptr`, `op_jneq_ptr`) read
  half of an 8-byte value on big-endian CPUs with 32-bit pointers, so a
  for-in loop never saw the value that ends it: it ran forever, and reading
  `o[k]` inside it crashed the program.
- `0009-amiga-stop-run-loops-at-exit.patch`: libpthread waits for every
  thread when a program exits, and a work queue's thread waits for work for
  ever, so a program that had made one never exited. The generic run loop
  keeps a list on AmigaOS, and `amigaStopAllRunLoops()` stops them all.
- `0010-amiga-fetch-proxy-experiment.patch`: the host-fetch experiment.
  With the Shell variable `OB_FETCH_PROXY` set (see
  `scripts/ob-fetch-proxy.py`), requests go to a proxy on another PC, which
  makes the TLS connections. Off unless the variable is set.
- `0011-service-decoder-and-svg-pictures.patch`: pictures go to a service
  decoder (Nursery's `media.decode/1`) before the datatypes, and SVG
  pictures are drawn.
- `0012-amiga-font-match-memo.patch`: fontconfig's match is remembered for
  each family, weight and slant, instead of being worked out again for each
  run of text.
- `0013-wtf-compactvariant-big-endian.patch`: `CompactVariant` keeps its
  payload in the low bytes on big-endian CPUs.
- `0014-jsc-weakhandleowner-align4.patch`: `WeakHandleOwner` aligned to 4
  bytes (the 68k's default alignment is 2).
- `0015-jsc-m68k-interpreter.patch`: the C loop's interpreter as 68k code
  (an M68K back end for offlineasm), on by default.
- `0016-amiga-thread-names-and-clean-exit.patch`: thread tasks are named on
  AmigaOS, and run-loop threads finish so the engine exits cleanly.
- `0017-jsc-m68k-interpreter-registers.patch`: the 68k interpreter keeps its
  hot interpreter registers in 68k registers (pointers in address registers).
- `0018-amiga-visit-cache.patch`: work in progress. An HTTP disk cache for
  pages already visited (on), and a cache of compiled scripts (built, but
  switched off). The browser side is `../docs/CACHE.md`.

`0001-amiga-m68k-jsconly.patch` is one combined patch. It holds:

- the m68k CPU and AROS platform layer from the AmigaChrome AROS m68k port;
- the AmigaOS 3.x platform layer (`OS(AMIGAOS3)`, and `OS(AMIGA)` for code
  shared with AROS);
- the big-endian and m68k-alignment fixes (see `../docs/PORT_NOTES.md`).

Each patched file keeps its WebKit licence header (LGPL-2 or BSD).
