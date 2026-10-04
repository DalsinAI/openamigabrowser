# OpenBrowser

A WebKit-based web browser for AmigaOS 3.2 (68020+ with FPU).

**Status, 4 October 2026:** its JavaScript engine, WebKit's JavaScriptCore,
runs on AmigaOS 3.2.3:

```
1> Stack 4194304
1> jsc -e "print(1+1)"
2
```

It runs functions, objects, arrays, strings, JSON, RegExp, Date, exceptions
and double-precision maths (`tests/js`). The browser itself comes next: a
GadTools window, images through datatypes, then pages drawn by WebKit. See
[docs/PHASE_PLAN.md](docs/PHASE_PLAN.md).

## What is here

| Folder | What |
| --- | --- |
| `webkit/` | The pinned upstream revision and the AmigaOS/m68k patch for WebKit (JSCOnly) |
| `toolchain/` | CMake toolchain file for the os32-gcc16 compiler |
| `scripts/` | `build-icu.sh` and `build-jsc.sh` |
| `icu/` | ICU 78.3 patch for AmigaOS 3.x |
| `stove/` | Notes and patches for the GCC 16.2 AmigaOS compiler, plus the compat library and headers it needs |
| `tools/` | Probe runner for an AmigaChrome test instance, and stack/map helpers |
| `tests/` | JavaScript and C test programs |
| `docs/` | Porting notes and the phase plan |

No binaries are kept here, and no WebKit, ICU or AmigaOS files.

## Building

1. Build the os32-gcc16 compiler (see `stove/STOVE-NOTES.txt` and
   `stove/gcc-patches/`), then build `stove/compat` into
   `libos32compat.a`. Copy `stove/include/*.h` into the compiler's
   `m68k-amigaos/sys-include`.
2. Build ICU 78.3 for AmigaOS with `scripts/build-icu.sh`.
3. Check out WebKit at the pinned revision and apply the patch (see
   `webkit/README.md`).
4. Run `WEBKIT_SOURCE=... scripts/build-jsc.sh`.

## Licence

OpenBrowser's own code and documents are MIT, Copyright (c) 2026 Dalsin
Limited (see [LICENSE](LICENSE)). WebKit/JavaScriptCore, ICU and GCC parts keep
their own licences (see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).

If you fork this or base work on it, please keep the credit and say what your
work is based on.
