# Smoke test and benchmark

[smoke-test.strela](smoke-test.strela) is a Strela program that checks the machine itself. It runs 14 tests that cover every instruction class and all free memory. It finishes in 4.82 s of simulated time, so even at the historical speed of about 2,000 operations per second it takes under 5 s.

| Test | What it checks |
| --- | --- |
| 1 | Cell 0000 stays zero after a write; constants 1, 1/2 and 2 in the constant store |
| 2 | Addition and subtraction, 01 03 04 |
| 3 | Integer part with cell 7575: 12 truncates and 01 rounds (book p. 140) |
| 4 | Multiplication 05, order operations 06 07, sign transfer 10 |
| 5 | Logical operations 11 13 16, with the book's mask example (p. 135) |
| 6 | Shifts 14 by address, both directions, and by the order of a number (p. 136) |
| 7 | Address arithmetic 02 15 and end-around addition 17, with the book's examples (pp. 136–139) |
| 8 | Conditional transfer 20 on both values of omega |
| 9 | Subroutine call 27 and its self-erasing return command (p. 138) |
| 10 | Group operations 30 37 34 and block transfer 45 (p. 139) |
| 11 | Standard functions 62–74 and decimal conversion 70 72 |
| 12 | Memory 0400–3777: an address-dependent pattern, its checksum and spot checks |
| 13 | Memory: every one of the 43 bits inverted in all 1,792 cells |
| 14 | Memory: block transfer of 896 words and comparison of checksums |

Each check ends with opcode 26. If the words differ, the machine stops with "comparison mismatch" at that check, and the panel shows the actual and expected words. If every check passes, the program punches two decimal words and stops normally at 0245. They are the number of tests, 14, and of memory cells tested, 1,792.

The expected words are computed by [make-smoke-test.py](make-smoke-test.py) from the rules in the 1959 book, not taken from the emulator. Standard functions are checked through identities within 2⁻²⁴, because they are host-math approximations. Examples are ln(eˣ) = x and arcsin x + arccos x = π/2.

## Running it

```
build/strela_run run examples/benchmark/smoke-test.strela
build/strela_run bench examples/benchmark/smoke-test.strela --repeat 50
```

`bench` runs the program repeatedly from the same initial state. It checks that every run ends identically and compares simulated time with host CPU time. A speed of at least 1× means the host can emulate the machine in real time. On the development machine one run takes about 0.6 ms of CPU, roughly 8,000× real time.

In the browser console the program is the «Проверка машины» deck in the card file. Tick «Реальная скорость машины» to run it at the historical pace, about 5 s.

## Checking the checker

[fault-sweep.py](fault-sweep.py) flips one bit at a time in every word of the program and data, in the third address and in the first, and runs each variant. A fault is caught when the run does not end exactly like the fault-free run. The current result is 480 faults, of which 338 are caught, 117 cannot change the outcome, and 25 are missed. Faults that cannot change the outcome include an unused third address or a scratch cell that is overwritten before use. Most of the missed faults lie in code that runs only after a check has already failed.

The CTest targets `smoke_test` and `smoke_test_detects_fault` run the program, and a copy with a deliberately wrong memory checksum, at `tests/fixtures/benchmark/smoke-test-bad-checksum.strela`.

## What it found

The first run stopped at test 3. The emulator kept three guard digits through order alignment and dropped them only after normalizing, so 3.75 + (7575) gave 3.75 for both 12 and 01. The book says the digits that leave the 35-digit grid during alignment are lost: 12 drops them, and 01 rounds by the highest lost digit. src/word.c now follows that rule. The book's own example, −1011001.1110001 giving −1011001 with 12, is a unit test in tests/test_word.c.
