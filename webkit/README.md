# WebKit for OpenBrowser

WebKit is not copied into this repository. Use the pinned upstream revision
in `UPSTREAM` and apply the patches in `patches/` in order.

```sh
git clone --filter=blob:none --no-checkout https://github.com/WebKit/WebKit.git
cd WebKit
git sparse-checkout set --cone CMakeLists.txt Source/cmake Source/WTF Source/bmalloc Source/JavaScriptCore Source/WebCore Source/ThirdParty Tools
git checkout 54fe1539718eef2f24d813019949ca8cd4b9e89e
git apply /path/to/openamigabrowser/webkit/patches/0001-amiga-m68k-jsconly.patch
git apply /path/to/openamigabrowser/webkit/patches/0002-amiga-webcore.patch
git apply /path/to/openamigabrowser/webkit/patches/0003-jsc-llint-prototype-cache-big-endian.patch
git apply /path/to/openamigabrowser/webkit/patches/0004-amiga-webcore-network.patch
git apply /path/to/openamigabrowser/webkit/patches/0005-utf16-big-endian.patch
git apply /path/to/openamigabrowser/webkit/patches/0006-amiga-disk-webp-quiet.patch
git apply /path/to/openamigabrowser/webkit/patches/0007-amiga-host-connections-llint-o2.patch
git apply /path/to/openamigabrowser/webkit/patches/0008-jsc-for-in-big-endian.patch
git apply /path/to/openamigabrowser/webkit/patches/0009-amiga-stop-run-loops-at-exit.patch
git apply /path/to/openamigabrowser/webkit/patches/0010-amiga-fetch-proxy-experiment.patch
```

The patches, in order:

- `0002-amiga-webcore.patch`: WebCore's Amiga port (`PORT=Amiga`): build
  options, the platform files (screen, keyboard, pasteboard, MIME types,
  user agent, fonts), pictures through the system's datatypes, cairo
  drawing without accelerated compositing, and the network layer over
  WebCore's curl backend. Notes are in `../docs/WEBCORE_PORT.md`.
- `0003-jsc-llint-prototype-cache-big-endian.patch`: a JavaScriptCore fix
  for big-endian 32-bit CPUs; without it, methods found on a prototype
  (`a.push`, `s.concat`) read back as `undefined` after their first cached
  call.
- `0004-amiga-webcore-network.patch`: the network layer running on AmigaOS:
  the curl delegate's type, no large-file requirement (libnix's `off_t` is
  32-bit), stopping curl's thread before the program exits, and settings
  for slow TLS handshakes on a 68k (X25519 and P-256 key exchange, one
  connection per host and four in all, a 120 s connect timeout).
- `0005-utf16-big-endian.patch`: fixes for big-endian CPUs, where 16-bit
  strings were converted to UTF-8 as if little-endian (garbling any text
  beyond Latin-1) and the HTML fast-path parser could not find tags in
  16-bit text.
- `0006-amiga-disk-webp-quiet.patch`: the cookie database keeps its journal
  in memory instead of on the disk, WebP sizes come from the file's header
  so a picture is decoded only when drawn, and a missing picture no longer
  writes a warning to stderr.
- `0007-amiga-host-connections-llint-o2.patch`: two connections per host
  instead of one (one left pictures queued long enough to time out), and
  the C-loop interpreter, which runs every line of JavaScript, built with
  `-O2`.
- `0008-jsc-for-in-big-endian.patch`: `for (k in o)` loops end. The
  interpreter's pointer comparisons (`op_jeq_ptr`, `op_jneq_ptr`) read
  half of an 8-byte value on big-endian CPUs with 32-bit pointers, so a
  for-in loop never saw the value that ends it: it ran forever, and reading
  `o[k]` inside it crashed the program.
- `0009-amiga-stop-run-loops-at-exit.patch`: libpthread waits for every
  thread when a program exits, and a work queue's thread waits for work for
  ever, so a program that had made one never exited. The generic run loop
  keeps a list on AmigaOS, and `amigaStopAllRunLoops()` stops them all.
- `0010-amiga-fetch-proxy-experiment.patch`: the host-fetch experiment.
  With the Shell variable `OB_FETCH_PROXY` set (see
  `scripts/ob-fetch-proxy.py`), requests go to a proxy on another PC, which
  makes the TLS connections. Off unless the variable is set.

- `0019-jsc-generators-in-python.patch`: JavaScriptCore builds without
  Ruby. offlineasm (front end and the M68K back end), the bytecode generator
  and GeneratePreferences run as Python 3 ports (standard library only), in
  `Source/JavaScriptCore/offlineasm-py`, `Source/JavaScriptCore/generator-py`
  and `Source/WTF/Scripts/GeneratePreferences.py`. Their output matches the
  Ruby scripts' byte for byte. The 68k build always uses the M68K back end
  (the `JSC_M68K_LLINT` option is gone). A step that still asks for Ruby
  runs `no-ruby.py`, which names the script and fails. (0018 is the visit
  cache, still a draft.)

- `0020-jsc-m68k-interpreter-data-registers.patch`: J1. The 68k
  interpreter keeps t0-t3's low words in d4-d7 instead of its register file
  in memory (58% fewer register-file accesses). jsbench on the AC090:
  int-loop and properties 1.4 times faster, array sort 1.1 times.

- `0021-webcore-generators-in-python.patch`: WebCore builds without Ruby.
  `generate-abstract-heap.rb`, `GenerateSettings.rb` and
  `GenerateQuirkBehaviors.rb` run as Python ports beside them, their nine
  templates as Python forms (`.pyerb`); the outputs match the Ruby scripts'
  byte for byte. No build step of the Amiga port runs Ruby now.

- `0022-amiga-webcore-curl-opentls.patch`: the engine without OpenSSL. curl
  does TLS through OpenTLS (openamigacurl's main branch), so `USE(OPENSSL)`
  is off and WebCore links no AmiSSL: curl's OpenSSL-only files are left
  out, and no cipher list, curve list or CA file is set (OpenTLS picks its
  own suites and reads `S:OpenTLS/ca-bundle.pem`). Responses carry no
  certificate details, and WebCrypto (`crypto/amiga/CryptoAmiga.cpp`)
  offers the SHA-1 and SHA-2 digests only.

- `0023-amiga-webcore-http2.patch`: HTTP/2. curl (openamigacurl, built with
  nghttp2) is asked for `CURL_HTTP_VERSION_2TLS` on https://, HTTP/1.1
  when the server does not offer h2, and the multi handle multiplexes
  (`CURLPIPE_MULTIPLEX`): requests to one server are streams on one
  connection, one TLS handshake per server. The programs link
  `libnghttp2.a` after curl.

- `0024-jsc-m68k-template-jit.patch`: J2's first stage, the 68k template
  JIT. A hot loop's CodeBlock is compiled by copying, for each bytecode
  instruction, the 68k interpreter's handler for it as a template
  (`offlineasm-py/m68k_templates.py`) with the instruction's operands written
  in (`llint/M68KTemplateJIT.cpp`). The copies run on the interpreter's own
  frames and registers and fall through into each other; op_loop_hint enters
  the code and `jsc_m68k_tjit_redispatch` leaves it. On by default since
  0029; `JSC_useM68KTemplateJIT=0` turns it off. Tools and the qemu test are in
  `../tools/jsjit/`.

- `0027-jsc-m68k-jit-int32-fast-paths.patch`: J3, 32-bit integer fast
  paths in the template JIT. For mov, add, sub, mul, inc, dec, the bit ops,
  jmp, loop_hint and the compare-and-jump ops the JIT writes its own 68k
  code: it checks the operands are int32s, works in 32-bit registers and
  stores the result; anything else runs the instruction's template, kept
  out of line. Branches between instructions are direct. JSValues stay
  64-bit in memory. `JSC_useM68KFastPaths=0` turns it off for comparison.

- `0029-jsc-m68k-template-jit-on-by-default.patch`: `useM68KTemplateJIT`
  defaults to true, so engine builds run the template JIT and its fast
  paths everywhere. `JSC_useM68KTemplateJIT=0` runs the interpreter alone.

- `0030-jsc-m68k-jit-functions-properties-doubles.patch`: J4. Whole
  functions: op_enter runs a function's code once it has been called often
  enough, not only its loops. Property access: get_by_id and put_by_id get
  their own code for sites whose interpreter cache holds an own property
  (`JSC_useM68KPropertyFastPaths=0` turns it off). Doubles: add, sub, mul
  and the compare-and-jumps run on the FPU when an operand is a double or an
  int32 result overflows. Double arithmetic, in the JIT and the interpreter,
  uses the 68040's fdadd/fdsub/fdmul/fddiv/fdsqrt, which round once to
  double as JavaScript asks.

`0001-amiga-m68k-jsconly.patch` is one combined patch. It holds:

- the m68k CPU and AROS platform layer from the AmigaChrome AROS m68k port;
- the AmigaOS 3.x platform layer (`OS(AMIGAOS3)`, and `OS(AMIGA)` for code
  shared with AROS);
- the big-endian and m68k-alignment fixes (see `../docs/PORT_NOTES.md`).

Each patched file keeps its WebKit licence header (LGPL-2 or BSD).
