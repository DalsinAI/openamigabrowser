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
   and JavaScriptCore on the 68040 with its FPU. Once step 4 lands,
   JavaScript runs in javascript.library's engine process
   (JavaScriptEngine), as the javascript.library design decided on 22
   October; everything else stays in the browser's process. Extra power
   comes from discrete services (autoconfig cores, the services card, a LAN
   Cradle), never from a browser running on the host.
2. **Nothing third-party except SQLite.** Every other library the engine
   links is one of ours, including what sits behind our libraries (no
   HarfBuzz inside openfont.library, for example). ICU is the one licence
   exception, as unicode.library, until step 7 replaces its code.
3. **Text is OpenFont** (openfont.library). It replaces FreeType, HarfBuzz
   and fontconfig. The Unicode work WebKit gives ICU belongs to
   unicode.library (22 October), not OpenFont.
4. **Drawing is OpenGfx and OpenGPU.** No cairo, no pixman.
5. **The network is openweb.fetch on OpenTLS** (openamigaservice). No curl,
   nghttp2 or libpsl.
6. **Every decoder is a datatype.** Pictures go through datatypes.library
   (and so does encoding, such as canvas `toDataURL` to PNG),
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
| Drawing is OpenGfx/OpenGPU | OpenGfx (openamigagfx: graphics.library 47.100 and layers.library 47.102, clean room), opengpu.library 0.16 (openamigartg), pixman.library 2.0 (25 calls over OpenGPU; builds from openamigapixman main, e4bd3ec) | OpenGfx has graphics.library's API: no anti-aliased paths, gradients, alpha compositing or glyph runs, and nothing deeper than 8 planes yet. OpenGPU's command stream has fill, copy, mask, premultiplied compositing, bilinear and affine scaled images and the 13 Porter-Duff operators (not yet on Vulkan), but no anti-aliased paths, gradients, blur or the PDF/HSL blend modes. openamigagfx main builds graphics.library 47.100 and layers.library 47.102 (rebuilt on the dev machine 22 Oct). No GraphicsContext exists on either; GraphicsContextOpenGfx and DrawGlyphRun are in design documents only | **no** | WebCore's cairo GraphicsContext and image surfaces |
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
2. **ICU data from the file in both engines.** The browser's build already
   defaults to it (`OB_ICU_DATA=file`, #52); a build folder configured
   before #52 has to be reconfigured. JavaScriptEngine needs the work
   itself: `scripts/build-javascript-engine.sh` reuses the jsc shell's link,
   so it needs #52's loader (`ob_icudata.c`) and stub (`ob_icustub.c`) and a
   load of the data file at start. Done when neither binary links the
   `libicudata.a` object (the stub's empty `icudt78_dat` stays, by design),
   and both still pass their tests. Saves 5.2 MB in each. The browser
   engine is done (the 22 October test build, 89.7 MB).

   **2a. Link and code shape (the rest of L0 and L2).** Decided on 17
   October in the Lean Engine design, sections 5.3, 6.1 and 7, and missed
   until 22 October. None of it changes WebCore's source:
   - fold COMDAT copies at link time: 5.8 MB of duplicate template copies
     (measured);
   - PC-relative calls and addresses (`bsr.l`, `(bd,pc)`) in place of
     absolute ones: about 0.79 million fewer relocations, 3.2 MB less of
     relocation tables (measured count);
   - `ALWAYS_INLINE` as plain `inline` and no `flatten` on the 68k, with
     the hot interpreter and style paths inlined by name: the JSON parser
     (3.06 MB) and the typed-array functions (1.67 MB) stop being giant
     single functions; 4 to 8 MB (estimate);
   - the Web Inspector out of the engine into webinspector.library, loaded
     on demand: 2.6 MB.
   - JSON.parse through json.library (patch 0058), so LiteralParser is no
     longer built: 1.27 MB.

   Done so far: folding (#78), the inspector left out of the build (patch
   0057, #80, 3.26 MB; no webinspector.library exists yet, so loading it
   on demand is still to do) and JSON.parse through json.library (patch
   0058, #81, 1,267,592 bytes;
   LiteralParser was about 1.0 MB in the map, not the 3.06 MB above). The
   engine now needs json.library in `LIBS:`. Patch 0056 (#83) makes
   forced inlining a plain inline outside JavaScriptCore, WTF and WebCore's
   style, layout and DOM code: 1,163,792 bytes, with the DOM bench within
   timer noise. Measured together, main with 0057 and 0058 is 79,535,312
   bytes stripped and folded, and 78,371,520 with 0056. Pc-relative code
   (`OB_PCREL=1`: -mpcrel -fPIC, linked as one hunk; #85) takes it to
   75,787,212 (2,584,308 bytes less): 864,276 fewer relocations (1,494,434
   to 630,158), for 0.70 MB more code, with the DOM bench within timer
   noise (1,527 against 1,567 ms, nine runs each). `OB_PCREL` is off by
   default, like `OB_GC_SECTIONS`.

   Section GC (`OB_GC_SECTIONS`, #45) is off by default; the 22 October
   test build turned it on, and it saves almost nothing on its own; with
   the duplicate template copies folded (`OB_FOLD`, on with it) the
   stripped engine is 5.95 MB smaller (89.7 to 83.7 MB); -O2 JavaScriptCore
   costs about 6 MB for its 1.1-1.65x. Done when each change is measured
   on the engine's map, and the test pages pass on lab copies of real
   instances.
3. **Text on OpenFont.** First, replace the HarfBuzz copy inside
   openfont.library's shaper with our own, and finish what it lacks for
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
6. **Drawing on OpenGfx and OpenGPU.** Much of what a GraphicsContext
   needs already exists; reuse it, don't write it again (check of 22 October):
   - anti-aliased path fill and clip: openamigafont `of_raster_a8()` (A8
     mask, curves, non-zero winding; even-odd winding still to add, for
     SVG, CSS and canvas `evenodd`), moved into OpenGPU, with its mask
     composite for clipping;
   - scaled and bilinear images: OpenGPU COMPOSITE BILINEAR and
     COMPOSITE_AFFINE (add premultiplied-with-transform and PAD edges);
   - the 13 Porter-Duff operators: OpenGPU COMPOSITE_PD (add the Vulkan
     back end);
   - glyphs: OpenGPU MASK (add a batched glyph run and a glyph cache);
   - shadows: openamigagfx `oc_shadow()`.

   Still to write: anti-aliased strokes, even-odd fill, linear, radial and
   conic gradients, blur, the PDF/HSL blend modes, OpenGfx bitmaps deeper
   than 8 planes, PNG encoding through the PNG datatype (canvas
   `toDataURL` and `toBlob`, today libpng through cairo), then a
   GraphicsContext in place of cairo's. pixman.library 2.0's
   DESIGN.md section 10 is the same gap list; this step tracks it. Before
   the step starts, clean up the duplication, with one home per job
   (decided by Dale, 22 October):
   - **opengpu.library** does all pixel work on chunky and RTG bitmaps:
     fill, blit, composite, scale, blends, glyphs, gradients, blur, and the
     anti-aliased path rasteriser (moved there from openfont), on its CPU,
     host and Vulkan back ends;
   - **OpenGfx** (graphics.library 47.100) keeps the graphics.library API
     and planar bitmaps only, and calls OpenGPU for RTG bitmaps;
   - **openfont** keeps fonts and shaping, and draws through OpenGPU;
   - OpenRTG `screens.c`, the host's `acrtg.c` and the UI gradient fillers
     (opengadtools, openlook, openintuition, gradientslider) call OpenGPU
     instead of their own copies;
   - the old OpenGfx 1.x code in opengpu.library (openamigartg
     `library/ogfx`) and the copied `ogfx_*` files go;
   - no cairo and no pixman (Dale, 22 October): WebCore draws on OpenGPU
     directly, and nothing in the browser uses pixman.library. This is the largest step: all of WebCore's
   painting goes through it. Done when cairo, pixman and libpng are gone
   from the map.
7. **unicode.library, then ICU out.** Make unicode.library (none exists
   yet): one shared copy of ICU's data and its calls for every program,
   starting with the browser engine and JavaScriptEngine. Then replace
   ICU's code with our own, in unicode.library: line and word breaking,
   NFC, IDNA, Unicode properties, and the Intl services JavaScriptCore
   uses (collation, number, date and plural formatting, locales and time
   zones, tested by `tests/c/icuprobe.cpp` and the compatibility page's
   Intl checks), so that ICU leaves both engines. Done when the only
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
checks run in a lab copy of an OS 3.2.3 instance (amigachrome's
`scripts/lab_instance.py`; in this repo, `tools/run-probe.sh`).
