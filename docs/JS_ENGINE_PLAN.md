# Faster JavaScript for OpenBrowser: plan

Status, 6 October 2026. JavaScriptCore runs on the 68k through its C
interpreter (`ENABLE_C_LOOP`). This is the plan for making it faster: a
68k interpreter first, then compiling as a service ("code in, runtime out").

## Where we start

Measured on the 68040 bench (AC090, Instance-24) with
`tests/jsbench.html` and the per-opcode page `tests/jsops.html`,
100,000 iterations each:

| Loop body | Time | Per opcode |
| --- | --- | --- |
| empty, `s = o`, `s = i - 3`, `o.a = i` | 20-40 ms | 0.2-0.4 µs |
| `s = o.a`, `s = a[0]` | 700-720 ms | ~7 µs |
| `s = i * 3` | 880 ms | ~8.8 µs |
| `s = f()` | 1,060 ms | ~10 µs |

The slow opcodes are ordinary 68k code (about 60 instructions each in
`JSC::LLInt::CLoop::execute`); the same kinds of instructions run at full
speed in C tests on the same bench. That points at the emulator, and a
reproducer has gone to AC090. On a real 68040 every instruction counts
instead, and there the C interpreter's code is the cost: GCC keeps the
interpreter's 64-bit virtual registers in a 12 KB stack frame, so most
instructions move values between memory and registers.

Two facts shape the work:

- **JavaScriptCore has only 64-bit values now.** Upstream removed the 32-bit
  interpreter (`LowLevelInterpreter32_64.asm`); every target uses
  `JSVALUE64`. A 68k back end has to turn each 64-bit operation into a pair
  of 32-bit ones. Pointers are still 32 bits (`CPU(ADDRESS32)`), so pointer
  operations stay single instructions.
- **The assembly interpreter switches on JavaScriptCore's assembler.**
  `PlatformEnable.h` turns `ENABLE_ASSEMBLER` on whenever the C interpreter
  is off, and the assembler has no 68k version. Either JavaScriptCore is
  taught to run the assembly interpreter without the assembler (its few
  generated stubs written in offlineasm instead), or a 68k `MacroAssembler`
  comes first. The first is smaller; the second is the foundation a 68k JIT
  needs anyway.

## Step 1: the 68k interpreter (done, 6 October 2026)

Built as WebKit patch 0015 (`webkit/patches/0015-jsc-m68k-interpreter.patch`),
on by default in `scripts/build-webcore.sh` (CMake `JSC_M68K_LLINT`). It took
a shorter route than the plan below: the C loop's plumbing stays (its own
JavaScript stack, slow paths as plain C calls, no assembler), and offlineasm's
new M68K back end (`offlineasm/m68k.rb`) emits the C loop's version of the
interpreter as 68k assembly instead of C. Interpreter registers are 64-bit
slots in a register file in memory, cfr is in a5.

Same answers as the C interpreter on a semantics page (integers, doubles,
NaN and -0, strings, objects, closures, exceptions, regular expressions),
and on the bench:

| Loop body (100,000 times) | C interpreter | 68k interpreter |
| --- | --- | --- |
| `s = o.a` | 700 ms | 60 ms |
| `s = a[0]` | 720 ms | 60 ms |
| `s = i * 3` | 880 ms | 40 ms |
| `s = f()` | 1,060 ms | 100 ms |
| `tests/jsbench.html` int loop (200,000) | 1,960 ms | 100 ms |
| `tests/jsbench.html` properties | 2,460 ms | 200 ms |
| `tests/jsbench.html` array sort | 1,420 ms | 600 ms |

Wikipedia's Amiga article loads completely in 40 s (97 s before), BBC and the
Microsoft sign-in page render as before. The slow opcodes were not the
emulator after all: they were GCC's code for the C loop, a 384 KB function.

Still to do: keep the hottest registers (PC, PB, t0-t3) in 68k registers
instead of the register file, and measure on a real 68040.

The original plan, for reference:

JavaScriptCore's interpreter is written once in offlineasm
(`llint/LowLevelInterpreter*.asm`) and turned into machine code by a back
end per CPU (`offlineasm/*.rb`). The work:

1. `offlineasm/m68k.rb`: the back end. Interpreter registers map to 68k
   registers where they fit (the call frame, the bytecode pointer and the
   metadata table in address registers; the hottest temporaries in data
   register pairs) and to a register block addressed from `a4` otherwise.
   64-bit operations become `add.l`/`addx.l`, `sub.l`/`subx.l` pairs;
   comparisons check the high word first; shifts use the 68020 bit-field and
   long-shift instructions.
2. `llint/LowLevelInterpreter.asm`: the 68k calling convention (AmigaOS
   GCC: arguments on the stack, results in `d0`/`d1`, `d2-d7`/`a2-a6`
   preserved), entry from C (`vmEntryToJavaScript`), and calls out to the
   slow paths.
3. JavaScriptCore without the assembler on M68K (`ENABLE_ASSEMBLER 0`
   with `ENABLE_C_LOOP 0`): the interpreter's native-call and
   entry stubs as offlineasm, and the places that assume the assembler
   whenever the C interpreter is off.
4. Checks: the existing JavaScript tests on the bench (`jsc-*` probes,
   `tests/jsbench.html`), every page in the validation set (Wikipedia, BBC,
   login.live.com, YouTube), and the timings above against the C
   interpreter. The C interpreter stays as a build option.

Expected gain: on a real 68040, about 2-3x on interpreter-bound code (fewer
instructions per opcode, no frame traffic). Under the emulator the gain only
shows fully once the slow opcodes above are fixed there.

## Step 2: compiling as a service ("code in, runtime out")

Like OpenMulticore's jobs: the engine sends a hot function's bytecode to a
`js.compile/1` service on `openservice.device` and gets back ready-to-run 68k
code.

- **Where it runs:** on a Cradle PC, on a card core, or on the Amiga itself
  when nothing else is there. Placement follows OpenMulticore's rules; the
  68k fallback is always present.
- **What goes in:** the function's bytecode, its constants, the value
  profiles the interpreter collected, and a table of the engine's runtime
  entry points (slow paths, the heap's allocation fast path), so the result
  can be linked without the compiler knowing the engine's memory layout.
- **What comes out:** relocatable 68k code plus its relocations and the
  exit points back into the interpreter. A small loader in the engine
  installs it.
- **The compiler:** JavaScriptCore's own baseline JIT with a 68k
  `MacroAssembler`, built for the host. On the host it can be as large as it
  likes.
- **Cache:** compiled functions are kept on disk by a hash of their bytecode
  and the engine's build, so revisited sites skip compiling.

Expected gain over the interpreter: typically 2-4x on hot code. The code
still runs on the 68k; the service removes the cost of compiling, not of
running.

## Step 3: offload the work itself (later)

With OpenMulticore cores that run native code, whole scripts (a worker, a
page's JavaScript) could run on the card with its own heap, the page's DOM
calls marshalled back. This needs WebCore's worker support on the Amiga and
is not started.
