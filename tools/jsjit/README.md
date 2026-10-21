# The 68k template JIT (J2): tools

JavaScriptCore on the 68k runs our own interpreter: offlineasm's M68K back end
(patches 0015, 0017, 0020) turns each bytecode handler into 68k code. Upstream
JavaScriptCore has no JIT for any CPU with 32-bit pointers, so J2 builds its
JIT from those same handlers: each one becomes a **template** that is copied
once per bytecode instruction into a code buffer, with the instruction's
operands written into the copy. The copies run one after another without the
interpreter's dispatch or operand decoding, on the interpreter's own frames
and registers, so compiled code and the interpreter can hand over at any
bytecode instruction.

| File | What |
| --- | --- |
| `m68k_templates.py` | Writes `LLIntTemplates.h` (templates and their hole tables) from the generated `LLIntAssembly.h` |
| `stitch.h` | The stitcher's core: copy a template, fill its holes |
| `check_templates.py` | Checks an assembled set: no PC-relative reference leaves a template, every hole is where its table says |
| `test/run_qemu_test.sh` | Stitches real `op_mov` templates and runs them under `qemu-m68k -cpu m68040` |

## How a template differs from its handler

- **Operand reads become holes.** A load from `(K,a3,a2.l*1)` (PB + PC + K)
  that is sign- or zero-extended becomes `move.l #imm,dN`; the stitcher reads
  the operand from the bytecode (size and sign from the hole's kind) and writes
  it into the immediate.
- **Dispatch becomes a check.** Fetching the next opcode and jumping through
  the opcode map becomes `cmpa.l #next,a2; jeq end; jmp redispatch`. The
  stitcher writes this instruction's PC plus its length as `next`: when PC
  moved by exactly one instruction the copy falls through into the next one,
  and otherwise (a jump, a slow path that changed PC, an exception) it leaves
  through `jsc_m68k_tjit_redispatch`.
- **Code outside the template is reached absolutely, labels inside it
  PC-relatively**, so a copy runs at any address.
- PB (a3) and PC (a2) stay valid, so anything a template still reads from
  the bytecode, and every slow path, works as in the interpreter.

All 585 JavaScript handlers (narrow, wide16, wide32) convert, with 2,745 holes.

## Running the checks (cloud container or any Linux)

```sh
apt-get install -y binutils-m68k-linux-gnu g++-m68k-linux-gnu qemu-user
# LLIntAssembly.h from a 68k build; it can also be generated on Linux, see
# the JS JIT handover notes
tools/jsjit/test/run_qemu_test.sh path/to/LLIntAssembly.h
```

The host test uses m68k Linux's calling convention; the templates themselves
only use the interpreter's registers, so they are the same on AmigaOS.
