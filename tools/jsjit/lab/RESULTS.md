# 68k template JIT: lab results

The AmigaChrome Team. jsc (WebKit pin 54fe1539, patches 0001-0024) cross-built
for m68k Linux with `-m68040`, run under `qemu-m68k -cpu m68040`. The numbers
are 68k instructions executed (`count.sh`), with jsc start-up (6.3 M) taken
off. They are not cycles: a real 68040 also pays for memory and branches.
Every test gives the same result with the JIT on and off.

## 21 Oct 2026, property shape tables, getters, strings, loop doubles (patch 0036)

get_by_id sites keep a table of the last four structures they saw, with
own values, getters (own, or on an unchanged prototype; called as op_call
calls) and missing properties; a garbage collection clears the entries
of structures that died. Strings get their own code for `s[i]`, `.length`
(flat strings and ropes) and `charCodeAt`. Loop register copies keep
doubles in fp2-fp6. `lab/correct9.js` (one site seeing six shapes, own
and prototype getters, prototypes changing and losing the property,
missing properties, throwing getters, native getters, proxies, shapes
dying across gc(), primitives, classes with super, dictionaries),
`lab/correct10.js` (characters and codes of 8- and 16-bit strings,
out-of-range indexes, ropes, `.length` of non-strings at the same site,
charCodeAt replaced, called on other receivers and through `.call`) and
`lab/correct11.js` (double loops with NaN, -0, Infinity, int32 overflow,
doubles that turn into ints and strings, read by closures, left by
exceptions, more doubles than FPU registers) print what node prints with
the JIT on and off, as do correct.js-correct8.js.

This lab build is a fresh one (GCC 14, jsc start-up 5.5 M, taken off
below), so its counts differ a little from the sections below.

| test | interpreter (M) | 0035 (M) | 0036 (M) | 0036 vs 0035 | vs interpreter |
|---|---:|---:|---:|---:|---:|
| page-lexer    |  79.6 |  27.5 |  18.6 | 1.47x | 4.3x |
| page-vdom     | 156.7 |  74.2 |  61.6 | 1.20x | 2.5x |
| page-events   | 263.9 |  95.7 |  94.4 | 1.01x | 2.8x |
| page-json     |  51.0 |  38.8 |  38.8 | 1.00x | 1.3x |
| page-template |  97.0 |  79.4 |  79.0 | 1.01x | 1.2x |
| double-loop   |  46.4 |   6.7 |   3.8 | 1.78x | 12.3x |
| closures      |  18.1 |   7.2 |   6.9 | 1.04x | 2.6x |
| array (sort)  | 102.2 |  64.8 |  63.5 | 1.02x | 1.6x |
| int-loop      |  91.2 |   6.0 |   6.1 | 1.00x | 15.1x |
| calls         |  96.1 |  12.0 |  12.0 | 1.00x | 8.0x |
| properties    |  94.2 |  12.5 |  12.5 | 1.00x | 7.5x |
| arrays        |  62.3 |   7.4 |   7.5 | 0.99x | 8.3x |
| strings       |   8.8 |   5.4 |   5.4 | 1.00x | 1.6x |

arrays is about 1% slower (the string path after the array path costs
a check). What the page tests still spend: for-in enumeration, array
builtins (push, `in`, put_by_val_direct), get_by_val with string keys
on objects, `arguments`, string `+=` (C++ ropes), and regular
expressions and JSON in C++.

## 21 Oct 2026, compares, new objects, polymorphic calls, scopes (patch 0035)

A real-page benchmark (`page/`: virtual DOM diff, HTML templating, JSON,
a tokenizer, an event store) showed where pages spend their time, by
opcode (a lab-only map from JIT code addresses to bytecodes): compares and
true/false tests on non-int32 values in the interpreter's copied code,
new properties on objects (cached transitions), calls whose site had gone
polymorphic or whose result is unused, and global and closure variables.
0035 gives all of those their own code. It also fixes a bug from 0031:
after a JIT call the interpreter's sp stayed at the callee's frame, so a
garbage collection cleared the caller's slots below it (`lab/correct8.js`
crashed). `lab/correct6.js` (every kind of value through ===, ==, <,
truthiness, == null), `lab/correct7.js` (polymorphic and closure calls,
.call, constructors, literals, setters on prototypes, late setters) and
`lab/correct8.js` (globals, let/const and TDZ, closures, eval injection,
with, read-only globals, gc()) print what node prints with the JIT on and
off; so do the page tests and correct3-5.

| test | interpreter (M) | 0034 (M) | 0035 (M) | 0035 vs 0034 | vs interpreter |
|---|---:|---:|---:|---:|---:|
| page-lexer    |  80.5 |  49.2 |  28.3 | 1.74x | 2.8x |
| page-events   | 264.4 | 134.6 |  95.6 | 1.41x | 2.8x |
| page-vdom     | 157.0 |  96.5 |  74.4 | 1.30x | 2.1x |
| page-json     |  48.8 |  40.0 |  36.5 | 1.09x | 1.3x |
| page-template |  97.3 |  83.0 |  79.7 | 1.04x | 1.2x |
| closures      |       |  10.2 |   7.2 | 1.42x | 2.6x |
| strings       |       |   5.8 |   5.4 | 1.08x |      |
| calls         |       |  11.7 |  11.9 | 0.98x |      |

calls pays two instructions per call for the sp fix. What the page tests
still spend: regular expressions (Yarr's interpreter) and JSON in C++,
array builtins written in JavaScript (`in` and put_by_val_direct),
for-in enumeration, get_by_id misses on objects of several shapes, and
the C++ slow paths' fixed cost.

## 21 Oct 2026, arrays and calls in loops (patch 0034)

get_by_val, put_by_val and get_length get their own code for int32 and
contiguous arrays; loop register copies also run array access and calls;
a nested entry from C++ saves only the sp and lr slots. `arrays` is a new
jsbench test (1000-element int array, read and written 50 times).
`lab/correct5.js` (holes, prototype holes, out-of-bounds, negative and
fractional indexes, copy-on-write literals, appends, doubles, objects,
mixed and changing shapes, sparse, frozen and typed arrays, array-likes,
arguments, lengths of strings and functions, 2^32-1 length) prints what
node prints with the JIT on and off, as do `lab/correct3.js` and
`lab/correct4.js`; `lab/correct.js` and `lab/correct2.js` are unchanged.

| jsbench test | 0032 (M) | 0034 (M) | 0034 vs 0032 | vs interpreter |
|---|---:|---:|---:|---:|
| arrays      |  25.8 |   7.3 | 3.53x |  8.5x |
| array (sort)|  76.8 |  71.3 | 1.08x |  2.1x |
| closures    |  10.9 |  10.1 | 1.07x |  1.9x |
| calls       |  12.0 |  11.6 | 1.03x |  8.8x |
| int-loop, double-loop, properties, strings, regexp, json | | | 1.00x | |

## 20 Oct 2026, stage J6 (loop variables in registers; patch 0032)

Integer loops get a second copy of their body with up to six variables in
d2-d7, and calls into JavaScript from C++ (the sort comparator here) no
longer copy the whole register file with memcpy twice per call: they keep
only the slots a C call must give back, with a run of move.l. Calls from
compiled code to a compiled function skip the interpreter's prologue (each
function's code starts with its own).
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
| properties  |  13.1 |  12.3 | 1.07x |  8.6x |
| double-loop |   6.5 |   6.5 | 1.00x |  7.9x |
| calls       |  16.5 |  12.0 | 1.37x |  8.5x |
| closures    |  10.8 |  10.8 | 1.00x |  1.8x |
| strings     |   5.7 |   5.7 | 1.00x |  1.6x |
| array (sort)| 107.9 |  76.8 | 1.41x |  1.9x |
| regexp      |   7.8 |   7.8 | 1.00x |  1.1x |
| json        |   3.7 |   3.7 | 1.00x |  1.0x |

The register copy covers int32 work and get_by_id/put_by_id (int-loop,
properties); loops that call or work on doubles are not covered yet.
`lab/correct3.js` also covers property loops (values turning into doubles
and strings, two objects at one site, getters, delete), checked against
node.

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
