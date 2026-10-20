# 68k template JIT: lab results

The AmigaChrome Team. jsc (WebKit pin 54fe1539, patches 0001-0024) cross-built
for m68k Linux with `-m68040`, run under `qemu-m68k -cpu m68040`. The numbers
are 68k instructions executed (`count.sh`), with jsc start-up (6.3 M) taken
off. They are not cycles: a real 68040 also pays for memory and branches.
Every test gives the same result with the JIT on and off.

## 20 Oct 2026, stage J3 (32-bit integer fast paths, patch 0025)

Interpreter column as below; JIT column with `JSC_useM68KTemplateJIT=1`.

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
