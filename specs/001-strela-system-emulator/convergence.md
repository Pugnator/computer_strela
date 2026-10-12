# Convergence audit, 2026-10-09

The headless emulator builds with GCC and Clang on Windows. This audit compares the current code to spec.md FR-001 through FR-014. The 1959 Kitov/Krinitsky scan is the primary instruction source; the 1958 Kitov and Krinitsky book supplies the end-to-end listing. Historical timing and device costs remain explicitly separated.

| Requirement | State | Evidence or remaining work |
| --- | --- | --- |
| FR-001 | Implemented | 2048 RAM words, zero cell, CDD validity; all 24 built-in constants compared with printed table 26, p. 141 in tests/test_programs.c. |
| FR-002 | Implemented | Raw 43-bit pack/unpack, strict octal listing and media fields. |
| FR-003 | Implemented with estimates | Every documented opcode in docs/OPCODES.md has a dispatch path; standard functions use host math. No unknown opcode silently succeeds. |
| FR-004 | Implemented | tests/test_opcodes.c covers address-field arithmetic, shift, omega lifetime, conditional/return branch, self-modifying code and all eight group modes; stop displays are checked in tests/test_operator.c. |
| FR-005 | Implemented with declared approximation | Raw integer arithmetic, zero/negative zero, normalization, exponent limits, BCD rejection and function domains have representative cases. Host math and historical last-bit rounding remain an explicitly declared approximate accuracy class. |
| FR-006 | Implemented with estimates | 220 us main tact kept distinct from a 500 us nominal interval; per-element group, batch and block timing, event ordering and budget overshoot tested. Media delays are adjustable engineering estimates. |
| FR-007 | Implemented | Owned 12-word cards, two sparse tape units, zone limits, capacity, strict text images, and cross-card read. |
| FR-008 | Implemented for selected variant | Checked transfer uses sum-before-data; good and bad checksums plus RAM/card/tape paths tested. The alternate historical placement is documented, not emulated. |
| FR-009 | Implemented | Pause, resume, single-step, reset, switch, edits and inspection through the opaque API and snapshot-based CLI operate command. |
| FR-010 | Implemented | Version 1 sectioned little-endian snapshot with FNV-1a-64, transactional restore, frozen Kitov fixture, group/batch/block/media/event replay. |
| FR-011 | Implemented | Opcode 40 and 26 display words, last and next locations are exposed and tested. |
| FR-012 | Implemented | Strict octal listings and card/tape images; malformed files leave mounted media unchanged. |
| FR-013 | Implemented, optional | The static browser GUI runs the same C core as WebAssembly; web/test-wasm.mjs compares snapshots with the CLI. The native server and terminal monitor remain optional development interfaces. |
| FR-014 | Implemented with open source limits | docs/OPCODES.md links every documented opcode to a CTest group; docs/TIMING.md, docs/BOOKS.md and docs/SOURCE-NOTES.md distinguish documented, inferred and estimated behavior. Exact ROM outputs and some device timings remain unknown. |
| FR-015–FR-018 | Implemented | web/public/ renders C core state through a static WebAssembly module or native development server. web/test-wasm.mjs verifies initial and stepped snapshot parity, media, restore and reset. |

## Test commands

    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build --output-on-failure

    cmake -S . -B build-clang -G Ninja -DCMAKE_C_COMPILER=clang
    cmake --build build-clang
    ctest --test-dir build-clang --output-on-failure

Both GCC and Clang builds compile with -Wall -Wextra -Werror. All 14 headless CTest targets passed under both GCC and Clang after the final opcode coverage pass. With the optional terminal console enabled, all 16 targets passed on Windows under both GCC and Clang. With the browser console enabled, all 17 targets passed on Windows under both GCC and Clang. The web_console target checks media mounting, snapshot import, reset, and byte-for-byte step parity. The standalone WebAssembly test independently matches initial, stepped, timed-run and media-mounted native snapshots; headless Chrome loaded the static module, exercised GUI Step and Reset controls, and verified the 390-pixel layout has no horizontal overflow. The native fallback also loaded in Chrome.

## Consistency pass, 2026-10-12

Tasks T054–T061 corrected the 1958 book attribution, made inspect report the recorded stop, allowed operator edits on a stopped machine, recorded constant provenance, and aligned the contracts and data model with the built API. Both the GCC tree in build/ and the Clang tree in build-clang/, configured with the terminal and browser consoles, pass 19 of 19 CTest targets. The prebuilt WebAssembly module in web/dist/ still matches the native CLI in node web/test-wasm.mjs. It must be rebuilt with Emscripten to read snapshots that contain operator-supplied constants.

## Remaining evidence limits

- The [Ershov archive](https://ershov.iis.nsk.su/ru/foundation/index) labels 220 us a main tact; the [Computer Museum profile](https://www.computer-museum.ru/english/strela.htm) gives 500 us clock time and 2,000 commands/s. The mapping from tacts to opcodes is unknown. We use 500 us as a throughput-derived nominal interval and expose 220 us separately.
- Card start 10,000 us, card word 1,000 us, tape start 20,000 us, tape word 500 us, and supply movement 50,000 us are engineering estimates, not measured Strela values.
- The 1959 book gives arithmetic and ROM-function behavior without enough independent raw output words to prove every last bit. Basic arithmetic is integer-based; standard functions use host libm and conversion. The accuracy class is functional/approximate, not bit exact.
- The 1958 book describes 253 physical tape zones, while the 1959 programming model addresses 511 zone numbers per tape. The emulator implements the addressable model with a 100,000-word capacity limit.
- Opcode 60 uses the sum-before-data variant described in docs/OPCODES.md. The historical sum-after-data variant remains unimplemented.
- The shift cutoff above 37, the zero written to a group prefix's third address, and the BCD exponent limit of ±19 are unverified implementation rules; see docs/SOURCE-NOTES.md.
- The opcode matrix has an acceptance reference for every documented row. The separately optional historical console is implemented as a terminal view and covered by T031-T033/T043. Exact ROM and hardware phase details remain source limits rather than claims of precision.


