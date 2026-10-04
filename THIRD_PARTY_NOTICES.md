# Third-party notices

OpenBrowser's own code is MIT (see LICENSE). These parts are not ours and keep
their own licences:

| Path | What | Licence |
| --- | --- | --- |
| `webkit/patches/` | Changes to WebKit / JavaScriptCore source files | The licence of each patched file: LGPL-2 or BSD-style, as stated in its header in WebKit (https://github.com/WebKit/WebKit) |
| `icu/patches/` | Changes to ICU 78.3 source files | Unicode License v3 (ICU) |
| `stove/gcc-patches/` | Changes to GCC 16 (bebbo amiga-gcc, branch amiga16.2) and its newlib headers | GPL-3.0-or-later with the GCC Runtime Library Exception (GCC); newlib's own licence for `sys-include` |

Built programs also link, at build time on your machine (none of it is in this
repository):

- WebKit's WTF, bmalloc and JavaScriptCore (LGPL-2 / BSD);
- ICU 78.3 (Unicode License v3);
- libnix and libpthread from bebbo's amiga-gcc (libnix: public domain/its own
  terms; libpthread by Szilard Biro: zlib-style);
- libstdc++, libgcc and libatomic (GPL-3.0 with the GCC Runtime Library Exception).

A `jsc` binary built from this repository is a combined work that includes
LGPL-licensed JavaScriptCore; distribute it under the LGPL's terms (source and
relinking information available). JavaScriptCore stays a separate program; it
is not part of AmigaChrome's own core.
