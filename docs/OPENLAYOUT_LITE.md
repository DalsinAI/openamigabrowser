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

## Next integration slice

The working OpenBrowser Lite shell remains untouched on this branch so far.

Next:

1. add an OpenLayout viewport beside the current listview implementation;
2. measure text from the active RastPort/font;
3. draw display-list text/rules/backgrounds into the page area;
4. scroll by changing the viewport origin;
5. resolve a click through `ol_hit_action(..., OL_ACTION_ACTIVATE)`;
6. feed the resulting href into the existing navigation/history code;
7. add datatype-backed image handles;
8. move raster fills/masks to pixman.library as the renderer matures.

The full WebKit engine remains available for pages that require JavaScript or unsupported modern layout. The lightweight path does not grow into a second WebKit.
