# OpenBrowser Lite 0.4.1

Date: 19 October 2026

0.4.1 brings the Lite 0.2 to 0.4 work (OpenLayout rendering, datatype
pictures, the CSS cascade; branch `openlayout-lite`, 7 to 8 October) onto
main, which had moved Lite 0.1 to OpenTLS (0.1.1, PR #19). Dale, 19 October:
Lite "should be rescued, modernised and delivered as part of OpenUp
standard", and "lite should almost create the bones of a weblayout.library".

## What changed since 0.4

- TLS is OpenTLS only, main's way (third_party/opentls, OpenMail 0.3.3's
  network layer). The branch's own AmiSSL-to-OpenTLS swap is not taken. The
  CA list is OpenTLS's, `S:OpenTLS/ca-bundle.pem`.
- The engine is weblayout, `src/weblayout/`, behind `weblayout.h`
  (`docs/WEBLAYOUT_LIBRARY.md`). `ob_main.c` calls nothing else of it: no
  OpenLayout types in the program any more.
- Built with OpenAmigaGCC 16.2.0-open3 and OpenNDK, for the 68040 with its
  FPU, `-Wall -Wextra -Werror` on everything compiled in (Lite, weblayout,
  OpenLayout, OpenMail's layer).
- One build script, `scripts/build-openbrowser.sh` (TOOLCHAIN_DIR or STOVE,
  NDK_INCLUDE, OPENLAYOUT_SRC, OPENLAYOUT=static|library, TESTS=1), in
  place of eight; host tests in `tests/run-host-tests.sh`; cloud CI in
  `.github/workflows/lite.yml`.
- The page is clipped to its area: text, pictures and boxes that cross its
  edges no longer draw over the address bar, the scroller or the status
  line.
- Resizing the window keeps the place on the page (it went back to the top).
- Addresses: `wl_url.c` (shared with the HTTP layer's redirects) keeps a
  link's #fragment, tells a scheme from a path that holds "://", and folds
  `.` and `..` at the end of a path; host-tested (`tests/test_wl_url.c`).
- The build no longer needs `/home/...` paths or an AmiSSL SDK.

## Checked

- The host tests (HTML Lite, CSS Lite, addresses) pass, also with
  AddressSanitizer and UBSan.
- OpenBrowser, obfetch and the four Amiga test programs build with no
  warnings, with OpenLayout built in and with the resident
  openlayout.library.

## Not yet checked

- On an AmigaOS 3.2.3 instance and on a real 68040: the window, scrolling,
  the clipping, a click-through, pictures and an https:// page through
  OpenTLS. 0.2 to 0.4 were qualified on AC090 with AmiSSL; 0.4.1 is not yet.
