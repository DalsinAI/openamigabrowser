# The OpenBrowser plan

This is the one place that says where OpenBrowser is going and what comes
next. Every PR that finishes a step, or changes a decision, updates this file
in the same PR. The older documents (PHASE_PLAN.md, JS_ENGINE_PLAN.md,
WEBCORE_PORT.md) explain how we got here. Where they disagree with this file,
this file is right.

Last checked: 22 October 2026, against the engine built from main at 96f8c89
(`OpenBrowser.engine`, 90.2 MB stripped) and its link map.

## The decisions

Taken on 17 October 2026 (design 0.5) and still in force:

1. **OpenBrowser is an Amiga program.** It is WebKit's `PORT=Amiga`, WebCore
   and JavaScriptCore in one process on the 68040 with its FPU. Extra power
   comes from discrete services (autoconfig cores, the services card, a LAN
   Cradle), never from a browser running on the host.
2. **Nothing third-party except SQLite.** Every other library the engine
   links is one of ours.
3. **Text is OpenFont** (openfont.library). It replaces FreeType, HarfBuzz
   and fontconfig, and later takes over the work WebKit gives ICU.
4. **Drawing is OpenGfx and OpenGPU.** No cairo, no pixman.
5. **The network is openweb.fetch on OpenTLS** (openamigaservice). No curl,
   nghttp2 or libpsl.
6. **Every decoder is a datatype.** Pictures go through datatypes.library,
   and compressed bodies through gzip.datatype and brotli.datatype.
   Acceleration goes into the datatypes, never into side decoders in the
   browser.
7. **XML is OXP,** our own parser (openamigaxml).
8. **JavaScript has a 68k JIT,** our template JIT in WebKit's patches. It is
   also shared with other programs as javascript.library.

## The documents this file overrides

The design documents disagree with each other (inventory of 22 October
2026). Where they disagree, the decisions above are the ones in force:

- amigachrome `docs/design/Design-OpenBrowser-JIT-on-core.md` 0.5 and
  `Design-OpenBrowser-Lean-Engine.md` 0.2 (17 October) are current: only
  our own libraries and SQLite.
- `Design-OpenBrowser.md` 0.2 and the migration design 0.1 are older. They
  keep curl on OpenTLS, a trimmed ICU 78.3, and link OpenBrowser with
  pixman. None of that holds.
- **Decided 22 October (Dale):** unicode.library owns line and word
  breaking, NFC, IDNA, case mapping and the other Unicode properties.
  OpenFont stays fonts and shaping only, as `Design-OpenFont.md` 0.1 §3
  says. The browser designs that give breaking to OpenFont are overridden.

## Where each decision stands

| Decision | Our part | State of our part | In today's engine? | What still links the old one |
| --- | --- | --- | --- | --- |
| XML is OXP | OXP, openamigaxml #7-#9 (passes the W3C suite) | done | **yes** (patch 0030, #13) | nothing |
| Text is OpenFont | openfont.library 1.0 (20 calls): own font reader and rasteriser (openamigafont #9), shaper (#10, HarfBuzz 14.5.1 still inside), font index and fallback (#8) | built only in lab t4-a (17 Oct), in no release; OpenFontLibTest not yet passed on a lab. Short of what WebCore needs: UTF-8 input only, no script, language, direction or feature settings, A8 coverage only, no glyph outlines, no fonts from memory (web fonts), no colour or variable fonts | **no** | WebCore's FreeType, fontconfig and HarfBuzz files: FontCacheFreeType, FontSetCache, FontPlatformDataFreeType, FontCustomPlatformDataFreeType, GlyphPageTreeNodeFreeType, ComplexTextControllerHarfBuzz |
| Drawing is OpenGfx/OpenGPU | OpenGfx (openamigagfx: graphics.library 47.100 and layers.library 47.102, clean room), opengpu.library 0.16 (openamigartg), pixman.library 2.0 (25 calls over OpenGPU, built only in lab t4-g-px) | OpenGfx has graphics.library's API: no anti-aliased paths, gradients, alpha compositing or glyph runs, and nothing deeper than 8 planes yet. OpenGPU's command stream has fill, copy, mask and premultiplied compositing, but no anti-aliased paths, gradients, scaled or filtered images, blur or blend modes. openamigagfx main builds graphics.library 47.100 and layers.library 47.102 (rebuilt on the dev machine 22 Oct). No GraphicsContext exists on either; GraphicsContextOpenGfx and DrawGlyphRun are in design documents only | **no** | WebCore's cairo GraphicsContext and image surfaces |
| Network is openweb.fetch | openweb.fetch/1 (openamigaservice `src/webfetch.c`, `docs/OPENWEB_FETCH.md`, v1 12 Oct) | runs on the services card only, and its TLS is the host's OpenSSL 3, not OpenTLS. It has cookies (passes every Set-Cookie through), keep-alive pooling, host-side gzip, deflate and Brotli, and 64 fetches at once. It lacks HTTP/2, WebSocket, proxies, HTTP auth and a LAN transport. No `OWFetch` binary on any drive; no WebCore network backend on it | **no**: today curl 8.22 on opentls.library 1.3 (patch 0022), HTTP/2 through nghttp2 (0023), bsdsocket.library from OpenSocket | WebCore's CurlContext and curl loader |
| Decoders are datatypes | picture datatypes (openamigaimage); stream.datatype, gzip.datatype, brotli.datatype | pictures: yes. The stream class and the gzip and Brotli datatypes are a design only (JIT-on-core 0.5 §5.1), with no code in any repo | partly | libpng (for cairo) and zlib are still linked; curl decodes compressed bodies |
| JavaScript JIT | patches 0020-0052 | in the browser | **yes** | nothing |
| javascript.library | #63, #64 | built and tested (22 Oct: EVAL, errors, .jsc cache 7.5 s to 0.14 s) | **no**: the browser still links its own JavaScriptCore | the browser's build |
| ICU | the data file (#52); later unicode.library (breaking, NFC, IDNA, properties) | the data file works. **No unicode.library exists**: no repo, branch, PR or binary was found on 22 October; `src/webcore/ob_icudata.c` names it as the later way to share the data. | **no**: the 5.2 MB of data is still built in | the build options |

What the old libraries cost in today's engine, from its link map (MiB):

| Library | Size | Goes when |
| --- | --- | --- |
| ICU (code 3.4 + data 5.2) | 8.6 | data: step 2; code: step 7 |
| HarfBuzz | 1.16 | step 3 |
| FreeType | 0.50 | step 3 |
| fontconfig, with Expat | 0.45 | step 3 |
| cairo, pixman, libpng | 0.94 | step 6 |
| curl, nghttp2, libpsl | 0.54 | step 5 |
| JavaScriptCore | 21.1 | step 4, into javascript.library |

WebCore's own cairo, FreeType and curl platform code (inside WebCore's
47 MB) goes along with the libraries.

## The work, in order

Each step is one or more PRs here (in `webkit/` patches and `src/`). A step
is done when its "done when" holds on the lab instance.

1. **Write this down.** This file. Done when it is merged and linked from
   the README.
2. **ICU data from the file in both engines.** Turn on #52's option in the
   browser engine's build and in JavaScriptEngine's.
   Done when neither binary has `icudt78_dat` in its map, and both still
   pass their tests. Saves 5.2 MB in each.
3. **Text on OpenFont.** First, finish what openfont.library lacks for
   WebCore: UTF-16 input, shaping with script, language, direction and
   features, fonts opened from memory (web fonts), glyph outlines or a
   path call, and a passing OpenFontLibTest on a lab. Then a `PORT=Amiga`
   font backend (FontCache, FontPlatformData, GlyphPage and the shaping
   path) that calls it. Done when FreeType, HarfBuzz, fontconfig and Expat
   are gone from the engine's map and the benchmark pages render the same.
4. **The browser on javascript.library.** Before the switch, build the
   library the way the browser is built: MinSizeRel with -O2 for
   JavaScriptCore (#57), and with the 68k JIT patches. Today
   JavaScriptEngine is built Release (-O3), and whether it carries the JIT
   patches is not checked. Done when the browser engine links no
   JavaScriptCore of its own (about 22.6 MB smaller, 90.2 to about 67.6 MB)
   and the benchmarks are no slower.
5. **Network on openweb.fetch.** First, give openweb.fetch what the
   browser needs: TLS through OpenTLS instead of the host's OpenSSL,
   HTTP/2, WebSocket, proxies, HTTP auth, and a transport that works
   without a services card (bsdsocket.library on the Amiga itself, or a LAN
   Cradle). Next, build the datatypes stream class with gzip.datatype and
   brotli.datatype, so compressed bodies are decoded the Amiga way. Then a
   WebCore network backend in place of CurlContext and the curl loader.
   Done when curl, nghttp2 and libpsl are gone from the engine's map.
6. **Drawing on OpenGfx and OpenGPU.** First, give OpenGPU what a
   GraphicsContext needs: anti-aliased path coverage, gradients, scaled and
   filtered images, and blend modes; also deeper-than-8-plane bitmaps in
   OpenGfx. Then a GraphicsContext in place of cairo's. This is the largest
   step: all of WebCore's painting goes through it. Done when cairo, pixman
   and libpng are gone from the map.
7. **unicode.library, then ICU out.** Make unicode.library (none exists
   yet): one shared copy of ICU's data and its calls for every program,
   starting with the browser engine and JavaScriptEngine. Then replace
   ICU's code with our own (line and word breaking, NFC, IDNA, Unicode
   properties), in unicode.library, so that ICU leaves both engines. Done when the only
   third-party code in the engine's map is SQLite.

Not in the order above, still open:

- **#6, the visit cache (patch 0018).** Open since 7 October: finish it or
  close it.
- **Two copies of ICU's code** while the browser runs on javascript.library
  (about 3.4 MB each once step 2 has moved the data out), until step 7.
- **The inventory of 22 October 2026** (every repo, PR, design document and
  lab) is the evidence behind the "where each decision stands" table. It is
  kept outside the repositories, in INVENTORY-20261022.

## How to check

Sizes come from the link maps and from stripping copies with the os32-gcc16
stove's `m68k-amigaos-strip`. A map is grouped by archive (`lib*.a`), and an
archive that is still in the map is still linked. The javascript.library
checks run in a lab copy of an OS 3.2.3 instance (`scripts/lab_instance.py`).
