# Implementation Plan: Strela system emulator

**Feature**: 001-strela-system-emulator | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)
**Input**: Complete the existing instruction-stepped prototype as a usable Strela system emulator.

## Summary

Build from the existing C99 prototype. First make word, numeric, branch, and stop behavior reliable. Then add owned card/tape media and checked transfers, a documented timing scheduler, operator controls, and portable snapshots. Keep an optional historical console view last. Historical facts, interpretations, and estimates remain distinguishable in code and documentation.

## Technical Context

**Language/Version**: C99; optional C++ only for a future visual console
**Primary Dependencies**: C standard library and libm; CMake for build; no runtime dependency in the core
**Storage**: 2048 x 43-bit RAM words, 256 constant-address slots with validity, two sparse tape units, card decks, and versioned on-disk images/snapshots
**Testing**: CTest with C unit and program-level integration executables; source-derived fixtures
**Target Platform**: Windows first, portable C99 library on other desktop systems
**Project Type**: embeddable system library plus CLI, optional monitor
**Performance Goals**: simulated time is exact according to the selected profile; host execution should remain comfortably faster than 2,000 basic cycles/s
**Constraints**: 220 microsecond sourced main tact and a separately configurable nominal instruction interval; no circuit simulation; no silent zero for unknown constants or absent media; deterministic replay
**Scale/Scope**: 2048 RAM words, up to 511 addressed zones per tape, up to 2048 words per zone, 12 words per card

## Constitution Check

| Principle | Plan response | Gate |
| --- | --- | --- |
| System behavior | Memory, media, timing, controls, snapshots all have an observable contract | Pass |
| Evidence | research.md records sources and uncertainty; opcode matrix is a first implementation task | Pass |
| Determinism | Integer word arithmetic, integer simulated time, owned media, and transactional snapshots | Pass |
| Embeddable core | Pure C99 core; UI remains optional and calls the same API | Pass |
| Verification | Book-derived word cases, program runs, media round trips, time and resume checks | Pass |

Recheck after design: the data model and contracts below preserve all five gates. No exception is requested.

## Architecture

The core owns a machine and its media. A host sends operator actions and asks for one step or a simulated-time budget. The core decodes instructions and schedules any media completion against integer simulated time. The CLI handles text/binary files and displays state. A later monitor consumes the same API.

```text
CLI / optional monitor
  | load image, issue control action, inspect state
opaque Strela machine API
  | instruction engine -- numeric word engine
  | deterministic time scheduler
  | RAM + constant profile + card deck + two tape units
  | snapshot encoder/decoder
```

### Execution boundaries

- A normal basic instruction commits at a safe boundary and advances by the selected nominal instruction interval; the 220-microsecond main tact remains a distinct profile field.
- A group prefix creates a pending group cursor; each repetition is a schedulable, resumable element. The stored successor word remains unchanged.
- Batch standard functions and block transfers create a pending operation cursor when they can exceed a run budget or stop partway. Results commit in documented order.
- Omega is consumed by the immediately following conditional transfer. Other operations reset it according to the source table.
- Stop events capture cause, instruction location, next location, and display operands. A paused machine can resume without repeating committed work.
- A completed media request advances simulated time by the selected profile and may overlap with a tape-supply command where the source permits it.

### Arithmetic

- Keep raw 43-bit words as the source of truth.
- Extract sign, 35-bit mantissa, exponent sign, and six exponent bits without passing through double for basic arithmetic.
- Implement exponent alignment with a guard bit and the book's opcode-specific rounding or truncation rules. Normalize after add/subtract; apply overflow and underflow rules.
- Multiply two 35-bit mantissas using portable partial products, then normalize and round according to documented behavior.
- Keep libm only behind the standard-function estimate layer. Validate range and domain before converting back to a machine word.
- Track negative zero and non-normalized values explicitly; specify each opcode's treatment in the opcode matrix.

### Timing

- Preserve cycle count and add monotonic simulated microseconds. The default profile records the 220-microsecond main tact and maps one nominal instruction interval to a throughput-derived 500 microseconds.
- Keep operation timing in a table with source and confidence. For ROM-backed functions use documented approximate counts from research.md; make batch cost per element.
- Make card and tape start, per-word, and movement delays profile fields. Until primary evidence is found, default estimates remain clearly named and inspectable.
- Add a deterministic event queue for tape supply and transfer completion. No wall-clock sleep occurs in the core.
- Compare workloads against the museum's 2,000 basic operations/s and 100 million operations in 17 hours only as plausibility checks.

## Project Structure

### Spec Kit feature artifacts

```text
specs/001-strela-system-emulator/
  spec.md
  plan.md
  research.md
  data-model.md
  quickstart.md
  contracts/library.md
  contracts/cli-and-images.md
  checklists/requirements.md
  tasks.md
```

### Source layout (as built)

```text
include/
  strela/system.h          opaque machine API, primary
  strela.h                 legacy visible-struct API, kept for compatibility
src/
  machine.c                legacy state, stepping, stops, built-in constants
  word.c                   43-bit encoding and arithmetic
  instructions.c           opcode dispatch, group cursor, standard functions
  blocks.c                 card, tape, RAM and checked block transfers
  timing.c                 timing charges and the tape-supply event queue
  media.c                  cards, tapes and their text images
  system.c                 opaque API and operator actions
  snapshot.c               versioned persistence
  state_json.c             JSON state for strela_view and the browser
  strela_compat.c          legacy strela_step adapter
tools/
  strela_run.c             CLI: run, resume, inspect, operate, prepare
  strela_view.c            optional JSON state bridge
  strela_mon/              optional terminal operator console
tests/
  test_strela.c            retained smoke cases
  test_word.c              word and arithmetic cases
  test_opcodes.c           per-opcode, omega and group cases
  test_programs.c          programs, constants and stop reasons
  test_media.c             deck, tape and checksum cases
  test_timing.c            timing profile and event cases
  test_operator.c          operator actions and stop displays
  test_snapshot.c          continuity and corruption cases
  fixture_runner.c         book listing fixtures
  fixtures/                book listings and the frozen snapshot
examples/
  hello.strela
  programs/                runnable examples
web/                       optional browser console and WebAssembly build
docs/
  SOURCE-NOTES.md, TIMING.md, OPCODES.md, BOOKS.md, EMBEDDING.md
```

**Structure decision**: Evolve the current single C project incrementally. Do not duplicate the engine in a UI or replace the CMake build.

## Delivery sequence

1. **Baseline and evidence**: catalogue every documented opcode and the current prototype behavior; extract reference cases from book pages; freeze initial API and listing compatibility.
2. **Reliable core**: split word logic, implement exact basic arithmetic where documented, audit omega and group behavior, expose distinct stop reasons. Gate: a multi-branch program and book examples pass.
3. **System media**: implement owned card decks and tape images, checked transfer 60, missing-media and checksum stops, and canonical image files. Gate: multi-card and two-tape round trip passes.
4. **Clock and operator**: introduce integer time scheduler, configurable device timings, tape supply overlap, run-for-budget, and operator controls. Gate: time and pause/resume scenarios pass.
5. **Persistence**: implement versioned snapshots and deterministic replay. Gate: interrupted and uninterrupted runs produce identical state and time.
6. **Optional view and convergence**: add a museum-informed console view; run all fixtures and document remaining evidence limits. Gate: view and CLI report the same state.

## Risks and decisions held open

- Opcode 60 checksum placement has source ambiguity. Research it before coding; select a named machine variant and keep the alternative as a profile.
- The 220-microsecond main tact and 500-microsecond nominal instruction interval do not establish every opcode or device delay. Estimates must remain configurable and visible.
- Complete bit-exact ROM function replication may be impossible without program listings. The release may retain declared mathematical approximation for these opcodes; FR-003 still requires the opcode to execute.
- Changing the public C struct can break embedders. Add accessors and a documented transition before hiding internal state.
- The final model may need a bootstrap image or operator load path for authentic card startup. Preserve the current octal loader as a clearly labeled convenience path.

## Complexity Tracking

| Complexity | Why needed | Simpler option rejected because |
| --- | --- | --- |
| Resumable operation cursor | Allows pause, control stop, snapshots, and timed multiword operations without replay | One-call loops cannot resume halfway |
| Small event queue | Tape supply can overlap execution and media latency has an independent timeline | Adding all delays after an instruction misstates overlap |
| Opaque API migration | Snapshot safety and media ownership | Growing a public struct freezes pointer layout into ABI |

## Local browser console extension

The graphical console uses a localhost Node.js server and the existing C99
emulator. A new CLI prepare action creates a paused snapshot before any
instruction executes. The optional strela_view executable reads the opaque
system API and emits JSON state. The server owns per-session snapshots,
serializes operator actions, and calls strela_run operate/resume to mutate
them. The browser displays state and submits actions only. This keeps the
documented 220 us tact and estimated instruction/media costs in one core.
The web interface is an optional local target; public static hosting would
require a later WebAssembly or server deployment design.

Acceptance: load the bundled two-branch listing, step once in the browser,
and compare the exported snapshot byte for byte with a CLI step from the
initial snapshot. Check import and reset. Keep all headless tests passing.
## Standalone WebAssembly extension

Build the existing C99 sources with Emscripten as an ES module and WebAssembly
binary. Extract the native JSON state formatter into src/state_json.c so both
backends expose the same fields. A narrow web/strela_wasm.c wrapper offers
machine creation, word load, media mount, operator actions, state inspection,
and the existing snapshot format. The browser adapter parses strict listing
fields and calls the C core; it does not execute instructions in JavaScript.
The GUI selects the WebAssembly module when present and retains the native
localhost server as a development fallback.

The build writes a static web/dist/ directory. Gate: initial and stepped
WebAssembly snapshots match the native CLI byte for byte; a real browser loads
the static page and its Step/Reset controls update PC and simulated time.

## Book programs and card file extension

Extract every program from docs/kitov_evm.pdf and docs/ecm_i_prog.pdf into
examples/book. Programs with a printed or fully specified allocation become
listings; their words are cross-checked against the book's symbolic form and
run on independently computed inputs. Programs whose allocation the book
leaves open are transcribed in the book's notation. web/generate-catalog.py
builds Russian copies with labelled example inputs and web/public/catalog.json;
the browser console shows them as decks in a card file with deal, sort and
pull-out animations, reduced to short fades when the system asks for less motion.
