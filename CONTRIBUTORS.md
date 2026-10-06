# Contributors

## Creator and maintainer

- **SacredTrees** ([@SacredTrees](https://github.com/SacredTrees)): created OpenBrowser, designs it and maintains it.

## The AmigaChrome team

We are the AI agents who build AmigaChrome alongside SacredTrees:

- **Agnus**, our coordinator, who keeps every thread moving.
- **Thufir**, **Kynes** and **Galen**, the earlier agents who started the work on SacredTrees's PC.
- **The Claude Code threads**, each one taking a piece of the work from design to release.

## Copyright holder

OpenBrowser's own code, documents and icons are Copyright (c) 2026 Dalsin
Limited, released under the MIT licence (`LICENSE`). Our patches to WebKit,
ICU and GCC stay under the licence of the files they change.
`THIRD_PARTY_NOTICES.md` carries the formal notices.

## Third-party work in this repository

OpenBrowser stands on other people's work. Each of these keeps its own
copyright and licence.

| Component | Where | Authors | Licence |
| --- | --- | --- | --- |
| WebKitLegacy's `BackForwardList`, adapted | `src/webcore/AmigaBackForwardList.cpp`, `.h` | Apple Inc., Torch Mobile Inc., Google, Inc. | BSD 2-clause (in each file's header) |
| WebKitLegacy's `WebResourceLoadScheduler`, adapted | `src/webcore/AmigaLoaderStrategy.cpp` | Lars Knoll, Dirk Mueller, Waldo Bastian, Apple Inc., Google Inc. | GNU LGPL 2 or later (in the file's header) |
| Key tables and keyboard handling from WebKit's WPE port | `src/webcore/AmigaEditorClient.cpp` | Igalia S.L. | BSD 2-clause (in the file's header) |
| Our patches to WebKit and JavaScriptCore | `webkit/patches/` | Dalsin Limited, to files by Apple Inc. and the WebKit contributors | Each patched file's own: LGPL 2 or BSD-style |
| Our patches to ICU 78.3 | `icu/patches/` | Dalsin Limited, to files by Unicode, Inc. and the ICU contributors | Unicode License v3 |
| Our patches to GCC 16 (bebbo's amiga-gcc, branch `amiga16.2`) and its newlib headers | `stove/gcc-patches/` | Dalsin Limited, to files by the GCC and newlib contributors | GPL-3.0-or-later with the GCC Runtime Library Exception; newlib's own licence for `sys-include` |

## From our other repositories

- **OpenMail** (`DalsinAI/openamigamail`): its message library, in `third_party/openmail/` (MIT, Dalsin Limited; `FROM` names the commit and our one change).

## Fetched at build time, not committed

- **WebKit** (WTF, bmalloc, JavaScriptCore, WebCore): pinned to `54fe1539718e` of https://github.com/WebKit/WebKit in `webkit/UPSTREAM`, by Apple Inc. and the WebKit contributors; LGPL 2 and BSD-style.
- **ICU 78.3** (icu4c), by Unicode, Inc. and the ICU contributors; Unicode License v3.
- **bebbo's amiga-gcc** (https://franke.ms/git/bebbo/amiga-gcc, GCC 16.2, branch `amiga16.2`), with libnix and libpthread (Szilard Biro, zlib-style); GCC's runtime libraries are GPL-3.0 with the GCC Runtime Library Exception.
- **The OpenAmiga ports** WebCore links: cairo and pixman, FreeType, HarfBuzz, fontconfig, Expat, libxml2, SQLite, curl, libpsl, zlib and libpng, each under its own licence; and **AmiSSL 5** (OpenSSL 3, Apache-2.0) at run time.

## Work we learned from

- **WebKit's curl network session** (`NetworkStorageSessionCurl`): `src/webcore/AmigaCookieJar.cpp` follows its cookie string logic.

Amiga, AmigaOS and other product names are trademarks of their respective
owners.
