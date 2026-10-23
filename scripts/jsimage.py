#!/usr/bin/env python3
# OpenBrowser (DalsinAI/openamigabrowser)
# Copyright (c) 2026 AmigaChrome Team. MIT licence, see LICENSE.
"""jsimage: link a program against JavaScriptEngine's code instead of its
own copy of JavaScriptCore, WTF and bmalloc.

JavaScriptEngine (src/jslib, built with the browser) is one pc-relative
hunk holding JavaScriptCore, WTF, bmalloc, ICU and the C and C++ run-time
they use. A client (the browser's engine) is linked with each name it
takes from the image set to __jsimage_base, the end of its own hunk, plus
the name's offset in the image. Its first code (src/jslib/
jsimage_client.c) loads the image, adds I - B - __jsimage_base to every
such reference (I the image's address, B the client's), and runs the
image, whose main runs the client's own start-up in the same process.
scripts/build-webcore.sh's "jsimage" command runs these steps.

  needs OUT PROGRAM MAP...   OUT: an EXTERN line for every name members of
                             libJavaScriptCore.a, libWTF.a and libbmalloc.a
                             define in the clients (unstripped program and
                             -Map pairs), linked into the image so that it
                             keeps all of it
  exports IMAGE MAP OUT      the names a client may take from the image:
                             every global defined by an archive member,
                             except the linker's own names, the C library's
                             start-up and exit members and library bases
                             (each program runs its own), and jsimage_id and
                             jsimage_client. One line each: offset name
                             member
  imports EXPORTS PROGRAM MAP OUT
                             the exports a client takes: those it linked
                             from the three archives, and those from an
                             archive member it linked as well (the C and
                             C++ run-time, ICU: their state is then the
                             image's alone)
  stamp IMAGE EXPORTS        write the image id (a CRC of the exports) into
                             the image's jsimage_id
  script IMPORTS SHIFT OUT   jsimage-imports.ld (toolchain/
                             amigaos3-onehunk-jsimage.ld INCLUDEs it): each
                             name as __jsimage_base + offset + SHIFT
  fixups LINK1 LINK2 EXPORTS OUT
                             LINK1 and LINK2 are the client linked with
                             SHIFT 0 and SHIFT 0x10000000; every long that
                             differs between them by that much refers to
                             the image. OUT is LINK1 with a second hunk,
                             the fixup table (src/jslib/jsimage.h)
  id EXPORTS                 the image id
"""
import bisect
import re
import struct
import subprocess
import sys
import zlib

D = 0x10000000

HUNK_HEADER, HUNK_CODE, HUNK_DATA, HUNK_BSS = 0x3F3, 0x3E9, 0x3EA, 0x3EB
HUNK_RELOC32, HUNK_SYMBOL, HUNK_DEBUG, HUNK_END = 0x3EC, 0x3F0, 0x3F1, 0x3F2
HUNK_RELOC32SHORT, HUNK_RELRELOC32, HUNK_DREL32 = 0x3FC, 0x3FD, 0x3F7

# names each program keeps for itself: the linker's own and its lists
LINKER_NAME = re.compile(r'^(___?(INIT|EXIT|CTOR|DTOR|LIB|EH_FRAME)\w*LIST__|___?(EH_FRAME)\w*'
                         r'|__s?text|__etext|__sdata|__edata|__bss_start|__end|___text_size|___data_size'
                         r'|___bss_size|___datadata_relocs|___machtype|_main|___main|_\w+Base)$')
# C library members that belong to each program's start-up
OWN_MEMBER = re.compile(r'commandline|swapstack|__stack|stdiowin|__initlibraries|__initcpp|__main'
                        r'|detach|__exit|atexit|ncrt0|crt0|__fpucheck')


def u32(b, o):
    return struct.unpack_from('>I', b, o)[0]


def read_hunks(path):
    """The executable's hunks: [(type, memflags, data bytes, blocks after)]"""
    b = open(path, 'rb').read()
    if u32(b, 0) != HUNK_HEADER:
        sys.exit(f'jsimage: {path} is not an Amiga program')
    o = 4
    while u32(b, o):            # resident library names
        o += 4 + 4 * u32(b, o)
    o += 4
    count, first, last = u32(b, o), u32(b, o + 4), u32(b, o + 8)
    o += 12
    sizes = [u32(b, o + 4 * i) for i in range(last - first + 1)]
    o += 4 * len(sizes)
    hunks = []
    cur = None
    while o < len(b):
        t = u32(b, o) & 0x3FFFFFFF
        if t in (HUNK_CODE, HUNK_DATA):
            n = u32(b, o + 4)
            cur = [t, u32(b, o) & 0xC0000000, bytearray(b[o + 8:o + 8 + 4 * n]), bytearray()]
            hunks.append(cur)
            o += 8 + 4 * n
        elif t == HUNK_BSS:
            cur = [t, u32(b, o) & 0xC0000000, u32(b, o + 4), bytearray()]
            hunks.append(cur)
            o += 8
        elif t == HUNK_END:
            cur[3] += b[o:o + 4]
            o += 4
        else:                   # relocations, symbols, debug: kept as they are
            s = o
            if t in (HUNK_RELOC32, HUNK_RELRELOC32):
                o += 4
                while u32(b, o):
                    o += 8 + 4 * u32(b, o)
                o += 4
            elif t in (HUNK_RELOC32SHORT, HUNK_DREL32):
                o += 4
                while True:
                    n = struct.unpack_from('>H', b, o)[0]
                    o += 2
                    if not n:
                        break
                    o += 2 + 2 * n
                if o % 4:
                    o += 2
            elif t == HUNK_SYMBOL:
                o += 4
                while u32(b, o):
                    o += 4 + 4 * (u32(b, o) & 0xFFFFFF) + 4
                o += 4
            elif t == HUNK_DEBUG:
                o += 8 + 4 * u32(b, o + 4)
            else:
                sys.exit(f'jsimage: {path}: hunk block {t:#x} at {o:#x} not known')
            cur[3] += b[s:o]
    return sizes, hunks


def write_hunks(path, sizes, hunks):
    out = bytearray(struct.pack('>IIIII', HUNK_HEADER, 0, len(hunks), 0, len(hunks) - 1))
    for s in sizes:
        out += struct.pack('>I', s)
    for t, flags, data, rest in hunks:
        if t == HUNK_BSS:
            out += struct.pack('>II', t | flags, data)
        else:
            out += struct.pack('>II', t | flags, len(data) // 4) + data
        out += rest
    open(path, 'wb').write(out)


def nm(path):
    r = subprocess.run([NM, path], capture_output=True, text=True, check=True).stdout
    for line in r.splitlines():
        p = line.split()
        if len(p) == 3:
            yield int(p[0], 16), p[1], p[2]


def map_sections(path):
    """The input sections of a one-hunk link's -Map, sorted:
    [(start, end, 'archive(member)' or file)]. A symbol's owner is the
    section it lies in (the map's own symbol lines are demangled)."""
    out = []
    sect = re.compile(r'^ (\S+)?\s+0x([0-9a-f]+)\s+0x([0-9a-f]+) (\S.*)$')
    pending = None
    for line in open(path, errors='replace'):
        if pending is not None and line.startswith('  '):
            line = pending + line
        pending = None
        m = sect.match(line)
        if m and m.group(1) and not m.group(1).startswith('*'):
            start, size = int(m.group(2), 16), int(m.group(3), 16)
            if size and not m.group(1).startswith('.debug'):
                out.append((start, start + size, m.group(4).strip()))
        elif re.match(r'^ \S+$', line.rstrip('\n')):
            pending = line.rstrip('\n')     # a long section name, its address on the next line
    out.sort()
    return out


def owner(sections, starts, off):
    i = bisect.bisect_right(starts, off) - 1
    if i >= 0 and sections[i][0] <= off < sections[i][1]:
        return sections[i][2]
    return ''


def cmd_exports(image, mapfile, out):
    sections = map_sections(mapfile)
    starts = [a for a, _, _ in sections]
    kept = skipped = 0
    with open(out, 'w') as f:
        for off, typ, name in sorted(nm(image)):
            member = owner(sections, starts, off)
            archive = re.match(r'.*/(lib[^/]*\.a)\((.*)\)$', member)
            ours = name.startswith('_jsimage_')
            if not ours and (typ.islower() or LINKER_NAME.match(name) or not archive
                             or OWN_MEMBER.search(archive.group(2))):
                skipped += 1
                continue
            f.write(f'{off:08x} {name} {archive.group(1) + "(" + archive.group(2) + ")" if archive else member}\n')
            kept += 1
    print(f'jsimage exports: {kept} names, {skipped} kept by each program')


MOVED = re.compile(r'(^|/)lib(JavaScriptCore|WTF|bmalloc)\.a\(')


def defined_by(program, mapfile):
    """(offset, name, owner) of each global the program defines"""
    sections = map_sections(mapfile)
    starts = [a for a, _, _ in sections]
    for off, typ, name in nm(program):
        if not typ.islower():
            yield off, name, owner(sections, starts, off)


def cmd_needs(out, pairs):
    """EXTERN list for the image: every name that members of
    libJavaScriptCore.a, libWTF.a and libbmalloc.a define in the clients
    (each a PROGRAM MAP pair), so the image keeps all of it"""
    names = set()
    for program, mapfile in zip(pairs[0::2], pairs[1::2]):
        for _, name, who in defined_by(program, mapfile):
            if MOVED.search(who):
                names.add(name)
    with open(out, 'w') as f:
        f.write(f'/* jsimage: what the clients take from JavaScriptCore, WTF and bmalloc */\n')
        for name in sorted(names):
            f.write(f'EXTERN({name})\n')
    print(f'jsimage needs: {len(names)} names')


def cmd_imports(exports, program, mapfile, out):
    """The names a client takes from the image: the image's exports that
    the client linked from libJavaScriptCore.a, libWTF.a or libbmalloc.a,
    or from an archive member the image has too (the C and C++ run-time,
    ICU: one copy, the image's, so that their state is shared)"""
    image_members = set(m for _, _, m in (l.rstrip('\n').split(' ', 2) for l in open(exports)))
    sections = map_sections(mapfile)
    client_members = set(w for _, _, w in sections)
    def archive_member(w):
        m = re.match(r'.*/(lib[^/]*\.a)\((.*)\)$', w)
        return f'{m.group(1)}({m.group(2)})' if m else w
    client_members = set(archive_member(w) for w in client_members)
    moved = shared = 0
    with open(out, 'w') as f:
        for line in open(exports):
            off, name, member = line.rstrip('\n').split(' ', 2)
            if name.startswith('_jsimage_'):
                continue
            if MOVED.search(member):
                moved += 1
            elif member in client_members:
                shared += 1
            else:
                continue
            f.write(line)
    print(f'jsimage imports: {moved} from JavaScriptCore, WTF and bmalloc, {shared} shared run-time names')


def read_exports(path):
    for line in open(path):
        off, name, _ = line.split(' ', 2)
        yield int(off, 16), name


def image_id(path):
    return zlib.crc32(b''.join(f'{o:x} {n}\n'.encode() for o, n in read_exports(path))) & 0xFFFFFFFF


def cmd_script(exports, shift, out):
    """INCLUDEd at the end of the client's .text (toolchain/
    amigaos3-onehunk-jsimage.ld): each name is __jsimage_base plus its
    offset in the image, plus SHIFT. Inside the output section, so the
    names are relative to the hunk and ld resolves pc-relative references
    to them as it does any other in the hunk."""
    shift = int(shift, 0)
    with open(out, 'w') as f:
        f.write(f'/* jsimage: JavaScriptEngine, image id {image_id(exports):08x} */\n')
        for off, name in read_exports(exports):
            f.write(f'    {name} = __jsimage_base + {off + shift:#x};\n')


def symbol(path, want):
    for off, _, name in nm(path):
        if name == want:
            return off
    sys.exit(f'jsimage: no {want} in {path}')


def cmd_stamp(image, exports):
    """Write the image id into the image's jsimage_id, where the client's
    jsimage_entry checks it"""
    sizes, hunks = read_hunks(image)
    off = dict((n, o) for o, n in read_exports(exports)).get('_jsimage_id')
    if off is None or len(hunks) != 1:
        sys.exit(f'jsimage stamp: {image} is not one hunk with a jsimage_id')
    struct.pack_into('>I', hunks[0][2], off, image_id(exports))
    write_hunks(image, sizes, hunks)
    print(f'jsimage stamp: {image} is image {image_id(exports):08x}')


def cmd_fixups(link1, link2, exports, out):
    """Every long that differs between the two links by D refers to the
    image: an absolute one (with a HUNK_RELOC32, so LoadSeg adds the
    client's address B) or a pc-relative one. Both are made right by adding
    I - B - E, where I is the image's address and E the client's
    __jsimage_base."""
    s1, h1 = read_hunks(link1)
    s2, h2 = read_hunks(link2)
    if len(h1) != 1 or len(h2) != 1:
        sys.exit('jsimage fixups: the client must be one hunk (OB_PCREL)')
    c1, c2 = h1[0][2], h2[0][2]
    if len(c1) != len(c2):
        sys.exit(f'jsimage fixups: {link1} and {link2} differ in size')
    fields = []
    i, n = 0, len(c1) - 3
    while i < n:
        d = (u32(c2, i) - u32(c1, i)) & 0xFFFFFFFF
        if d == D:
            fields.append(i)
            i += 4
        else:
            i += 2
    base = symbol(link1, '__jsimage_base')
    names = dict((name, off) for off, name in read_exports(exports))
    for need in ('_jsimage_id', '_jsimage_client'):
        if need not in names:
            sys.exit(f'jsimage fixups: the image exports no {need}')
    table = struct.pack('>4sIIIiiII', b'JSIM', 1, image_id(exports), base,
                        names['_jsimage_id'], names['_jsimage_client'], 0, len(fields))
    table += b''.join(struct.pack('>I', o) for o in fields)
    s1 = s1 + [len(table) // 4]
    h1.append([HUNK_DATA, 0, bytearray(table), bytearray(struct.pack('>I', HUNK_END))])
    write_hunks(out, s1, h1)
    print(f'jsimage fixups: {len(fields)} references to the image, table {len(table)} bytes')


NM = 'm68k-amigaos-nm'
if __name__ == '__main__':
    args = sys.argv[1:]
    if args and args[0].startswith('--nm='):
        NM = args.pop(0)[5:]
    if len(args) == 4 and args[0] == 'exports':
        cmd_exports(*args[1:])
    elif len(args) == 4 and args[0] == 'script':
        cmd_script(*args[1:])
    elif len(args) == 5 and args[0] == 'fixups':
        cmd_fixups(*args[1:])
    elif len(args) >= 4 and args[0] == 'needs' and len(args) % 2 == 0:
        cmd_needs(args[1], args[2:])
    elif len(args) == 5 and args[0] == 'imports':
        cmd_imports(*args[1:])
    elif len(args) == 3 and args[0] == 'stamp':
        cmd_stamp(*args[1:])
    elif len(args) == 2 and args[0] == 'id':
        print(f'{image_id(args[1]):08x}')
    else:
        sys.exit(__doc__)
