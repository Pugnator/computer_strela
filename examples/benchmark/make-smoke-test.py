#!/usr/bin/env python3
"""Assemble the Strela smoke test and benchmark program.

The program checks every instruction class and all free RAM, cells 0400-3777.
Each check ends with opcode 26: a mismatch stops the machine with the actual
and expected words on the panel, at the address of the failed check. When all
checks pass, the program punches the number of tests and of memory cells
tested, as decimal words, and stops normally.

Expected words are computed here from the rules in the 1959 book (number
format, address arithmetic, end-around addition, shifts), not taken from the
emulator. Run: python examples/benchmark/make-smoke-test.py
It writes smoke-test.strela beside this script and a deliberately faulty
variant to tests/fixtures/benchmark/smoke-test-bad-checksum.strela.
"""
import math
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
MASK = (1 << 43) - 1
ALL43 = MASK


# ---- word formats -----------------------------------------------------------
def cmd(a, b, c, x, o):
    return (a << 31) | (b << 19) | (c << 7) | (x << 6) | o


def fields(w):
    return f"{(w >> 31) & 0o7777:04o} {(w >> 19) & 0o7777:04o} {(w >> 7) & 0o7777:04o} {(w >> 6) & 1} {w & 0o77:02o}"


def num(v):
    """Exact binary number: sign, 35-bit normalized mantissa, signed order."""
    if v == 0:
        return 0
    f, e = math.frexp(abs(v))
    m = f * 2**35
    assert m == int(m), f"{v} is not exact in 35 bits"
    assert -63 <= e <= 63
    return (1 << 42 if v < 0 else 0) | (int(m) << 7) | ((64 | -e) if e < 0 else e)


def bcd(text):
    """Decimal word for a short exact decimal such as '1.5'."""
    sign = text.startswith("-")
    digits = text.lstrip("-").replace(".", "")
    point = text.lstrip("-").find(".")
    exponent = (point if point >= 0 else len(digits))
    digits = digits.lstrip("0")
    exponent -= len(text.lstrip("-").split(".")[0]) - len(text.lstrip("-").split(".")[0].lstrip("0"))
    mant = int((digits + "0" * 9)[:9])
    field = 0
    for place in range(9):
        field |= (mant % 10) << (place * 4)
        mant //= 10
    return (1 << 42 if sign else 0) | (field << 6) | ((1 << 5) if exponent < 0 else 0) | abs(exponent)


def addfields(w, k, sign=1):
    """Opcodes 02 and 15: each 12-bit address field added (or subtracted)
    modulo 10000 octal, no carry between fields; check bit and opcode kept."""
    out = w & 0o177
    for shift in (31, 19, 7):
        f = ((w >> shift) & 0o7777) + sign * ((k >> shift) & 0o7777)
        out |= (f % 0o10000) << shift
    return out


def endaround(a, b):
    """Opcode 17: 43-bit sum with the carry from the top digit added back."""
    s = a + b
    return ((s & MASK) + (s >> 43)) & MASK


# ---- tiny assembler ---------------------------------------------------------
class Program:
    def __init__(self, origin):
        self.origin = origin
        self.items = []          # (label or None, kind, payload, comment)
        self.tests = 0

    def label(self, name):
        self.items.append((name, "label", None, None))

    def op(self, o, a=0, b=0, c=0, comment=None, check=0):
        self.items.append((None, "cmd", (a, b, c, o, check), comment))

    def word(self, name, value, comment=None):
        self.items.append((name, "word", value, comment))

    def check(self, actual, expected, comment):
        """26: XOR the two words, stop with both on the panel if they differ."""
        self.op(0o26, actual, expected, 0, comment)

    def near(self, actual, expected, comment, scratch="D"):
        """|actual - expected| < EPS, else stop with both words on the panel.
        Successive checks use different scratch cells, so a lost write leaves
        a stale value that cannot pass."""
        n = len(self.items)
        self.op(0o03, actual, expected, scratch, comment)
        self.op(0o04, scratch, "EPS", 0, "omega = 1 when the difference is below EPS")
        self.op(0o20, f"@{n + 3}", f"@{n + 4}", 0, "omega = 0 means too far apart")
        self.op(0o26, actual, expected, 0, "stop: the two values differ")

    def passed(self, title):
        self.tests += 1
        self.op(0o01, "COUNT", 0o7400, "COUNT", f"test {self.tests} passed: {title}")

    def assemble(self):
        addr, labels, index_addr = self.origin, {}, {}
        for i, (name, kind, _, _) in enumerate(self.items):
            index_addr[i] = addr
            if kind == "label":
                labels[name] = addr
                continue
            if name:
                labels[name] = addr
            addr += 1
        self.end = addr

        def resolve(x):
            if isinstance(x, int):
                return x
            if x.startswith("@"):
                return index_addr[int(x[1:])]
            base, _, off = x.partition("+")
            return labels[base] + (int(off, 8) if off else 0)

        rows = []
        for i, (name, kind, payload, comment) in enumerate(self.items):
            if kind == "label":
                rows.append(("label", name))
                continue
            a = index_addr[i]
            if kind == "cmd":
                x, y, z, o, check = payload
                w = cmd(resolve(x), resolve(y), resolve(z), check, o)
            else:
                w = payload
            rows.append(("word", a, w, comment))
        return rows, labels


# ---- the test program -------------------------------------------------------
P = Program(0o20)
CDD_ONE, CDD_TWO, CDD_HALF, CDD_PI2, CDD_INT, CDD_ADDRMASK = 0o7400, 0o7455, 0o7461, 0o7560, 0o7575, 0o7434

P.label("Zero cell and constant store")
P.op(0o13, CDD_ONE, 0, 0, "try to write 1 into cell 0000")
P.op(0o11, 0, "ALLONES", "R", "R = (0000) AND ones")
P.check("R", "ZERO", "cell 0000 must still read zero")
P.op(0o01, CDD_HALF, CDD_HALF, "R", "1/2 + 1/2 from cell 7461")
P.check("R", CDD_ONE, "equals the constant 1 in cell 7400")
P.op(0o01, CDD_ONE, CDD_ONE, "R", "1 + 1")
P.check("R", CDD_TWO, "equals the constant 2 in cell 7455")
P.passed("zero cell and constants")

P.label("Addition and subtraction, 01 03 04")
P.op(0o01, "N075", "N025", "R", "0.75 + 0.25")
P.check("R", CDD_ONE, "= 1")
P.op(0o03, "N3", "N5", "R", "3 - 5")
P.check("R", "M2", "= -2")
P.op(0o04, "M5", "N3", "R", "|-5| - |3|")
P.check("R", CDD_TWO, "= 2")
P.passed("addition and subtraction")

P.label("Rounding: 01 rounds, 12 truncates (integer part with cell 7575, book p. 140)")
P.op(0o12, "N375", CDD_INT, "R", "integer part of 3.75 without rounding")
P.check("R", "N3", "= 3")
P.op(0o01, "N375", CDD_INT, "R", "3.75 rounded to the nearest integer")
P.check("R", "N4", "= 4")
P.passed("rounding 01 and truncation 12")

P.label("Multiplication 05, order operations 06 07, sign transfer 10")
P.op(0o05, "N25", "N4", "R", "2.5 x 4")
P.check("R", "N10", "= 10")
P.op(0o05, "M3", "N5", "R", "-3 x 5")
P.check("R", "M15", "= -15")
P.op(0o06, CDD_ONE, "N4", "R", "1 with order raised by the order of 4 (3)")
P.check("R", "N8", "= 8")
P.op(0o07, "N8", "N4", "R", "8 with order lowered by 3")
P.check("R", CDD_ONE, "= 1")
P.op(0o10, "N5", "M3", "R", "magnitude of 5 with the sign of -3")
P.check("R", "M5", "= -5")
P.passed("multiplication, order and sign operations")

P.label("Logic 11 13 16 (book p. 135 mask example)")
P.op(0o11, "BMWORD", "BMMASK", "R", "keep the third address")
P.check("R", "BMRES", "= 0000 0000 0115 0 00")
P.op(0o13, "ORA", "ORB", "R", "logical OR")
P.check("R", "ORRES", "expected OR")
P.op(0o16, "ORA", "ORA", "R", "x XOR x")
P.check("R", "ZERO", "= 0")
P.passed("logical operations")

P.label("Shifts 14 by address and by order (book p. 136)")
P.op(0o14, "SHW", 0o4112, "R", "shift right 10, form 4100 + k")
P.check("R", "SHR10", "expected")
P.op(0o14, "SHW", 0o4005, "R", "shift left 5, form 4000 + k")
P.check("R", "SHL5", "expected")
P.op(0o14, "SHW", "SHCOUNT", "R", "shift right 10 by the order of a number")
P.check("R", "SHR10", "expected")
P.passed("shifts")

P.label("Address arithmetic 02 15, end-around addition 17 (book pp. 136-139)")
P.op(0o02, "AS1", "AS2", "R", "special addition")
P.check("R", "AS3", "= 0076 0135 0061 0 01")
P.op(0o15, "SS1", "SS2", "R", "special subtraction")
P.check("R", "SS3", "= 1124 0222 0222 0 04")
P.op(0o17, "EA1", "EA2", "R", "end-around sum, example 3")
P.check("R", "EA3", "= 7160 2474 7531 0 00")
P.op(0o17, "EB1", "EB2", "R", "end-around sum, example 4 (carry wraps)")
P.check("R", "EB3", "= 1445 7644 0441 0 00")
P.op(0o17, "EC1", "ALLONES", "R", "7777 7777 7777 1 77 acts as zero")
P.check("R", "EC1", "unchanged")
P.passed("address arithmetic and end-around addition")

P.label("Conditional transfer 20 on omega")
n = len(P.items)
P.op(0o03, CDD_ONE, CDD_TWO, 0, "1 - 2 < 0 gives omega = 1")
P.op(0o20, f"@{n + 2}", f"@{n + 3}", 0, "omega = 1 must take the second address")
P.op(0o26, CDD_ONE, "ZERO", 0, "stop: wrong branch for omega = 1")
n = len(P.items)
P.op(0o03, CDD_TWO, CDD_ONE, 0, "2 - 1 > 0 gives omega = 0")
P.op(0o20, f"@{n + 3}", f"@{n + 2}", 0, "omega = 0 must take the first address")
P.op(0o26, CDD_TWO, "ZERO", 0, "stop: wrong branch for omega = 0")
P.passed("conditional transfer")

P.label("Subroutine call 27 with self-erasing return (book p. 138)")
P.op(0o27, "SUB", "SUB", "RET", "call; the return command goes into RET")
P.op(0o27, "SUB", "SUB", "RET", "call again")
P.check("CALLS", CDD_TWO, "two calls counted")
P.check("RET", "ZERO", "the return command erased itself")
P.passed("subroutine call and return")

P.label("Group operations 30, 37, 34 and block transfer 45 (book p. 139)")
P.op(0o13, CDD_ONE, 0, "PW", "PW = 1")
P.op(0o30, 0, 0o11, 0, "repeat the next command 10 times")
P.op(0o05, CDD_TWO, "PW", "PW", "PW = 2 x PW")
P.check("PW", "N1024", "2^10 = 1024")
P.op(0o37, 0, 0o3, 0, "4 pairwise products, all addresses advance")
P.op(0o05, "V1", "V2", "V3", "V3[i] = V1[i] x V2[i]")
P.op(0o13, "ZERO", 0, "S", "S = 0")
P.op(0o34, 0, 0o3, 0, "sum of 4 cells, first address advances")
P.op(0o01, "V3", "S", "S", "S = S + V3[i]")
P.check("S", "N70", "5 + 12 + 21 + 32 = 70")
P.op(0o45, "V1", 0o3, "V4", "copy V1 to V4 with transfer 45")
P.op(0o13, "ZERO", 0, "S", "S = 0")
P.op(0o34, 0, 0o3, 0, "end-around sum of V4")
P.op(0o17, "V4", "S", "S")
P.check("S", "V1SUM", "same words as V1")
P.passed("group operations and block transfer")

P.label("Standard functions 62-74 and decimal conversion 70 72")
P.op(0o62, "R124", 0o2, "R3", "reciprocals of 1, 2, 4 as one batch")
P.op(0o13, "ZERO", 0, "S", "S = 0")
P.op(0o34, 0, 0o2, 0, "sum the three reciprocals")
P.op(0o01, "R3", "S", "S")
P.check("S", "N175", "1 + 0.5 + 0.25 = 1.75")
P.op(0o63, "N16", 0, "R", "square root of 16")
P.check("R", "N4", "= 4")
P.check(0o0002, "N025", "cell 0002 receives 1/sqrt(16) = 0.25")
P.op(0o64, "N05", 0, "F1", "e^0.5")
P.op(0o66, "F1", 0, "F2", "ln(e^0.5)")
P.near("F2", CDD_HALF, "returns 0.5")
P.op(0o67, "N025", 0, "F1", "sin 0.25")
P.op(0o74, "F1", 0, "F2", "arcsin(sin 0.25); arccos goes to cell 0002")
P.near("F2", "N025", "returns 0.25", scratch="R")
P.op(0o01, "F2", 0o0002, "F3", "arcsin + arccos")
P.near("F3", CDD_PI2, "= pi/2, constant 7560", scratch="S")
P.op(0o73, "R124", 0, "F1", "arctan 1 = pi/4")
P.op(0o01, "F1", "F1", "F2", "2 arctan 1")
P.near("F2", CDD_PI2, "= pi/2", scratch="F3")
P.op(0o72, "D15", 0, "R", "decimal 1.5 to binary")
P.check("R", "N15", "= binary 1.5")
P.op(0o70, "N15", 0, "R", "binary 1.5 to decimal")
P.check("R", "D15", "= decimal 1.5")
P.passed("standard functions and decimal conversion")

P.label("Memory: address pattern over cells 0400-3777 (1792 words)")
P.op(0o13, "SEED", 0, 0o400, "first cell")
P.op(0o35, 0, 0o3376, 0, "1791 repetitions, first and third addresses advance")
P.op(0o02, 0o400, "KSTEP", 0o401, "cell[i+1] = cell[i] + 0001 0001 0001 (special addition)")
P.op(0o13, "ZERO", 0, "S", "S = 0")
P.op(0o34, 0, 0o3377, 0, "end-around sum of all 1792 cells")
P.op(0o17, 0o400, "S", "S")
P.check("S", "SUM1", "checksum of the address pattern")
P.check(0o3777, "LAST1", "spot check of the last cell")
P.check(0o2000, "MID1", "spot check of cell 2000")
P.passed("memory address pattern")

P.label("Memory: every bit inverted")
P.op(0o35, 0, 0o3377, 0, "1792 repetitions")
P.op(0o16, 0o400, "ALLONES", 0o400, "cell = cell XOR all 43 ones")
P.op(0o13, "ZERO", 0, "S", "S = 0")
P.op(0o34, 0, 0o3377, 0)
P.op(0o17, 0o400, "S", "S")
P.check("S", "SUM2", "checksum of the inverted pattern")
P.passed("memory inversion")

P.label("Memory: block transfer of 896 words, 0400-2177 to 2200-3777")
P.op(0o45, 0o400, 0o1577, 0o2200, "copy the lower half over the upper half")
P.op(0o13, "ZERO", 0, "S", "S = 0")
P.op(0o34, 0, 0o1577, 0, "sum of the upper half")
P.op(0o17, 0o2200, "S", "S")
P.check("S", "SUM3", "the copy has the lower half's checksum")
P.passed("memory block transfer")

P.label("Every test ran; punch the results and stop")
P.check("COUNT", "NTESTS", "number of tests passed")
P.op(0o13, "COUNT", 0, "OUT", "OUT = tests passed")
P.op(0o13, "NMEM", 0, "OUT+1", "OUT+1 = memory cells tested")
P.op(0o70, "OUT", 0o1, "OUT", "convert both to decimal")
P.op(0o44, "OUT", 0o1, 0, "punch them")
P.op(0o40, "OUT", "OUT+1", 0, "normal stop; the panel shows both results")

P.label("Subroutine for test 9")
P.label("SUB")
P.op(0o01, "CALLS", CDD_ONE, "CALLS", "count the call")
P.word("RET", 0, "return cell; the call places 20 L+1 L+1 RET here")

# ---- data -------------------------------------------------------------------
SEED = cmd(0o1234, 0o5670, 0o0123, 1, 0o52)
KSTEP = cmd(1, 1, 1, 0, 0)
cells = [SEED]
for _ in range(0o3377):
    cells.append(addfields(cells[-1], KSTEP))
assert len(cells) == 0o3400
SUM1 = 0
for w in cells:
    SUM1 = endaround(SUM1, w)
inverted = [w ^ ALL43 for w in cells]
SUM2 = 0
for w in inverted:
    SUM2 = endaround(SUM2, w)
SUM3 = 0
for w in inverted[:0o1600]:
    SUM3 = endaround(SUM3, w)

SHW = cmd(0o5432, 0o1076, 0o5432, 1, 0o67)
V1 = [num(v) for v in (1, 2, 3, 4)]
V1SUM = 0
for w in V1:
    V1SUM = endaround(V1SUM, w)

data = [
    ("COUNT", 0, "tests passed (binary)"), ("NTESTS", num(P.tests), f"{P.tests} tests"),
    ("NMEM", num(0o3400), "1792 memory cells"),
    ("ZERO", 0, "0"), ("EPS", num(2.0**-24), "tolerance 2^-24 for standard functions"),
    ("ALLONES", ALL43, "all 43 digits one"),
    ("N05", num(0.5), "0.5 (standard functions read RAM only)"), ("N025", num(0.25), "0.25"), ("N075", num(0.75), "0.75"), ("N3", num(3), "3"), ("N4", num(4), "4"),
    ("N5", num(5), "5"), ("N8", num(8), "8"), ("N10", num(10), "10"), ("N16", num(16), "16"),
    ("N25", num(2.5), "2.5"), ("N375", num(3.75), "3.75"), ("N15", num(1.5), "1.5"),
    ("N175", num(1.75), "1.75"), ("N70", num(70), "70"), ("N1024", num(1024), "1024"),
    ("M2", num(-2), "-2"), ("M3", num(-3), "-3"), ("M5", num(-5), "-5"),
    ("M15", num(-15), "-15"), ("D15", bcd("1.5"), "decimal 1.5"),
    ("BMWORD", cmd(0o0125, 0o0675, 0o0115, 0, 0o01), "book p. 135 command word"),
    ("BMMASK", cmd(0, 0, 0o7777, 0, 0), "mask of the third address"),
    ("BMRES", cmd(0, 0, 0o0115, 0, 0), "expected AND"),
    ("ORA", cmd(0o0123, 0o4567, 0, 0, 0), "OR operand"), ("ORB", cmd(0, 0, 0o7654, 1, 0o21), "OR operand"),
    ("ORRES", cmd(0o0123, 0o4567, 0, 0, 0) | cmd(0, 0, 0o7654, 1, 0o21), "expected OR"),
    ("SHW", SHW, "word to shift"), ("SHL5", (SHW << 5) & MASK, "shifted left 5"),
    ("SHR10", SHW >> 10, "shifted right 10"),
    ("SHCOUNT", cmd(0, 0, 0, 1, 0o12), "shift count: order -10, shift right 10 digits"),
    ("AS1", cmd(0o0075, 0o0133, 0o0056, 0, 0o01), "book p. 138"), ("AS2", cmd(1, 2, 3, 0, 0), "1(I) 2(II) 3(III)"),
    ("AS3", addfields(cmd(0o0075, 0o0133, 0o0056, 0, 0o01), cmd(1, 2, 3, 0, 0)), "expected 0076 0135 0061 0 01"),
    ("SS1", cmd(0o1125, 0o0223, 0o0224, 0, 0o04), "book p. 139"), ("SS2", cmd(1, 1, 2, 0, 0o10), "subtrahend"),
    ("SS3", addfields(cmd(0o1125, 0o0223, 0o0224, 0, 0o04), cmd(1, 1, 2, 0, 0o10), -1), "expected 1124 0222 0222 0 04"),
    ("EA1", cmd(0o2271, 0o3533, 0o0757, 1, 0o17), "book p. 137 example 3"), ("EA2", cmd(0o4666, 0o6741, 0o6551, 0, 0o61), "operand"),
    ("EA3", endaround(cmd(0o2271, 0o3533, 0o0757, 1, 0o17), cmd(0o4666, 0o6741, 0o6551, 0, 0o61)), "expected"),
    ("EB1", cmd(0o2650, 0o2365, 0o7053, 0, 0o63), "book p. 137 example 4"), ("EB2", cmd(0o6575, 0o5256, 0o1365, 1, 0o14), "operand"),
    ("EB3", endaround(cmd(0o2650, 0o2365, 0o7053, 0, 0o63), cmd(0o6575, 0o5256, 0o1365, 1, 0o14)), "expected"),
    ("EC1", cmd(0o4157, 0o0213, 0o0621, 1, 0o23), "book p. 137 example 5"),
    ("CALLS", 0, "subroutine call counter"), ("PW", 0, "power of two"),
    ("V1", V1[0], "1"), ("V1+1", V1[1], "2"), ("V1+2", V1[2], "3"), ("V1+3", V1[3], "4"),
    ("V2", num(5), "5"), ("V2+1", num(6), "6"), ("V2+2", num(7), "7"), ("V2+3", num(8), "8"),
    ("V3", 0, "products"), ("V3+1", 0, ""), ("V3+2", 0, ""), ("V3+3", 0, ""),
    ("V4", 0, "copy of V1"), ("V4+1", 0, ""), ("V4+2", 0, ""), ("V4+3", 0, ""),
    ("V1SUM", V1SUM, "end-around sum of V1"),
    ("R124", num(1), "1"), ("R124+1", num(2), "2"), ("R124+2", num(4), "4"),
    ("R3", 0, "reciprocals"), ("R3+1", 0, ""), ("R3+2", 0, ""),
    ("SEED", SEED, "first memory word"), ("KSTEP", KSTEP, "pattern step"),
    ("SUM1", SUM1, "checksum of the address pattern"), ("LAST1", cells[-1], "expected cell 3777"),
    ("MID1", cells[0o2000 - 0o400], "expected cell 2000"),
    ("SUM2", SUM2, "checksum after inversion"), ("SUM3", SUM3, "checksum of the copied half"),
    # Scratch cells start with values no check expects, so a lost write shows.
    ("R", num(-7.125), "result cell"), ("D", num(-6.875), "difference"), ("S", num(-5.625), "sum"),
    ("F1", num(-4.375), "function value"), ("F2", num(-3.125), ""), ("F3", num(-2.875), ""),
    ("OUT", 0, "results to punch"), ("OUT+1", 0, ""),
]
previous = ""
for name, value, comment in data:
    comment = comment or previous      # continuation words share their array's note
    previous = comment
    P.word(name if "+" not in name else None, value, comment)


def write(path, rows, faulty=False):
    out = [
        "# Strela smoke test and benchmark.",
        "# Generated by examples/benchmark/make-smoke-test.py; see examples/benchmark/README.md.",
        f"# {P.tests} tests of every instruction class and of RAM cells 0400-3777.",
        "# A failed check stops with 'comparison mismatch' at the check; the panel",
        "# shows the actual and expected words. A pass punches the number of tests",
        "# and of memory cells tested (decimal) and stops normally at the last 40.",
    ]
    if faulty:
        out.insert(1, "# FAULT INJECTION: SUM1 is deliberately wrong; the memory test must stop.")
    out.append("START 0020")
    for row in rows:
        if row[0] == "label":
            out.append("# --- " + row[1])
            continue
        _, a, w, comment = row
        if faulty and a == labels["SUM1"]:
            w ^= 1
        out.append(f"{a:04o} {fields(w)}" + "")
        if comment:
            out[-1:] = [f"# {a:04o}: {comment}", out[-1]]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


rows, labels = P.assemble()
assert P.end <= 0o400, f"program and data end at {P.end:04o}, past cell 0377"
write(HERE / "smoke-test.strela", rows)
write(ROOT / "tests" / "fixtures" / "benchmark" / "smoke-test-bad-checksum.strela", rows, faulty=True)
print(f"{P.tests} tests; program and data occupy 0020-{P.end - 1:04o}; "
      f"memory test 0400-3777; SUM1 at {labels['SUM1']:04o}")
