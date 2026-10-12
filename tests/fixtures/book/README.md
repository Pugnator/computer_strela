# Book-derived fixtures

All fields below are octal except the check bit. The local source is Kitov and Krinitsky, *Electronic Digital Machines and Programming* (1959), `docs/ecm_i_prog.pdf`, section 28, printed pp. 135–137. The scanned command table on pp. 128–134 is image-only in the available PDF text layer; the [Russian Virtual Computer Museum command table](https://www.computer-museum.ru/english/strcomm.htm) is the searchable cross-reference.

- `bit-mask.strela`: printed p. 135 gives instruction word `0125 0675 0115 0 01`, mask `0000 0000 7777 0 00`, AND command `0020 0376 0500 0 11`, and result `0000 0000 0115 0 00`. The listing adds a stop and can be executed at `0010`.
- `control-add.strela`: printed p. 136 example 3 gives operands `2271 3533 0757 1 17`, `4666 6741 6551 0 61`, and end-around result `7160 2474 7531 0 00`. The listing adds a `17` command and a stop at `0010`.
- Printed p. 136 example 4 gives another vector: `2650 2365 7053 0 63` plus `6575 5256 1365 1 14` produces `1445 7644 0441 0 00`.
- Printed p. 135 gives decimal encoding `1621 5210 0000 0 42` for +0.0072351. This is a format vector, not a standalone program.

- `kitov-linear.strela`: Kitov and Krinitsky, *Electronic Computing Machines* (1958), `docs/kitov_evm.pdf`, printed pp. 45-46, table 4 concrete program and stated input A=1, B=2, C=5, D=7, E=5, F=17. The listing preloads the BCD words as an operator shortcut. It expects x near 1 and y near 2 at cells 0051/0052, then punches two results and stops.

