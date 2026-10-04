# WebKit for OpenBrowser

WebKit is not copied into this repository. Use the pinned upstream revision
in `UPSTREAM` and apply the patches in `patches/` in order.

```sh
git clone --filter=blob:none --no-checkout https://github.com/WebKit/WebKit.git
cd WebKit
git sparse-checkout set --cone CMakeLists.txt Source/cmake Source/WTF Source/bmalloc Source/JavaScriptCore Source/ThirdParty Tools
git checkout 54fe1539718eef2f24d813019949ca8cd4b9e89e
git apply /path/to/openamigabrowser/webkit/patches/0001-amiga-m68k-jsconly.patch
```

`0001-amiga-m68k-jsconly.patch` is one combined patch. It holds:

- the m68k CPU and AROS platform layer from the AmigaChrome AROS m68k port;
- the AmigaOS 3.x platform layer (`OS(AMIGAOS3)`, and `OS(AMIGA)` for code
  shared with AROS);
- the big-endian and m68k-alignment fixes (see `../docs/PORT_NOTES.md`).

Each patched file keeps its WebKit licence header (LGPL-2 or BSD).
