# Research and decisions

## Evidence hierarchy

1. Kitov and Krinitsky, Electronic Digital Machines and Programming (1959), local scan at docs/ecm_i_prog.pdf, chapter V sections 27 and 28, printed pp. 123–141, is the primary source for code, number format, media addresses, and worked examples.
2. [Russian Virtual Computer Museum architecture](https://www.computer-museum.ru/english/strcomm.htm) supplies a searchable instruction table and corroborates the book.
3. [Russian Virtual Computer Museum Strela profile](https://www.computer-museum.ru/english/strela.htm) gives 500 microsecond clock time, 2,000 commands/s, 20 microsecond RAM access, and ROM summary; the Ershov–Shura-Bura table separately gives a 220 microsecond main tact.
4. [Carr's report on Soviet computers, reprinted by Bell and Newell](https://gordonbell.azurewebsites.net/Computer_Structures__Readings_and_Examples/00000234.htm) gives approximate cycle counts for standard subprograms.
5. [Polytechnic Museum collection record](https://polymus.ru/collection/detail/bloki-elektronnoy-vychislitelnoy-mashiny-strela) documents surviving controls and a 100-million-operation workload.
6. [Habr historical article](https://habr.com/ru/companies/ua-hosting/articles/371785/) gives operator and physical context but has transcription conflicts documented in docs/SOURCE-NOTES.md.

Each rule in the implementation matrix should carry source, page or section, confidence (documented, inferred, estimated), and a validation case. No new instruction behavior is inferred from the Habr table when it disagrees with the book.

## Additional primary book added during implementation

[Kitov and Krinitsky, Electronic Computing Machines (1958)](../../docs/kitov_evm.pdf), PDF pp. 42-49, has a complete 16-command Strela program, its concrete octal listing, an input-card bootstrap, and a real result. The local scan supplied by the user is in docs/kitov_evm.pdf. PDF pp. 35-38 also describe card, tape, console, 253 physical tape zones, and a 100,000-word-per-tape limit. This book supplies an end-to-end program fixture and startup model. It does not resolve the 220-versus-500-microsecond timing labels. See docs/BOOKS.md.

## Existing implementation audit

The current C99 prototype builds static and shared libraries, a CLI runner, and one CTest target. It has 2048 RAM words, a hardwired zero cell, a partial constant table, arithmetic and logic opcodes, conditional transfers, grouped execution, card callbacks, sparse tape zones, and a cycle counter. The smoke test passes on the current Windows toolchain.

Known gaps that drive this feature:

- Basic arithmetic uses host double. It cannot implement opcode 12's no-rounding alignment faithfully and can misrepresent bit-level rounding, negative zero, and non-normalized inputs.
- Standard mathematical commands call host libm. Their value and timing are approximations, not the original ROM algorithms.
- Opcode 60 checked group transfer has no implementation.
- Card callbacks do not own a multi-card deck or persist position. Tape supply opcode 25 validates an address but schedules no motion.
- Tape images have no file format, total capacity enforcement, or provenance for physical timings.
- The cycle counter is a post-instruction total. There is no event scheduler, time-budget run, or resumable partial group operation.
- Normal stop, control stop, compare mismatch, arithmetic stop, and media failure are not all distinguished; stop operands are not exposed.
- Constant memory has only the listed entries, while reads of unlisted entries currently yield zero without an unknown-constant signal.
- No snapshot, operator console contract, or independent multi-program corpus exists.
- The public machine structure exposes storage and callback pointers, making persistent snapshots and ABI stability difficult.

## Decision 1: instruction-level system model

**Decision**: Advance at completed-instruction or resumable group-element boundaries. Keep memory, card, tape, control panel state, and a deterministic event queue in one system. Do not model tube voltage, RAM refresh, or individual 20-microsecond accesses.

**Rationale**: This matches the user's requested system emulator while retaining observable software behavior and basic clock timing.

**Alternative**: Circuit and pin-level model. Rejected because surviving sources do not define enough transition timing and it does not help run Strela programs.

## Decision 2: timebase and event semantics

**Decision**: Store simulated time as a 64-bit integer count of microseconds and count nominal instruction intervals separately from the 220-microsecond sourced main tact. Charge a completed basic command one cycle. Allow per-device delays and overlapping tape-supply events from a versioned timing profile. A time-budget run stops only at a safe instruction or group-element boundary and reports any overshoot.

**Rationale**: The museum profile reports 500 microseconds as clock time and 20 microseconds per RAM access; the Ershov–Shura-Bura table reports a 220-microsecond main tact and 30-microsecond memory access. Their exact relationship is unresolved, and neither gives a complete opcode timing table. Separate CPU occupancy and device latency avoids presenting transfer estimates as measured facts.

**Alternative**: Treat every instruction and transfer as one cycle. Rejected because group, standard-function, and I/O work then has unrealistically constant duration.

## Decision 3: arithmetic fidelity levels

**Decision**: Use integer mantissa and exponent operations for documented basic arithmetic and word operations. Implement the documented alignment rounding for opcode 01 and truncation for opcode 12. Use a portable two-limb product for 35-by-35-bit multiplication. Keep standard transcendental functions as explicitly labeled libm estimates until historical ROM algorithms or independent vectors are available.

**Rationale**: The book gives enough information to distinguish arithmetic instructions that host double currently conflates. Word-level determinism is needed for self-modifying programs.

**Alternative**: Keep all arithmetic as double. Rejected for opcode 12 and bitwise repeatability. Exact replication of all ROM subprograms is deferred because their full code is not available.

## Decision 4: constant and media profiles

**Decision**: Treat documented constants as a built-in profile, track validity of each constant address, and let an operator load additional constant values before execution with provenance. Expose card decks as ordered 12-word cards. Use two tape units with zones addressed 4001–4777 and 5001–5777, each holding 1–2048 words. The historical profile enforces the book's 100,000-word-per-tape total; another profile may relax that limit with a visible label.

**Rationale**: This reflects source evidence and makes unknown constants or missing media visible.

**Alternative**: Fill unlisted constants with zero and allow unlimited tape words. Rejected because it hides missing machine setup.

## Decision 5: API evolution

**Decision**: Introduce an opaque machine handle and accessor functions for state, media, control, and snapshots. Keep the existing include/strela.h source interface during migration where practical, then document removals before a stable release. Move card and tape data ownership behind the machine API.

**Rationale**: The present public struct exposes pointers that cannot be serialized portably and lets hosts bypass invariants.

**Alternative**: Continue extending the public struct. Rejected because every new device field changes ABI and creates inconsistent direct writes.

## Decision 6: program and snapshot formats

**Decision**: Use human-readable octal listings for interchange and debugging; define a strict canonical media representation with explicit card boundaries and tape zone headers. Use a versioned binary snapshot with magic, length, checksums, and little-endian fixed-width fields. Restore transactionally only after full validation.

**Rationale**: Listing text is easy to compare with the book. A snapshot needs lossless 43-bit words, media positions, and pending timed work.

**Alternative**: Serialize raw C structs. Rejected because pointer values, alignment, and endianness are not portable.

## Open evidence work

- Confirm full opcode 60 layouts, checksum placement, and source/destination combinations from the scanned table on printed pp. 128–134. The prose allows variants and some machines stored the sum after the array. Keep this a configurable format profile until the chosen Strela variant is resolved.
- Find a stronger primary source for card and tape transport latency. Until then, label configured values estimates.
- Extract worked numeric examples across exponent boundaries and non-normalized inputs. Do not claim all basic arithmetic exact until those cases pass.
- Check whether initial program loading uses a card-reader bootstrap procedure or should be treated as an operator shortcut. Keep the shortcut explicit.

