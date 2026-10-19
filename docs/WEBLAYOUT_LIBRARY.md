# weblayout.library: the proposal

Status: a proposal for approval, 19 October 2026. Nothing here ships as a
system library yet. OpenBrowser Lite 0.4.1 has the engine split out in
`src/weblayout/` behind `src/weblayout/weblayout.h`, linked into the
program; this page says what it would take to make that `LIBS:weblayout.library`.

## Why

Dale, 19 October 2026: "lite should almost create the bones of a
weblayout.library". A web page laid out and drawn into a RastPort is useful
to more than one program: OpenMail's HTML mail, OpenView's help pages,
AmigaGuide-like readers, installers' release notes, OpenUp's own "what's
new". Each of them today either flattens HTML to text or ships its own
renderer. One small resident engine, with no JavaScript and no network of
its own, is the Amiga way to share that.

## The layers

```text
the program        fetching (HTTP, files), history, windows, gadgets, menus
---------------------------------------------------------------- weblayout.h
weblayout          WLDocument: HTML Lite + CSS Lite -> semantic tree,
                   layout at a width, painting into a RastPort area,
                   pictures through picture.datatype, link hit-testing,
                   address resolution
OpenLayout         geometry, display list, hit-testing (openamigalayout;
                   built in, or the resident openlayout.library 2.0)
the OS             graphics.library, layers.library, datatypes.library
```

| Source (src/weblayout) | What it does |
| --- | --- |
| `weblayout.h` | the public calls; the only header a program includes |
| `wl_document.c` | the calls, over the parts below |
| `wl_html.c` | HTML Lite: tokeniser, entities, the semantic tree (headings, paragraphs, lists, links, images, pre, tables as roles) |
| `wl_css.c` | CSS Lite: element, class, ID and compound selectors, specificity and order, inline `style=`; colours, sizes, margins, padding, alignment, display |
| `wl_paint.c` | the display list painted into a RastPort, clipped to the page's area; text measured with the RastPort's font |
| `wl_image_dt.c` | page pictures through picture.datatype, remapped and scaled |
| `wl_url.c` | relative addresses made absolute (plain C) |
| `wl_openlayout.c/.h` | OpenLayout built in, or through openlayout.library |

What stays in the program: the network (OpenBrowser Lite's `ob_http.c` on
OpenMail's bsdsocket/OpenTLS layer), charsets to UTF-8, temporary picture
files, history, the window and its gadgets, the scroller, the ARexx port and
the Links... chooser.

## The calls

All of them are in `weblayout.h` today with these names and arguments; as a
library they keep them and gain a base.

| Call | What |
| --- | --- |
| `WL_NewDocument(data, length, kind, base_url)` | a page from UTF-8 bytes (`WLKIND_HTML` or `WLKIND_TEXT`), its own address for relative ones; the bytes are copied |
| `WL_DisposeDocument(doc)` | everything the document holds, pictures too |
| `WL_GetTitle(doc)` | `<title>`, UTF-8 |
| `WL_CountResources(doc, kind)` / `WL_GetResource(doc, kind, i, &res)` | what the page asks for: `WLRES_STYLESHEET` (after creation) and `WLRES_IMAGE` (after layout, with its box), as absolute addresses |
| `WL_AddStyleSheet(doc, css, length)` | CSS the program fetched; the next layout builds the page again with it (256 KiB at most in all) |
| `WL_Layout(doc, &view)` | lay out at `wv_Width`, measuring with `wv_Font` in `wv_RPort` |
| `WL_ContentHeight(doc)` | for the scroller |
| `WL_SetImageFile(doc, id, path, screen)` | decode a picture the program fetched to a file, for the screen |
| `WL_Draw(doc, &view)` | paint into the view's area at `wv_ScrollY`; a NULL doc clears it |
| `WL_HitLink(doc, &view, x, y)` | the absolute address of the link under the pointer |
| `WL_ResolveURL(base, href, out, len)` | the resolver on its own |
| `WL_Open()` / `WL_Close()` | today: the resident OpenLayout and datatypes; as a library these go into its Open and Expunge |

`struct WLView` (RastPort, font, the area, scroll, four pens) and
`struct WLResource` carry their own size (`wv_Size`, `wr_Size`), so later
versions can add fields without breaking older programs.

## Ownership

- A `WLDocument` belongs to the task that made it and owns everything in it:
  the copied bytes, the CSS added, the OpenLayout tree, the decoded
  pictures. `WL_DisposeDocument` frees all of it.
- Strings the library hands out (`WL_GetTitle`, `wr_URL`, `WL_HitLink`) are
  the document's: a picture's `wr_URL` and `WL_HitLink`'s answer are good
  until the next such call on the same document; a stylesheet's and the
  title until disposal. Copy them to keep them.
- The program keeps a picture's file until it disposes of the document
  (picture.datatype may read it lazily).
- The RastPort, font and screen are the program's; the library only draws
  through them between the call's start and end.

## Threading

- One document is used by one task at a time; the library takes no locks
  on a document. Two tasks with two documents need nothing.
- As a library, its own global state is only the bases it opens
  (graphics, layers, datatypes, openlayout), opened in its LibOpen.
- No call waits on the network or a signal; the slow calls are
  `WL_Layout` (text measuring) and `WL_SetImageFile` (decoding). A program
  that wants to stay responsive runs those on its own worker process.

## Before it can be a library

1. Memory: the engine uses the C library's malloc and free (libnix). A
   resident library cannot rely on a caller's C library: it needs an
   Exec-pool runtime, as openlayout.library's `openlayout_runtime.c`
   already has. The work: route `wl_*.c`'s allocations through one
   allocator (`wl_alloc`/`wl_free`) and give the library build a pool per
   document.
2. libc in the engine: `snprintf`, `strtol`, `ctype` and `strcasecmp`
   need the same treatment (small own versions, or `utility.library`).
3. `oam_utf8_to_latin1` (OpenMail's `oam_text.c`) comes into
   `wl_paint.c` as its own small converter.
4. The SFD, the LVO table, `proto/`, `inline/` and `clib/` headers, made
   the way openamigalayout's `library/openlayout_lib.sfd` and OpenSDK make
   them; then an OpenNDK entry and an OpenUp part (`LIBS:weblayout.library`,
   openlayout.library beside it).
5. Tests: the Amiga test programs open the library from `LIBS:` (as
   openamigalayout's `LayoutLibTest` does), and the host tests stay.

None of that changes `weblayout.h`'s calls, which is the point of fixing
them now.

## Open questions for approval

- The name: weblayout.library (proposed) or a version of openlayout.library
  with the web parts in it.
- Where the source lives: here in `src/weblayout` (proposed while
  OpenBrowser Lite is its only user) or a repository of its own when a second
  program uses it.
- Whether pictures should also come from memory (`WL_SetImageData`), which
  needs picture.datatype's memory source (DTST_MEMORY, datatypes 44+) on
  every format.
- Fonts: today one font for the page (bold, italic and underline as soft
  styles). CSS font sizes and families want a font list in `struct WLView`
  (a later `wv_Size`).
