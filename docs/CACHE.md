# The visit cache

Revisited pages should not cost what the first visit did: on a 68k the
network, the secure connection and reading big scripts are each seconds.

## HTTP disk cache (on)

WebKit patch 0018, `platform/network/amiga/AmigaDiskCache.cpp`, hooked into
the port's `ResourceHandleAmiga.cpp`. Responses are kept as files, one per
URL (named by the URL's SHA-1, in 16 drawers `0`-`f`), in `PROGDIR:Cache` of
the engine (`LIBS:WebBrowser/Cache` when installed, `Libs/WebBrowser/Cache`
in OpenBrowser's drawer), up to 32 MB; the least recently stored go first.

- A response is kept when HTTP allows it: a GET answered 200, not
  `no-store`, not `Vary: *`, at most 4 MB, with a lifetime or a validator
  (`ETag`, `Last-Modified`). Cookies were taken when it arrived; the kept
  copy has no `Set-Cookie`, as a browser's private cache does.
- On a revisit a fresh entry (by `Cache-Control`/`Expires`, or the usual
  heuristic from `Last-Modified`) comes from disk without asking the
  server. A stale one is revalidated with `If-None-Match` /
  `If-Modified-Since`: a `304` serves the file and refreshes its times.
- Reload (WebCore's `ReloadIgnoringCacheData`) and `no-cache` requests go
  to the network; back/forward may use stale entries, as HTTP allows.

`obcore-view -cache dir` turns it on for the test viewer and prints what
it does with each request (`OBCACHE fresh|stale-revalidate|miss|store|...`).

Measured on the 68040 bench, second visit against first:

| Page | First visit | Revisit |
| --- | --- | --- |
| three.js, lodash and moment from cdnjs (`DH1:OB/pages/libs.html`) | 16.6 s | 5.6 s |
| Wikipedia, Amiga article | 79 s | 61 s (14 files from disk, 5 revalidated) |
| login.live.com | 375 s | 312 s (Microsoft sent other script versions on the revisit) |

## Bytecode cache (built, off)

`bindings/js/CachedScriptSourceProviderAmiga.cpp` keeps a script's compiled
bytecode in the cache's `js` drawer (the jsc shell's `--diskCachePath`
scheme). On the 68k JavaScriptCore reads it and then compiles the script
again, appending a second copy, so it is off
(`setAmigaBytecodeCacheEnabled`) until that is understood. With the HTTP
cache, three.js still takes 3.8 s on a revisit; how much of that is parsing
and how much running it is the next thing to measure.

## Pictures (to do)

Decoded pictures kept as IFF, shared with the datatypes, so revisits skip
decoding; the format to be agreed with the datatypes work.
