# Tasks: Strela system emulator

**Input**: [spec.md](spec.md), [plan.md](plan.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)
**Status**: All implementation tasks complete; checked tasks have recorded evidence.
**Tests**: Historical cases, media round trips, timing, and replay tests are required by the feature acceptance criteria.

## Format

Each task uses a checkbox, stable ID, optional parallel marker, story label, and exact project paths. [P] means work can proceed independently after its stated prerequisites.

## Phase 1: Baseline and evidence

- [X] T001 Create docs/OPCODES.md with one row for every documented opcode: operands, result, omega, stop behavior, timing, source page/URL, confidence, current support, and acceptance fixture; resolve duplicate or absent opcode entries against docs/ecm_i_prog.pdf and docs/SOURCE-NOTES.md.
- [X] T002 [P] Extract book-derived octal words and complete short listings into tests/fixtures/book/ with a provenance note for each fixture; include the section 28 bit-mask and control-addition examples.
- [X] T003 [P] Record the present public API and CLI behavior in docs/EMBEDDING.md and a baseline CTest run in specs/001-strela-system-emulator/baseline.md; identify signatures that need a migration adapter.

**Checkpoint**: The spec describes future work, while the baseline records what already runs.

## Phase 2: Shared foundation

- [X] T004 Add the planned opaque machine declarations and state/error accessors in include/strela/system.h; keep include/strela.h usable through a documented adapter in src/strela_compat.c.
- [X] T005 Split src/strela.c into src/machine.c, src/word.c, src/instructions.c, src/timing.c, and src/media.c, then update CMakeLists.txt without changing outputs verified by tests/test_strela.c.
- [X] T006 Implement a deterministic microsecond clock and ordered event queue in src/timing.c; expose the selected timing profile and estimate markers through include/strela/system.h.
- [X] T007 Add a source-fixture test helper in tests/fixture_runner.c and register focused CTest targets in CMakeLists.txt; each case must compare raw words, omega, PC, stop reason, and simulated time.

**Checkpoint**: Existing smoke tests still pass and the new state can be inspected through the opaque API.

## Phase 3: User Story 1 - Run and inspect a program (P1, MVP)

**Goal**: Program behavior, numerical results, branches, stops, and basic time are trustworthy.
**Independent Test**: Run the book-derived and multi-branch fixtures, inspect words and stop state, and repeat from the same initial image.

- [X] T008 [P] [US1] Add raw 43-bit number-format cases for zero, negative zero, normalization, exponent limits, and BCD validation in tests/test_word.c using documented examples from docs/ecm_i_prog.pdf.
- [X] T009 [US1] Implement sign-magnitude exponent alignment, normalization, opcode 01 rounding and opcode 12 truncation in src/word.c; make tests/test_word.c pass on raw-word expectations.
- [X] T010 [US1] Implement portable 35-by-35-bit mantissa multiplication and audit order operations 06/07 and sign transfer 10 in src/word.c and src/instructions.c; cover non-normalized inputs, overflow, underflow, and omega in tests/test_word.c.
- [X] T011 [P] [US1] Audit logic, shift, control addition, special address arithmetic, comparison, and conditional transfer in src/instructions.c against docs/OPCODES.md; add source-derived branch and self-modifying cases in tests/test_programs.c.
- [X] T012 [US1] Replace whole-group execution with a resumable group cursor in src/instructions.c and src/machine.c; test repeated address increments, unchanged stored command, overlap, and control stop during repetition in tests/test_programs.c.
- [X] T013 [US1] Separate normal, control, mismatch, address, opcode, arithmetic, unknown-constant, and domain stop records in src/machine.c; expose instruction and display operands through include/strela/system.h and verify in tests/test_programs.c.
- [X] T014 [US1] Validate every listed CDD entry and track unknown addresses in src/machine.c; support operator-supplied pre-run constants and test read-only behavior in tests/test_programs.c.
- [X] T015 [US1] Audit estimated standard-function opcodes in src/instructions.c: domain, batch order, cell 0002 side effects, raw-word conversion, and per-element timing; write representative range cases in tests/test_programs.c and declare remaining approximation in docs/OPCODES.md.
- [X] T016 [US1] Extend tools/strela_run.c to load strict octal listings and report PC, omega, stop cause, words, and microseconds; preserve the current positional invocation and add a multi-branch example under examples/programs/.

**Checkpoint**: Gate A in quickstart.md passes. This is the first deliverable that can run and diagnose non-I/O programs.

## Phase 4: User Story 2 - Card and tape media (P1)

**Goal**: Programs load and save ordered words through realistic card and tape abstractions.
**Independent Test**: Read across a card boundary, write and read both tapes, then force checksum and end-of-media stops.

- [X] T017 [P] [US2] Specify card deck and tape zone round-trip fixtures in tests/fixtures/media/ and source-based opcode 60 variants in docs/OPCODES.md before changing transfer behavior.
- [X] T018 [US2] Implement owned 12-word card decks, deck position, output card finalization, and end-of-deck status in src/media.c; replace the current one-shot callback assumption and verify in tests/test_media.c.
- [X] T019 [US2] Implement two sparse tape units with zone addresses, per-zone word counts, supplied-zone state, historical capacity profile, and transactional replacement in src/media.c; verify invalid and empty zones in tests/test_media.c.
- [X] T020 [US2] Implement opcode 60 checked group transfer across the documented RAM, card, and tape combinations in src/instructions.c and src/media.c; select and name the checksum-placement profile, then test both correct and deliberately bad checksums in tests/test_media.c.
- [X] T021 [US2] Add strict, transactional card-deck and tape-image parsers/writers in src/media.c and tools/strela_run.c following contracts/cli-and-images.md; verify a two-card and two-tape round trip in tests/test_media.c.
- [X] T022 [US2] Replace provisional per-word I/O charging with start, word, and movement fields in src/timing.c; make every estimate inspectable and update docs/TIMING.md with its source or estimated label.

**Checkpoint**: Gate B passes and no missing medium is interpreted as zero-filled data.

## Phase 5: User Story 3 - Operator controls and clock (P2)

**Goal**: An operator can step, pause, resume, inspect, edit, and run for a simulated-time budget.
**Independent Test**: Pause within a group, change a writable RAM word, resume, and inspect distinct stop operands and exact time.

- [X] T023 [P] [US3] Add control-action scenarios for start, pause, step, reset, check switch, and word edit in tests/test_operator.c.
- [X] T024 [US3] Implement the action/state machine and safe-boundary pause/resume in src/machine.c; reject edits while running and preserve the pending group or batch cursor.
- [X] T025 [US3] Implement run-for-microseconds and deterministic event delivery in src/timing.c and src/machine.c; model tape supply overlap without wall-clock sleeps and verify budget overshoot and event ordering in tests/test_timing.c.
- [X] T026 [US3] Report opcode 40 and 26 display words, last instruction location, next location, omega, switches, cycles, and microseconds in include/strela/system.h and tools/strela_run.c; verify in tests/test_operator.c.

**Checkpoint**: Gate C passes. The CLI is a complete headless operator interface.

## Phase 6: User Story 4 - Snapshots and replay (P2)

**Goal**: A saved session resumes with the same result as uninterrupted execution.
**Independent Test**: Compare continuous and restored runs halfway through a group and a media transfer.

- [X] T027 [P] [US4] Create continuous-versus-resumed and corrupt-snapshot fixtures in tests/test_snapshot.c, including pending events and media positions.
- [X] T028 [US4] Implement fixed-width versioned snapshot encode/decode with checksum and transactional restore in src/snapshot.c; finalize the binary magic and checksum in contracts/cli-and-images.md with a frozen fixture.
- [X] T029 [US4] Add run, resume, inspect, save-snapshot, and media output options to tools/strela_run.c; preserve existing positional usage and document exit statuses in contracts/cli-and-images.md.
- [X] T030 [US4] Verify deterministic replay and failed-restore immutability in tests/test_snapshot.c and register the CTest target in CMakeLists.txt.

**Checkpoint**: Gate D passes. The headless system feature is complete.

## Phase 7: User Story 5 - Optional historical console (P3)

**Goal**: A museum-informed view displays and controls the same machine state as the CLI.
**Independent Test**: Compare a shared sample in both interfaces at each step.

- [X] T031 [P] [US5] Draft the console control and memory-inspection layout in tools/strela_mon/README.md from the Polytechnic Museum's three surviving modules; cite the photographs and keep hardware appearance separate from execution rules.
- [X] T032 [US5] Implement an optional tools/strela_mon/ target in CMakeLists.txt that uses include/strela/system.h only; show RAM, current instruction, omega, stop operands, media, and simulated time.
- [X] T033 [US5] Add a shared CLI/view state comparison fixture or documented manual acceptance script in tests/console-acceptance.md.

**Checkpoint**: Gate E passes without making graphical dependencies mandatory for the core.

## Phase 8: Convergence

- [X] T034 Check every FR-001 through FR-014 against tests and docs/OPCODES.md; record remaining estimate classes and unresolved source issues in specs/001-strela-system-emulator/convergence.md.
- [X] T035 Update README.md, docs/EMBEDDING.md, docs/TIMING.md, and docs/SOURCE-NOTES.md with final commands, API migration, source choices, and timing profiles.
- [X] T036 Build and run CTest on the Windows baseline and at least one other available C99 toolchain; record commands and results in specs/001-strela-system-emulator/convergence.md.

## Dependencies

- T001–T003 precede feature implementation.
- T004–T007 are shared foundations. T008–T016 implement US1 after them.
- US2 needs the word and stop contracts from US1; its fixture research T017 can begin earlier.
- US3 needs the resumable cursor from T012 and media timing from T022.
- US4 needs stable machine, media, and pending-event state from US1–US3.
- US5 depends on the opaque API and operator controls, but can be deferred until after the headless release.
- T034–T036 follow completed headless work; repeat convergence after optional US5.

## Parallel opportunities

- T002 and T003 can proceed independently.
- T008 and T011 touch distinct numeric and control cases after the fixture helper exists.
- T017 media fixtures and T023 operator scenarios can be prepared while US1 implementation is underway.
- T027 snapshot cases and T031 visual layout can be drafted independently once the data model is stable.

## Implementation strategy

Deliver US1 as the first reviewable MVP. Then complete US2 for a usable system, US3 for clock-aware operation, and US4 for reproducibility. US5 is optional and must not delay headless correctness. Review the source and timing matrices at each checkpoint before calling that milestone complete.


## Phase 9: Convergence audit follow-up

The tag at the end of each task records the audit finding that prompted it, such as partial or missing. It describes the state before the task, not after.

- [X] T037 Audit binary arithmetic and BCD word conversion against documented examples; add raw zero, negative-zero, normalization, exponent-limit, invalid-BCD, multiplication, and omega fixtures in tests/test_word.c and tests/test_programs.c per FR-005 and SC-001 (partial).
- [X] T038 Add source-derived acceptance cases for every opcode row still lacking one, especially address-field arithmetic, shifts, omega lifetime, conditional branch, mismatch and domain stops; update docs/OPCODES.md with test names per FR-004 and SC-006 (partial).
- [X] T039 Give card, tape, checked, and RAM block transfers a pending cursor so time budgets, pause and snapshots can stop at committed word boundaries; test overlapping memory and partial media stops per plan: execution boundaries and FR-006 (partial).
- [X] T040 Enforce exact four/four/four/one/two octal field widths in card and tape image parsers and test noncanonical inputs leave mounted media unchanged per FR-012 (contradicts).
- [X] T041 Add step, edit, reset and check-switch operator options or subcommands to the headless CLI, with state parity tests against the opaque API per FR-009 and Gate C (partial).
- [X] T042 Complete the named legacy API adapter in src/strela_compat.c, verify every built-in CDD word against the local book, and add constant-validity and read-only tests per T004, T014 and FR-014 (partial).
- [X] T043 Build the separately optional museum-informed console using only the opaque API and a CLI/view comparison script per US5 and T031-T033 (missing).

## Phase 10: Local browser operator console

- [X] T044 Add a prepare CLI action that creates a paused snapshot from a strict listing before the first instruction in tools/strela_run.c.
- [X] T045 Add an optional JSON view executable using include/strela/system.h in tools/strela_view.c and CMakeLists.txt.
- [X] T046 Add a localhost session server in web/server.mjs with serialized operator actions, listing and media load, snapshot export/import, and core-backed state reads.
- [X] T047 Build the graphical control, instruction, memory, media and timing panels in web/public/ and document the local workflow in web/README.md.
- [X] T048 Compare a browser step snapshot byte for byte with the CLI in web/test.mjs; verify media mount, import, and reset; register the optional web_console CTest and run all 17 targets under GCC and Clang.

## Phase 11: Standalone static playground

- [X] T049 Extract a shared native/browser JSON state formatter in src/state_json.c and retain the strela_view output contract.
- [X] T050 Compile the C99 machine as an Emscripten ES module through web/strela_wasm.c and web/build-wasm.ps1, with browser-local snapshot and media access.
- [X] T051 Add web/public/wasm-api.mjs and automatic backend selection so the GUI runs from static files while the native development server remains usable.
- [X] T052 Compare native and WebAssembly initial, stepped, timed-run and media-mounted snapshots byte for byte in web/test-wasm.mjs, including restore and reset cases.
- [X] T053 Render and operate the static GUI in Chrome, confirm Step and Reset PC/time updates, and document build and hosting requirements in web/README.md.

## Phase 12: Consistency pass, 2026-10-12

- [X] T054 Correct the 1958 book to Kitov and Krinitsky, Electronic Computing Machines (Электронные вычислительные машины), in README.md, docs, specs, fixtures and web pages, and name the frozen snapshot's pending opcode as 72.
- [X] T055 Make strela_run inspect, run and resume report the recorded stop reason even when the saved machine is paused; add the cli_stop_snapshot and cli_inspect_reason CTest targets.
- [X] T056 Accept operator edits, constants, the check switch, media changes and reset while the machine is stopped, as data-model.md describes; leave reset paused; cover it in tests/test_operator.c and tests/test_snapshot.c.
- [X] T057 Record whether each CDD word is built in or operator-supplied, expose strela_machine_constant_source, store it in the snapshot source byte, and test it in tests/test_programs.c.
- [X] T058 Accept --check-stops with an optional 0 or 1 in strela_run run, matching operate.
- [X] T059 Reconcile contracts/library.md, contracts/cli-and-images.md and data-model.md with the built API, tape header and snapshot fields, and list what this release does not provide.
- [X] T060 Mark unsourced rules for shift cutoff, group-prefix third address and BCD exponent limit in code and docs; fix opcode matrix rows 45 and 72, the Carr source naming, zero handling notes, and the checked-transfer commit description.
- [X] T061 Remove stray arguments from three CTest commands; rebuild GCC and Clang trees and pass all 19 targets in each; confirm node web/test-wasm.mjs still matches the native CLI.

## Phase 13: Book programs and card file

- [X] T062 Inventory every page of docs/kitov_evm.pdf and docs/ecm_i_prog.pdf that contains Strela code; record included, symbolic and excluded material in examples/book/README.md per FR-019.
- [X] T063 Write runnable listings for the programs with printed or fully specified allocation in examples/book/; cross-check them against the book's symbolic programs and run them on independently computed inputs; record the table 36, quadratic and iteration corrections.
- [X] T064 Transcribe the remaining programs in the book's notation in examples/book/symbolic/, annotating suspected typos.
- [X] T065 Replace the two program buttons of the browser console with the card file (web/public/index.html, app.js, style.css), generated by web/generate-catalog.py; verify deal, sort, pull-out and reduced-motion behavior, keyboard focus and 375-pixel layout in a real browser per FR-020.
- [X] T066 Add the web_catalog and book_iteration CTest targets; pass all 21 targets under GCC and Clang.
