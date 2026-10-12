# Source comparison

## Habr article, 2016

[“Серийная ЭВМ «Стрела»”](https://habr.com/ru/companies/ua-hosting/articles/371785/)
is useful for the system's physical operation and historical context. It
describes an operator console able to start and stop the machine, inspect
execution, and enter or display individual RAM words while stopped. Its card
workflow has a separate keyboard and input punch, a reader for prepared decks,
and an output punch. These details support a future console and card-deck UI.
The article also mentions a 4096-word magnetic drum in the final Strela
modification; the current emulator targets the tape-based configuration.

For machine code and address limits, the 1959 book and the Russian Virtual
Computer Museum architecture page remain the working references. The Habr
article appears to contain several transcription errors or conflicting
figures:

| Habr wording | Working interpretation | Reason |
| --- | --- | --- |
| Cells numbered from bit 0 to 42, then calls the last bit 43 | 0 to 42 | 43-bit word, as the book states |
| “2-bit addresses” in the prose | 12-bit addresses | Its own specification and the book both say 12 |
| 253 zones per tape in the physical description | Addressable zones `4001`–`4777` and `5001`–`5777` (511 each) | The book explicitly gives these ranges and up to 511 zones; 253 may describe a particular physical tape layout |
| 200,000 total tape words in one paragraph, 1.5 million in the specifications | The selected historical profile enforces 100,000 recorded words per tape | The book gives up to 100,000 words per tape |
| Opcode `46` described as transferring cards to RAM | RAM to tape | The opcode name, book and museum instruction table agree |
| Opcode `44` says RAM transfer begins at address `c` | Begins at `a` | The book and museum instruction table agree |

The article repeats the 500 µs main cycle and 20 µs RAM access cycle already
reported by the [museum's Strela profile](https://www.computer-museum.ru/english/strela.htm).
It does not supply a verified tape or card transfer rate, so the current
device timing estimates remain unchanged.

## Polytechnic Museum collection record

The [Polytechnic Museum's “Strela computer blocks” collection entry](https://polymus.ru/collection/detail/bloki-elektronnoy-vychislitelnoy-mashiny-strela)
documents three surviving modules dated 1954–1955 (inventory `23147/1-3`):

- The manual control panel could start and stop the machine, follow program
  execution, and enter information into memory.
- The memory inspection block let an operator view the contents of any of the
  cathode-ray storage tubes. A future console view should expose memory in a
  way that reflects this operator function, while keeping the 43-bit RAM model.
- A nine-tube panel belonged to the central control unit's number-address
  generation block. This is useful visual evidence for a future museum-style
  interface, but it does not specify an instruction-level timing rule.

The entry also states that no complete Strela survives. It describes an
aerodynamic calculation for the Tu-104 that took 17 hours for 100 million
operations. That works out to about 1,634 operations/s, or 612 µs per operation
on average. The figure is an observed workload average, so it does not replace
the separately documented 500 µs basic machine cycle. It could include longer
commands, transfers, and operator or device time; the entry does not break the
17 hours down by cause.

## Ershov and Shura-Bura timing table

The [archive table](https://ershov.iis.nsk.su/ru/foundation/index) places 220 us in the Strela column for the main tact, alongside 2,000 operations/s and 30 us memory access. The [Computer Museum profile](https://www.computer-museum.ru/english/strela.htm) reports 500 us clock time, 2,000 commands/s, and 20 us memory access. The terms and RAM access figures conflict. We retain the sourced 220 us main tact and a distinct 500 us throughput-derived nominal operation interval. Their exact hardware relationship is unresolved; neither source gives a per-opcode tact count.

## Unverified implementation rules

Three rules in the code have no located source. Each is marked in a code comment and in docs/OPCODES.md:

- Opcode 14 gives zero for shift counts above 37. The 1959 book, printed p. 136, puts the count in bits 37–42 and shows only small shifts.
- A group prefix (30–37) with a nonzero third address writes zero there. The book's group examples on printed pp. 139–140 all use 0000.
- BCD conversion (70 and 72) limits the decimal exponent to ±19. This matches the binary range of about 2^63, while the five exponent bits alone would allow 31.

The Russian Virtual Computer Museum command table could not be reached when these rules were reviewed on 2026-10-12. Check them against its rows for 14, 30–37, 70 and 72.

## Local books added in October 2026

The [1959 Kitov and Krinitsky scan](ecm_i_prog.pdf) is the instruction and number-format reference. The [1958 Kitov and Krinitsky scan](kitov_evm.pdf) has a complete 16-command two-equation Strela program on PDF pp. 42-49. Its concrete listing and BCD inputs are preserved in [the Kitov fixture](../tests/fixtures/book/kitov-linear.strela). The printed example reports the second answer as 1.99999999, while the fixture checks the corresponding nine-digit BCD word. The [Vitenberg 1964 scan](%D0%92%D0%B8%D1%82%D0%B5%D0%BD%D0%B1%D0%B5%D1%80%D0%B3_%D0%9D%D0%9C_%20%D0%90%D0%BD%D0%B0%D0%BB%D0%BE%D0%B3%D0%BE%D0%B2%D1%8B%D0%B5_%D0%B2%D1%8B%D1%87%D0%B8%D1%81%D0%BB%D0%B8%D1%82%D0%B5%D0%BB%D1%8C%D0%BD%D1%8B%D0%B5_%D0%BC%D0%B0%D1%88%D0%B8%D0%BD%D1%8B%281964%29-s.pdf) is about analog machines and is not an instruction source. See [BOOKS.md](BOOKS.md).

