# Programs from the Strela books

This folder holds every Strela program printed in the two local books:

- **1958**: A. I. Kitov, N. A. Krinitsky, *Электронные вычислительные машины* (Electronic Computing Machines), [docs/kitov_evm.pdf](../../docs/kitov_evm.pdf).
- **1959**: A. I. Kitov, N. A. Krinitsky, *Электронные цифровые машины и программирование* (Electronic Digital Machines and Programming), [docs/ecm_i_prog.pdf](../../docs/ecm_i_prog.pdf).

Page numbers are printed page numbers. In both scans they equal the PDF page numbers.

## Runnable listings

These use the project listing format (`ADDRESS AAAA BBBB CCCC X OO`). Each file's header names the source, the memory layout, and every change from the printed text. Decimal constants that the book gives as numbers are entered as BCD words computed for the listing. Inputs the book leaves open are marked in a comment.

| File | Problem | Pages | Form in the book |
| --- | --- | --- | --- |
| [1958-linear-equations.strela](1958-linear-equations.strela) | Two linear equations, with the book's data | 1958 pp. 43–47; 1959 pp. 151–154 | Concrete listing and input card |
| [1959-integral-t-power-n.strela](1959-integral-t-power-n.strela) | Integral of tⁿ from 1 to x | 1959 pp. 155–156 | Concrete listing and input card |
| [1959-quadratic-equation.strela](1959-quadratic-equation.strela) | px² + qx + r = 0, real or complex roots | 1959 pp. 156–157 | Symbolic, with all bases given |
| [1959-three-branch-function.strela](1959-three-branch-function.strela) | φ(x) = f(x² + x − 1), three formulas | 1959 pp. 163–165 | Concrete listing (table 36) and input card |
| [1959-iteration-cubic.strela](1959-iteration-cubic.strela) | Root of x³/65 + x − 5.001 = 0 by iteration | 1959 pp. 166–167 | Symbolic, with all bases given, and data |
| [1959-group-sum.strela](1959-group-sum.strela) | Group operation 34: sum of 16 cells | 1959 p. 139 | Two commands |
| [1959-group-add-constant.strela](1959-group-add-constant.strela) | Group operation 35: add a number to 24 cells | 1959 p. 139 | Two commands |
| [1959-group-pairwise-product.strela](1959-group-pairwise-product.strela) | Group operation 37: 64 pairwise products | 1959 p. 139 | Two commands |

Each listing was run in the emulator on test inputs, and the results matched values computed independently. The linear equations give x = 1 and y = 2 from the book's data.

The browser console offers these programs in its card file. [web/generate-catalog.py](../../web/generate-catalog.py) builds Russian-language copies from these listings. Where the book gives no inputs, it adds clearly labelled example data.

Corrections to the printed text:

- **Table 36, cell 0024**: the printed operands are swapped. That word tests |y| − 1 and exchanges the first two branches. The listing uses the operator derived on p. 164, which tests |1| − |y|.
- **Quadratic, h+1**: the collected program prints opcode 13. The step that defines it uses 05, which gives the complex-roots flag 4.
- **Iteration, stop**: the stop is printed as `b+14`, but it is `a+14`. The book also gives the constant as 5.0001 in one formula and 5.001 elsewhere, and the listing uses 5.001.

## Symbolic programs

From section 33 example 4 on, the 1959 book omits input, conversion, output and stop. It also leaves the base letters unassigned. The 1958 sine table leaves its bases open too. These programs cannot run without choosing an allocation, so they are transcribed in the book's notation in [symbolic/](symbolic/). Suspected typos are annotated, not corrected.

| File | Problem | Pages |
| --- | --- | --- |
| [1958-sine-cosine-table.txt](symbolic/1958-sine-cosine-table.txt) | sin n, cos n for n = 1…1000 with a check | 1958 pp. 52–55 |
| [1959-p167-cubic-table-transfer.txt](symbolic/1959-p167-cubic-table-transfer.txt) | Table of x⁵ + 2x + 3, cycle with group transfer | 1959 pp. 167–168 |
| [1959-p168-cubic-table-readdressing.txt](symbolic/1959-p168-cubic-table-readdressing.txt) | Same table, cycle with readdressing | 1959 pp. 168–169 |
| [1959-p170-polynomial-in-sine.txt](symbolic/1959-p170-polynomial-in-sine.txt) | Polynomial in sin x, two restoration methods | 1959 pp. 170–171 |
| [1959-p173-nested-root.txt](symbolic/1959-p173-nested-root.txt) | x² + √(1 + √(1 + x)), cycle working twice | 1959 pp. 173–174 |
| [1959-p175-sine-series.txt](symbolic/1959-p175-sine-series.txt) | sin x by its series, with a counter | 1959 pp. 175–176 |
| [1959-p177-horner.txt](symbolic/1959-p177-horner.txt) | Polynomial by Horner's scheme | 1959 p. 177 |
| [1959-p178-logical-scale.txt](symbolic/1959-p178-logical-scale.txt) | Cycle control with a logical scale | 1959 pp. 178–179 |
| [1959-p180-function-table.txt](symbolic/1959-p180-function-table.txt) | Function table, with and without standard cells | 1959 pp. 180–181 |
| [1959-p182-euler-ode.txt](symbolic/1959-p182-euler-ode.txt) | y′ = 10/(x² + y²) + x by Euler's method with recalculation | 1959 pp. 182–184 |
| [1959-p184-simpson.txt](symbolic/1959-p184-simpson.txt) | Simpson's rule | 1959 pp. 184–185 |
| [1959-p185-select-subsequence.txt](symbolic/1959-p185-select-subsequence.txt) | Select xᵢ ≥ k with a forming operator | 1959 pp. 185–186 |
| [1959-p198-runge-kutta-library.txt](symbolic/1959-p198-runge-kutta-library.txt) | Runge–Kutta step, library subroutine | 1959 pp. 197–199 |
| [1959-p202-symbolic-addresses.txt](symbolic/1959-p202-symbolic-addresses.txt) | Correcting a program written in symbolic addresses | 1959 p. 202 |
| [1959-p205-input-programs.txt](symbolic/1959-p205-input-programs.txt) | Five input routines with checksums | 1959 pp. 205–207 |
| [1959-p207-double-count.txt](symbolic/1959-p207-double-count.txt) | Double and double-triple counting | 1959 pp. 207–209 |
| [1959-p230-table-lookup.txt](symbolic/1959-p230-table-lookup.txt) | Six table-lookup subroutines | 1959 pp. 230–234 |

## Not included

- **Single-command examples**: the 1958 book pp. 42–43 and 51–52, and the 1959 book section 28, pp. 135–140. The bit-mask and end-around-sum examples are already fixtures in [tests/fixtures/book](../../tests/fixtures/book/README.md).
- **Loop-control patterns** without a problem: 1959 pp. 173–178, and the x-to-the-n group example on p. 139, whose count n is open.
- **1959 section 36, pp. 186–192**: a program for the one-address Ural machine, not Strela.
- **1959 pp. 252–284**: the input language of the programming program, not machine code.
- **1959 from p. 285**: machine translation examples for other machines.
- **N. M. Vitenberg (1964)**: analog machines, no Strela code.
