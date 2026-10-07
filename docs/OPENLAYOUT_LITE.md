# OpenLayout Lite browser path

Status: first semantic adapter built, 7 October 2026.

This branch adds the first engine slice for the super-lightweight OpenBrowser path without changing the working GadTools browser window yet.

## Current pipeline

```text
HTML bytes
   |
ob_html_lite
   |
OpenLayout semantic tree
   |
OpenLayout layout + display list
```

The adapter preserves semantic structure rather than flattening a page to the old numbered-link text view.

Currently represented:

- title;
- body/div/section/article/header/footer/main/nav;
- headings;
- paragraphs;
- links with href + activate action;
- strong/bold, emphasis/italic, underline, code and span;
- blockquotes and preformatted text;
- unordered and ordered lists;
- images with src/alt/width/height metadata;
- line breaks and horizontal-rule fallback;
- table/row/cell semantic roles.

Scripts and style blocks are detected and skipped. They are never executed by this engine.

Tables are preserved semantically but counted as unsupported until OpenLayout has real table-grid geometry.

## Test

Host:

```sh
OPENLAYOUT_ROOT=/home/da1ek/openamigalayout ./scripts/build-html-lite-host.sh
```

AmigaOS 3.x / 68040 + FPU:

```sh
OPENLAYOUT_ROOT=/home/da1ek/openamigalayout ./scripts/build-html-lite-amiga.sh
```

The first m68k test binary is 37,216 bytes and verifies:

- entity decoding and title extraction;
- script/style detection;
- heading/paragraph/list/image display-list output;
- real link semantics and href;
- nearest-actionable hit testing;
- stable link identity after reflow from 360 CSS px to 180 CSS px.

## Live viewport milestone — 7 October 2026

The working OpenBrowser Lite shell now has a real OpenLayout viewport instead of the old numbered-text page listview.

The integration:

- measures text against the active Amiga RastPort/font;
- lays the semantic document to the actual page width;
- draws text, backgrounds, rules and image placeholders directly into the window RastPort;
- uses a GadTools vertical scroller over the OpenLayout document height;
- resolves mouse coordinates through semantic actionable hit-testing;
- resolves a clicked link relative to the current page and feeds it into the existing navigation/history path;
- keeps the existing network layer, history, Links... chooser and ARexx port.

The first real OS 3.2.3 / AC090 68040 qualification fetched today's 577-byte `http://example.com/`, laid it out and painted it in the OpenBrowser window on OpenRTG. The resulting browser binary is 80,900 bytes and does not contain WebCore or JavaScriptCore.

The semantic HTML/link test remains 37,216 bytes on m68k and verifies stable link identity through reflow plus actionable hit resolution.

## Next integration slice

1. decode and paint image nodes through Amiga datatypes rather than placeholders;
2. add better font/style selection while keeping the layout measurement callback authoritative;
3. move fill/mask/glyph raster work to `pixman.library`;
4. add local/headless golden-page pixel tests;
5. qualify click-through navigation on a linked real page in the guest;
6. add the explicit `Open in Full Browser` escalation for script-heavy/unsupported pages.

The full WebKit engine remains available for pages that require JavaScript or unsupported modern layout. The lightweight path does not grow into a second WebKit.
