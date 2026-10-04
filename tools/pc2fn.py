#!/usr/bin/env python3
# OpenBrowser (DalsinAI/openamigabrowser). Copyright (c) 2026 Dalsin Limited. MIT licence, see LICENSE.
"""pc2fn.py NMFILE MAIN_RUNTIME_ADDR ADDR... : map runtime code addresses to
functions using `nm -n` of the hunk executable (text symbols only; mangled
names ending in 'E' are variables such as s_info and are skipped)."""
import subprocess, sys
nm, mainrt, addrs = sys.argv[1], int(sys.argv[2], 16), [int(x, 16) for x in sys.argv[3:]]
syms = []
for line in open(nm):
    t = line.split()
    if len(t) != 3 or t[1] not in "Tt":
        continue
    name = t[2]
    if name.startswith("__Z") and name.endswith("E"):
        continue
    syms.append((int(t[0], 16), name))
syms.sort()
main = next(a for a, n in syms if n == "_main")
base = mainrt - main
names = {}
def demangle(n):
    try:
        return subprocess.run(["c++filt", "-_"], input=n, capture_output=True, text=True).stdout.strip()
    except Exception:
        return n
import bisect
keys = [a for a, _ in syms]
for a in addrs:
    off = a - base
    i = bisect.bisect_right(keys, off) - 1
    if i < 0 or off > keys[-1] + 0x100000:
        print(f"{a:#010x}  (outside jsc text)")
        continue
    s, n = syms[i]
    print(f"{a:#010x}  {demangle(n)[:150]} +{off - s:#x}")
