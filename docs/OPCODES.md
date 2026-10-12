# Strela opcode matrix

The primary source is Kitov and Krinitsky, *Electronic Digital Machines and Programming* (1959), `docs/ecm_i_prog.pdf`, §28, printed pp. 127–140. Pages 128–134 contain an image-only table in this scan. The searchable [Russian Virtual Computer Museum table](https://www.computer-museum.ru/english/strcomm.htm#2.3) supplies the text of that table; the book's prose on pp. 135–140 checks examples and corner cases. The support column describes the current implementation. A dash in Fixture means acceptance coverage remains to be added. Timing B means one throughput-derived nominal 500 µs instruction interval; the separately sourced main tact is 220 microseconds. E is an estimate (see [TIMING.md](TIMING.md)). Source key M is the museum table, B135–140 the book prose, and C the Carr report on Soviet computers as reprinted by Bell and Newell, whose subprogram counts are approximate. A row marked unverified records an implementation rule without a located source.

| Octal | Operands and effect | Omega; stop | Time | Source/confidence | Support | Fixture |
| --- | --- | --- | --- | --- | --- | --- |
| 00 | no operation | clear; control switch | B | B127 documented | yes | hello |
| 01 | a+b → c, normalized, rounded | negative; overflow | B | M documented | raw integer | word test |
| 02 | add three 12-bit address fields, retain a tail → c | clear | B | M documented | yes | opcodes |
| 03 | a−b → c, normalized | negative; overflow | B | M documented | raw integer | word test |
| 04 | abs(a)−abs(b) → c | negative; overflow | B | M documented | raw integer | word test |
| 05 | normalized a×b → c, otherwise zero | abs(c)≥1; overflow | B | M documented | raw integer | word test |
| 06 | order(a)+order(b), mantissa(a) → c | order(c)≥1; overflow | B | M documented | yes | opcodes |
| 07 | order(a)−order(b), mantissa(a) → c | order(c)≥1; overflow | B | M documented | yes | opcodes |
| 10 | magnitude(a), sign(b) → c | order(c)≥1 | B | M documented | yes | opcodes |
| 11 | a AND b → c | zero | B | B135 documented | yes | bit-mask |
| 12 | a+b → c without alignment rounding | zero; overflow | B | M documented | raw integer | word test |
| 13 | a OR b → c | zero | B | B135 documented | yes | opcodes |
| 14 | shift a by b order or encoded direction/count → c; counts above 37 give zero | zero | B | B135–136 documented; cutoff unverified | yes | opcodes |
| 15 | subtract three address fields, retain a tail → c | clear | B | M documented | yes | opcodes |
| 16 | a XOR b → c | nonzero | B | B136 documented | yes | opcodes |
| 17 | 43-bit end-around sum a+b → c | clear | B | B136 documented | yes | control-add |
| 20 | conditional transfer using previous omega; zero c | clear | B | B137 documented | yes | two-branches |
| 25 | start tape zone supply at a; b=c=0 | clear; invalid zone | B+E | M documented motion estimated | event queue | timing |
| 26 | a XOR b → c; display a,b and stop if mismatch | nonzero; mismatch stop | B | M documented | distinct stop | operator |
| 27 | conditional transfer; place return instruction in c | clear | B | M documented | yes | opcodes |
| 30 | repeat next arithmetic/logic b+1 times; a nonzero c of the prefix receives zero | per inner operation | B+repetitions | M documented; c rule unverified | cursor | opcodes |
| 31 | 30, increment inner c | per inner operation | B+repetitions | M documented | cursor | opcodes |
| 32 | 30, increment inner b | per inner operation | B+repetitions | M documented | cursor | opcodes |
| 33 | 30, increment inner b,c | per inner operation | B+repetitions | M documented | cursor | opcodes |
| 34 | 30, increment inner a | per inner operation | B+repetitions | M documented | cursor | opcodes |
| 35 | 30, increment inner a,c | per inner operation | B+repetitions | M documented | cursor | opcodes |
| 36 | 30, increment inner a,b | per inner operation | B+repetitions | M documented | cursor | opcodes |
| 37 | 30, increment inner a,b,c | per inner operation | B+repetitions | M documented | cursor | opcodes |
| 40 | stop, display a,b; c=0 | normal stop | B | M documented | distinct stop | hello |
| 41 | b+1 card words → RAM at c; a=0 | end of deck | B+E | M documented transport estimated | owned deck | media |
| 43 | b+1 words tape zone a → RAM c | absent/short zone | B+E | M documented transport estimated | owned zones | media |
| 44 | b+1 RAM words at a → cards; c=0 | media failure | B+E | M documented transport estimated | owned deck | media |
| 45 | b+1 RAM words at a → RAM c | address fault | B per word | M documented | yes | media |
| 46 | b+1 RAM words at c → tape zone a | media failure | B+E | M documented transport estimated | owned zones | media |
| 60 | checked transfer, b count, source a to destination c; checksum via 17 | mismatch, display sums | E | M documented; sum-before-data profile selected | yes | media |
| 62 | b+1 reciprocals a.. → c.. | zero/domain | about 20 each | M; C timing approximate | libm | opcodes |
| 63 | b+1 square roots; last reciprocal root → cell 2 | negative/domain | about 40 each | M; C timing approximate | libm | opcodes |
| 64 | b+1 exponentials | overflow | about 40 each | M; C timing approximate | libm | opcodes |
| 66 | b+1 natural logarithms | nonpositive/domain | about 60 each | M; C timing approximate | libm | opcodes |
| 67 | b+1 sines | domain if any | about 25 each | M; C timing approximate | libm | opcodes |
| 70 | b+1 binary to BCD words | exponent/domain | E 50 each | M documented conversion | libm | core; Kitov |
| 72 | b+1 BCD to binary words; exponent limit ±19 inferred | invalid BCD/domain | E 50 each | M documented conversion | libm | core; programs |
| 73 | b+1 arctangents | conversion fault | E 40 each | M documented operation | libm | opcodes |
| 74 | b+1 arcsines; last arccosine → cell 2 | abs(a)>1/domain | E 40 each | M documented operation | libm | opcodes |

The printed book table assigns no operation to 21–24, 42, 47–57, 61, 65, 71, 75–77; those codes are unknown opcodes, not aliases. Zero is stated separately in the book prose. The museum table presents 05 before 04 and 12 before 11, but both distinct octal codes are present; there is no duplicate opcode. The 44 English row says the third address is zero while describing a transfer beginning at c; the source address is a, as the row's title and book agree. For 60, this matrix selects the book/museum sum-before-data variant for memory destinations; a documented machine variant placed the sum after the array. The checked-transfer choice is exercised by tests/test_media.c. Fixtures name CTest groups; rows marked with host libm use approximate numeric checks rather than bit-exact ROM outputs.

## Zero, negative zero and non-normalized operands

Rows 01, 03, 04 and 12 accept non-normalized operands and normalize the result. A negative zero operand counts as zero, and an exact zero result is stored as positive zero. Row 05 returns zero when either mantissa lacks its leading bit, which covers zero, negative zero and non-normalized operands, as the museum row's "otherwise zero" states. Rows 06 and 07 keep the sign and mantissa of a unchanged, so they can preserve a negative zero or a non-normalized mantissa; an order below −63 gives zero. Row 10 can produce a negative zero by copying the sign of b onto a zero magnitude. Rows 02 and 15 operate on address fields, and rows 11, 13, 16, 17 and 26 on raw bits. Rows 62 to 74 convert through host double, where negative zero equals zero: 62 stops with a domain stop for either zero, and the other functions store a zero result as positive zero.
