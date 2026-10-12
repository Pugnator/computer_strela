#!/usr/bin/env python3
"""Inject single-bit faults into the smoke test and count how many it catches.

Each run flips one bit of one word of smoke-test.strela (the low digit of the
third address, bit 7, or of the first address, bit 31) and runs the program.
A fault counts as detected when the program does not end exactly as the
fault-free run does: same normal stop, panel words, command count and
simulated time. Some flips cannot change the outcome: the unused third address of a
check (26) or group prefix (30-37) only writes to cell 0001, and result and
scratch cells are overwritten before they are read. Those are listed apart.

Usage: python examples/benchmark/fault-sweep.py [path/to/strela_run]
"""
import pathlib
import re
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
runner = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build" / ("strela_run.exe" if sys.platform == "win32" else "strela_run"))
source = (HERE / "smoke-test.strela").read_text(encoding="utf-8").splitlines()
SCRATCH = ("result cell", "difference", "sum", "function value", "results to punch",
           "products", "copy of V1", "reciprocals", "power of two", "subroutine call counter")


def word(line):
    a, x, y, z, c, o = line.split()
    return int(a, 8), (int(x, 8) << 31) | (int(y, 8) << 19) | (int(z, 8) << 7) | (int(c) << 6) | int(o, 8)


def text(addr, w):
    return f"{addr:04o} {(w >> 31) & 0o7777:04o} {(w >> 19) & 0o7777:04o} {(w >> 7) & 0o7777:04o} {(w >> 6) & 1} {w & 0o77:02o}"


comments, rows = {}, []
for i, line in enumerate(source):
    m = re.match(r"^# ([0-7]{4}): (.*)$", line)
    if m:
        comments[int(m.group(1), 8)] = m.group(2)
    elif re.match(r"^[0-7]{4} [0-7]{4} ", line):
        rows.append(i)


def status(path):
    """Stop line and panel line: stop address, counts, simulated time, display words."""
    out = subprocess.run([runner, "run", str(path), "--max-us", "100000000"], capture_output=True, text=True)
    return out.returncode, "\n".join(out.stdout.splitlines()[:2])


baseline = status(HERE / "smoke-test.strela")
assert baseline[0] == 0 and baseline[1].startswith("normal stop"), baseline
data_start = min(a for a, c in comments.items() if c.startswith("tests passed (binary)"))


def benign(addr, w, bit):
    if addr >= data_start:
        note = comments.get(addr, "")
        if any(note.startswith(s) for s in SCRATCH) or note.startswith("book p. 137 example 5"):
            return True
        # Only the order digits of a shift count are used.
        return note.startswith("shift count") and bit in (7, 31)
    op = w & 0o77
    if bit == 7 and (op == 0o26 or 0o30 <= op <= 0o37 or op == 0o20):
        return True                       # third address unused: only cell 0001 is touched
    field = (w >> (7 if bit == 7 else 31)) & 0o7777
    return field == 0                     # 0000 becomes 0001, another zero cell


tmp = pathlib.Path(tempfile.gettempdir()) / "strela-fault-sweep.strela"
caught, missed, harmless = 0, [], 0
for bit in (7, 31):
    for i in rows:
        addr, w = word(source[i])
        lines = list(source)
        lines[i] = text(addr, w ^ (1 << bit))
        tmp.write_text("\n".join(lines) + "\n", encoding="utf-8")
        survived = status(tmp) == baseline
        if not survived:
            caught += 1
        elif benign(addr, w, bit):
            harmless += 1
        else:
            missed.append(f"{addr:04o}/bit{bit}")
total = caught + harmless + len(missed)
print(f"{total} single-bit faults: {caught} caught, {harmless} cannot change the outcome, {len(missed)} missed")
if missed:
    print("missed:", " ".join(missed))
