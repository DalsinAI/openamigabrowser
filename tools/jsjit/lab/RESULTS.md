# 68k template JIT: lab results

The AmigaChrome Team. jsc (WebKit pin 54fe1539, patches 0001-0024) cross-built
for m68k Linux with `-m68040`, run under `qemu-m68k -cpu m68040`. The numbers
are 68k instructions executed (`count.sh`), with jsc start-up (6.3 M) taken
off. They are not cycles: a real 68040 also pays for memory and branches.
Every test gives the same result with the JIT on and off.

## 20 Oct 2026, stage J6 (loop variables in registers; patch 0032)

Integer loops get a second copy of their body with up to six variables in
d2-d7, and calls into JavaScript from C++ (the sort comparator here) no
longer copy the whole register file with memcpy twice per call: they keep
only the slots a C call must give back, with a run of move.l.
`lab/correct4.js` (sort, map, forEach, reduce, replace callbacks, getters,
JSON reviver, valueOf, toString, exceptions and deep recursion through
callbacks) prints what node prints, with the JIT on and off.
`lab/correct.js`, `lab/correct2.js` and `lab/correct3.js` (overflow
of add, sub, mul and inc/dec inside the copy, values turning into doubles
and back, strings, nested loops, early exits before a variable is written,
-0, valueOf) print the same with the JIT on and off. 0032 leaves the
interpreter's templates as they were.

| jsbench test | J4 (M) | J6 (M) | J6 vs J4 | J6 vs interpreter 0029 |
|---|---:|---:|---:|---:|
| int-loop    |   9.9 |   5.9 | 1.68x | 17.3x |
| properties  |  13.1 |  13.1 | 1.00x |  8.0x |
| double-loop |   6.5 |   6.5 | 1.00x |  7.9x |
| calls       |  16.5 |  16.5 | 1.00x |  6.2x |
| closures    |  10.8 |  10.8 | 1.00x |  1.8x |
| strings     |   5.7 |   5.7 | 1.00x |  1.6x |
| array (sort)| 107.9 |  76.8 | 1.41x |  1.9x |
| regexp      |   7.8 |   7.8 | 1.00x |  1.1x |
| json        |   3.7 |   3.7 | 1.00x |  1.0x |

Only int-loop's loop qualifies for the register copy: the others' loops
access properties, call or work on doubles, which the copy does not cover
yet.

## 20 Oct 2026, stage J4 (whole functions, calls, property access, doubles; patch 0031)

Interpreter columns: as of patch 0029, and with 0031 (it no longer fills
profiles, and its double arithmetic uses fdadd and friends). Speed-up is J4
against the interpreter as of 0029. `lab/correct.js` and `lab/correct2.js`
(doubles, NaN, -0, overflow, inline and out-of-line properties, polymorphic
sites, prototypes, getters, setters, delete, frozen objects, objects stored,
recursion) print the same with the JIT on and off; `check_templates.py`: 585
templates, 0 problems.

| jsbench test | interpreter 0029 (M) | interpreter 0031 (M) | J3 (M) | J4 (M) | speed-up | J4 vs J3 |
|---|---:|---:|---:|---:|---:|---:|
| int-loop    | 101.8 |  91.0 |   9.9 |   9.9 | 10.33x | 1.00x |
| properties  | 105.1 |  94.0 |  39.6 |  13.1 |  8.01x | 3.02x |
| double-loop |  51.7 |  46.3 |  24.7 |   6.5 |  7.90x | 3.77x |
| calls       | 101.9 |  95.9 |  54.4 |  16.5 |  6.19x | 3.30x |
| closures    |  19.0 |  18.1 |  13.8 |  10.8 |  1.75x | 1.28x |
| strings     |   9.2 |   8.8 |   6.6 |   5.7 |  1.61x | 1.16x |
| array (sort)| 146.6 | 143.4 | 135.7 | 107.9 |  1.36x | 1.26x |
| regexp      |   8.9 |   8.7 |   8.0 |   7.8 |  1.14x | 1.03x |
| json        |   3.6 |   3.6 |   3.7 |   3.7 |  0.98x | 1.00x |

`lab/correct2.js` also covers exceptions thrown through compiled calls,
stack overflow (RangeError), native callees, polymorphic call sites,
constructors, missing and extra arguments, and getters that call.

## 20 Oct 2026, stage J3 (32-bit integer fast paths, patch 0027)

Interpreter column as below; JIT column with the template JIT on (`JSC_useM68KTemplateJIT=1`, the default since patch 0029).

| jsbench test | interpreter (M) | J3 (M) | speed-up |
|---|---:|---:|---:|
| int-loop    | 101.8 |   9.9 | 10.30x |
| properties  | 105.1 |  39.6 |  2.65x |
| double-loop |  51.7 |  24.7 |  2.10x |
| calls       |  99.4 |  54.4 |  1.83x |
| strings     |   9.2 |   6.7 |  1.39x |
| closures    |  18.8 |  13.9 |  1.35x |
| regexp      |   8.9 |   8.0 |  1.10x |
| array (sort)| 143.9 | 135.7 |  1.06x |
| json        |   3.6 |   3.7 |  0.97x |

int-loop's loop body is now about 49 instructions per iteration (was ~460).
jsbench results and `lab/correct.js` are identical with the JIT on and off.

## 20 Oct 2026, stage T4 (peephole pass)

| jsbench test | JIT off (M) | JIT on (M) | change | speed-up |
|---|---:|---:|---:|---:|
| int-loop    | 101.8 |  69.7 | -31.5% | 1.46x |
| double-loop |  51.7 |  36.6 | -29.2% | 1.41x |
| calls       |  99.4 |  77.5 | -22.1% | 1.28x |
| properties  | 105.1 |  69.3 | -34.0% | 1.52x |
| closures    |  18.8 |  15.8 | -15.9% | 1.19x |
| strings     |   9.2 |   7.6 | -17.8% | 1.22x |
| regexp      |   8.9 |   8.4 |  -5.7% | 1.06x |
| array (sort)| 143.9 | 139.5 |  -3.1% | 1.03x |
| json        |   3.6 |   3.6 |  +1.0% | 0.99x |

`lab/correct.js` (15 loops: int overflow, doubles, exceptions, getters,
closures, for-in, switch) prints the same with the JIT on and off.

Wall time under qemu is not a measure: qemu retranslates code whose page is
written, so before JIT code had pages of its own (test hosts only) sort ran
25x slower in the emulator while executing fewer instructions.

## 20 Oct 2026, stage T1 (plain templates)

| jsbench test | JIT off (M) | JIT on (M) | change |
|---|---:|---:|---:|
| int-loop    | 101.8 | 92.7 | -9.0% |
| double-loop |  51.7 | 48.2 | -6.7% |
| calls       |  99.4 | 92.9 | -6.6% |
| properties  | 105.1 | 93.9 | -10.7% |
| closures    |  18.8 | 17.9 | -4.5% |
| array (sort)| 143.9 | 142.8 | -0.7% |
| strings, regexp, json | 9.2, 8.9, 3.6 | same | 0% |

- Only loops are compiled (entry is op_loop_hint); functions without a loop
  (call targets, sort comparators) still run in the interpreter.
- strings, regexp, json and sort spend their time in C++ (string builder,
  Yarr, JSON, the sort), which no JS JIT changes.
- What the templates remove is dispatch and operand decoding. What remains
  is the interpreter's own code: int-loop costs about 460 instructions per
  iteration (8 bytecodes), mostly 64-bit value moves through the register
  file in memory, high-word clears and value profiling. That is the next
  target (DESIGN.md T4, then type-specialised int32 code).
