#!/usr/bin/env python3
# Copyright (c) 2026 AmigaChrome Team. MIT licence (LICENSE).
# SPDX-License-Identifier: MIT
"""Writes the files a WebKit patch adds whole into a folder, for the host
tests of code that lives only in a patch (such as 0070's public suffix
lookup).

    tools/patch-files.py PATCH OUT_DIR NAME...

NAME is a file's name (not its path); only new files can be taken."""
import os
import re
import sys


def main():
    patch, out, names = sys.argv[1], sys.argv[2], set(sys.argv[3:])
    os.makedirs(out, exist_ok=True)
    found, name, lines, new = set(), None, [], False

    def flush():
        if name and new:
            with open(os.path.join(out, name), "w") as f:
                f.write("".join(lines))
            found.add(name)

    for line in open(patch):
        if line.startswith("diff --git "):
            flush()
            name, lines, new = None, [], False
        elif line.startswith("new file mode"):
            new = True
        elif (m := re.match(r"\+\+\+ b/(.*)", line)):
            base = os.path.basename(m.group(1).strip())
            name = base if base in names else None
        elif name and line.startswith("+"):
            lines.append(line[1:])
    flush()
    missing = names - found
    if missing:
        sys.exit("%s adds no new %s" % (patch, ", ".join(sorted(missing))))


if __name__ == "__main__":
    main()
