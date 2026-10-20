#!/usr/bin/env python3
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 AmigaChrome Team. MIT licence, see LICENSE.
#
# The 68k template JIT's templates (J2, stage T1 prototype).
#
# Reads the 68k interpreter that offlineasm's M68K back end generates
# (DerivedSources/JavaScriptCore/LLIntAssembly.h) and writes each bytecode
# handler a second time as a template: the same code, made so it can be
# copied into a code buffer, one copy per bytecode instruction, and patched.
#
# - Operand reads, loads from (K,a3,a2.l*1) (PB + PC + K), become
#   "move.l #imm,dN" holes. The stitcher writes the operand's value, read
#   from the bytecode with the hole's size and sign, into the immediate.
# - Dispatch (fetch the next opcode, jump through the opcode map) becomes
#   "cmpa.l #next,a2; jeq end; jmp redispatch". The stitcher writes this
#   instruction's PC plus its length as next; when PC moved by exactly one
#   instruction the code falls through into the next copied template, and
#   otherwise (a jump, a slow path that changed PC, an exception) it leaves
#   for _jsc_m68k_tjit_redispatch, which carries on in the interpreter.
# - Branches and calls to code outside the template become absolute, and
#   references to labels inside it PC-relative, so a copy runs anywhere.
# - Local labels get a suffix so templates and interpreter share one file.
#
# Interpreter registers, frames and the register file are untouched, so a
# template and the interpreter can hand over at any bytecode boundary.
#
# usage: m68k_templates.py [--only op_a,op_b] LLIntAssembly.h LLIntTemplates.h [report.txt]

import re
import sys

SUFFIX = "__tpl"
PLACEHOLDER = "0x55555555"

# Hole kinds, as the stitcher reads them (llint/M68KTemplateJIT.h).
HOLE_U8, HOLE_S8, HOLE_U16, HOLE_S16, HOLE_32, HOLE_NEXT_PC = 1, 2, 3, 4, 5, 6

INSTR = re.compile(r'^(\s*)"(.*?)\s*\\n"(.*)$')
LOCAL_DEF = re.compile(r'^\s*OFFLINE_ASM_LOCAL_LABEL\((\w+)\)\s*$')
INLINE_DEF = re.compile(r'^(Lm68k\d+):$')
HANDLER = re.compile(r'^OFFLINE_ASM_(OPCODE_LABEL|GLUE_LABEL|GLOBAL_LABEL|UNALIGNED_GLOBAL_LABEL)\((\w+)\)')
OPERAND_LOAD = re.compile(r'^move\.([bwl]) \((-?\d+),a3,a2\.l\*1\),(d\d)$')
LOCAL_REF = re.compile(r'LOCAL_LABEL_STRING\((\w+)\)')
INLINE_REF = re.compile(r'\b(Lm68k\d+)\b')
GLOBAL_REF = re.compile(r'LABEL_REFERENCE\((\w+)\)')
DISPATCH_LEA = 'LABEL_REFERENCE(os_script_config_storage)'


class Unsupported(Exception):
    pass


def parse(path):
    """Splits LLIntAssembly.h into (name, kind, lines) blocks, one per
    global label, keeping only the 68k OFFLINE_ASM block."""
    blocks = []
    current = None
    for raw in open(path):
        line = raw.rstrip("\n")
        m = HANDLER.match(line)
        if m:
            current = (m.group(2), m.group(1), [])
            blocks.append(current)
            continue
        if line.startswith("OFFLINE_ASM_END"):
            current = None
            continue
        if current is not None:
            current[2].append(line)
    return blocks


def items(lines):
    """Turns a block's lines into ("label", name) and ("insn", text)."""
    out = []
    for line in lines:
        m = LOCAL_DEF.match(line)
        if m:
            out.append(("label", m.group(1)))
            continue
        m = INSTR.match(line)
        if not m:
            continue
        text = m.group(2).strip()
        if text.startswith(".size") or text.startswith(".type"):
            continue
        m2 = INLINE_DEF.match(text)
        if m2:
            out.append(("label", m2.group(1)))
            continue
        out.append(("insn", text))
    return out


def defined_labels(body):
    return {name for kind, name in body if kind == "label"}


def find_dispatch(body, i):
    """body[i] is the "lea os_script_config_storage,a0" of a dispatch.
    Returns (start, end): the opcode fetch before it up to the jmp (a0)."""
    start = i
    # Back over the opcode fetch: [moveq #0,d0] move.b (K,a3,a2.l*1),d0,
    # then t0's write (move.l d0,d4 / clr.l of the high word).
    j = i - 1
    while j >= 0 and body[j][0] == "insn" and not OPERAND_LOAD.match(body[j][1]):
        if i - j > 4:
            raise Unsupported("dispatch without opcode fetch")
        j -= 1
    if j < 0 or body[j][0] != "insn" or not body[j][1].startswith("move.b"):
        raise Unsupported("dispatch without opcode fetch")
    start = j
    if start > 0 and body[start - 1] == ("insn", "moveq #0,d0"):
        start -= 1
    k = i + 1
    while k < len(body) and body[k] != ("insn", "jmp (a0)"):
        if body[k][0] != "insn" or k - i > 6:
            raise Unsupported("dispatch without jmp (a0)")
        k += 1
    if k >= len(body):
        raise Unsupported("dispatch without jmp (a0)")
    if not any(re.match(r'move\.l \(\d+,a0,d\d\.l\*4\),a0', body[x][1]) for x in range(i + 1, k)):
        raise Unsupported("dispatch without opcode map load")
    return start, k


def rename(text, local):
    text = LOCAL_REF.sub(lambda m: 'LOCAL_LABEL_STRING(%s%s)' % (m.group(1), SUFFIX) if m.group(1) in local else m.group(0), text)
    text = INLINE_REF.sub(lambda m: m.group(1) + SUFFIX if m.group(1) in local else m.group(0), text)
    return text


def target_of(text):
    """The label a branch or jump names, and whether it is local to the
    interpreter's text (LOCAL_LABEL_STRING / Lm68k) or a global symbol."""
    m = LOCAL_REF.search(text)
    if m:
        return m.group(1), "local"
    m = INLINE_REF.search(text)
    if m:
        return m.group(1), "local"
    m = GLOBAL_REF.search(text)
    if m:
        return m.group(1), "global"
    return None, None


def ref(name, scope):
    return '" LOCAL_LABEL_STRING(%s) "' % name if scope == "local" else '" LABEL_REFERENCE(%s) "' % name


def make_template(name, body):
    local = defined_labels(body)
    out = []          # lines of the template, as asm text (C string bodies)
    holes = []        # (label, bytecode offset, kind)
    stubs = []        # far branch stubs
    counter = [0]

    def newlabel(prefix):
        counter[0] += 1
        return "Ltj_%s_%s_%d" % (prefix, name, counter[0])

    end = "Ltj_end_%s" % name
    windows = {}
    for x, (kind, text) in enumerate(body):
        if kind == "insn" and text.startswith("lea") and DISPATCH_LEA in text:
            start, stop = find_dispatch(body, x)
            windows[start] = stop
    i = 0
    n = len(body)
    while i < n:
        kind, text = body[i]
        if kind == "label":
            if text.startswith("Lm68k"):
                out.append('%s%s:' % (text, SUFFIX))
            else:
                out.append('" LOCAL_LABEL_STRING(%s%s) ":' % (text, SUFFIX))
            i += 1
            continue
        if i in windows:
            hl = newlabel("next")
            out.append('%s:' % hl)
            out.append('cmpa.l #%s,a2' % PLACEHOLDER)
            holes.append((hl, 0, HOLE_NEXT_PC, i))
            out.append('jeq %s' % end)
            out.append('jmp (" LABEL_REFERENCE(jsc_m68k_tjit_redispatch) ").l')
            i = windows[i] + 1
            continue
        m = OPERAND_LOAD.match(text)
        if m:
            size, offset, reg = m.group(1), int(m.group(2)), m.group(3)
            signed = None
            skip_next = False
            if size == "l":
                hkind = HOLE_32
            else:
                nxt = body[i + 1] if i + 1 < n else ("", "")
                ext = "extb.l %s" % reg if size == "b" else "ext.l %s" % reg
                if nxt == ("insn", ext):
                    signed = True
                    skip_next = True
                elif out and out[-1] == "moveq #0,%s" % reg:
                    signed = False
                    out.pop()
                else:
                    # A byte or word load that is neither sign- nor zero-extended
                    # (an opcode compare, for example): keep it as code. It
                    # still reads the bytecode through PB and PC, which stay
                    # valid in compiled code.
                    out.append(text)
                    i += 1
                    continue
                hkind = {("b", True): HOLE_S8, ("b", False): HOLE_U8,
                         ("w", True): HOLE_S16, ("w", False): HOLE_U16}[(size, signed)]
            hl = newlabel("op")
            out.append('%s:' % hl)
            out.append('move.l #%s,%s' % (PLACEHOLDER, reg))
            holes.append((hl, offset, hkind, i))
            i += 2 if skip_next else 1
            continue
        text = rename(text, local)
        op = text.split(" ", 1)[0]
        label, scope = target_of(body[i][1])
        if label is not None and label not in local:
            if op in ("jra", "jmp"):
                out.append('jmp (%s).l' % ref(label, scope))
            elif op == "jsr":
                out.append('jsr (%s).l' % ref(label, scope))
            elif re.match(r'j(eq|ne|cs|cc|hi|ls|lt|le|gt|ge|mi|pl|vs|vc)$', op):
                stub = newlabel("far")
                stubs.append('%s:' % stub)
                stubs.append('jmp (%s).l' % ref(label, scope))
                out.append('%s %s' % (op, stub))
            elif op == "lea" or (op == "move.l" and text.startswith("move.l #")):
                out.append(text)          # an absolute address: fine anywhere
            else:
                raise Unsupported("unknown use of outside label: %s" % text)
        elif label is not None and label in local and op in ("lea", "move.l", "pea"):
            # Absolute reference to a label in this template (a return
            # address): make it PC-relative.
            if op == "lea":
                m2 = re.match(r'lea (.*),(a\d)$', text)
                out.append('lea %s(pc),%s' % (m2.group(1), m2.group(2)))
            else:
                m2 = re.match(r'move\.l #(.*),(.*)$', text)
                if not m2:
                    raise Unsupported("local label as data: %s" % text)
                out.append('pea %s(pc)' % m2.group(1))
                out.append('move.l (sp)+,%s' % m2.group(2))
        else:
            if re.search(r'a3,a2', text):
                raise Unsupported("other bytecode access: %s" % text)
            out.append(text)
        i += 1
    if stubs:
        out.append('jra %s' % end)
        out.extend(stubs)
    return out, holes, end


def emit(templates, path):
    with open(path, "w") as f:
        f.write("// Generated by tools/jsjit/m68k_templates.py from LLIntAssembly.h. Do not edit.\n")
        f.write("// The AmigaChrome Team. Templates for the 68k template JIT (J2).\n\n")
        f.write('".text\\n"\n".balign 2\\n"\n')
        for name, out, holes, end in templates:
            f.write('"Ltj_start_%s:\\n"\n' % name)
            for line in out:
                f.write('    "%s \\n"\n' % line)
            f.write('"%s:\\n"\n' % end)
        f.write('".data\\n"\n".balign 4\\n"\n')
        for name, out, holes, end in templates:
            f.write('".globl " LABEL_REFERENCE(jsc_m68k_tpl_%s) "\\n"\n' % name)
            f.write('LABEL_REFERENCE(jsc_m68k_tpl_%s) ":\\n"\n' % name)
            f.write('    ".long Ltj_start_%s, %s - Ltj_start_%s, %d\\n"\n' % (name, end, name, len(holes)))
            for hl, offset, kind, _ in holes:
                imm = 2  # the immediate follows a one-word opcode in both forms
                f.write('    ".word %s + %d - Ltj_start_%s, %d, %d\\n"\n' % (hl, imm, name, offset, kind))
        f.write('".text\\n"\n')


def main(argv):
    only = None
    if len(argv) > 2 and argv[1] == "--only":
        only = set(argv[2].split(","))
        argv = argv[:1] + argv[3:]
    if len(argv) < 3:
        sys.stderr.write("usage: m68k_templates.py [--only op_a,op_b] LLIntAssembly.h LLIntTemplates.h [report.txt]\n")
        return 2
    blocks = parse(argv[1])
    templates = []
    report = []
    for name, kind, lines in blocks:
        if kind != "OPCODE_LABEL" or not name.startswith("op_"):
            continue
        if only is not None and name not in only:
            continue
        body = items(lines)
        try:
            out, holes, end = make_template(name, body)
        except Unsupported as e:
            report.append("SKIP %s: %s" % (name, e))
            continue
        templates.append((name, out, holes, end))
        report.append("OK   %s: %d lines, %d holes" % (name, len(out), len(holes)))
    emit(templates, argv[2])
    text = "\n".join(report) + "\n%d templates, %d skipped\n" % (len(templates), sum(1 for r in report if r.startswith("SKIP")))
    if len(argv) > 3:
        open(argv[3], "w").write(text)
    sys.stdout.write(text.splitlines()[-1] + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
