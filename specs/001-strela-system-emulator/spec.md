# Feature Specification: Strela system emulator

**Feature Directory**: 001-strela-system-emulator
**Created**: 2026-10-08
**Status**: Implemented. Evidence and remaining source limits are in [convergence.md](convergence.md).
**Input**: Complete the existing Strela emulator as an instruction-stepped system emulator with credible clock timing, media, and operator controls.

## User Scenarios & Testing

### User Story 1 - Run and inspect a Strela program (Priority: P1)

A researcher loads an octal program and data, chooses its start address, runs or steps it, and inspects the resulting words, stop reason, and simulated elapsed time.

**Why this priority**: Every other feature depends on correct program execution and a trustworthy timebase.

**Independent Test**: Run a short listing made from worked book examples and compare memory, branch choice, stop reason, and cycle count with expected values.

**Acceptance Scenarios**:

1. **Given** a program using RAM, constants, arithmetic, comparison, and a conditional transfer, **when** it runs, **then** the destination words and selected branch match the documented rules.
2. **Given** a control-stop-marked instruction and an enabled control-stop switch, **when** that instruction completes, **then** execution stops with a distinct control-stop reason and retains an inspectable next address.
3. **Given** an invalid address, unsupported instruction, overflow, or invalid mathematical argument, **when** it is encountered, **then** the machine stops with a specific reason and does not report normal completion.
4. **Given** the same program and initial state twice, **when** each run completes, **then** words, stop reason, and simulated time are identical.

### User Story 2 - Operate card and tape media (Priority: P1)

An operator mounts a card deck and two tape images, runs a program that transfers blocks, and saves the resulting card and tape output. Media operations follow Strela word ordering and zone rules.

**Why this priority**: Strela software was loaded and saved through media; a system emulator cannot rely only on preloaded RAM.

**Independent Test**: Load a deck with multiple 12-word cards, transfer words to RAM and tape, restart from the saved media, and compare all 43-bit words and checksums.

**Acceptance Scenarios**:

1. **Given** a card deck with two cards, **when** a card-read command requests 13 words, **then** the first 12 come from the first card and the thirteenth from the next card in order.
2. **Given** a mounted tape zone, **when** a tape-to-RAM command requests a permitted prefix, **then** the words appear in RAM in the same order and the simulated time advances.
3. **Given** a checked group transfer with a deliberately incorrect checksum, **when** it executes, **then** the machine stops and reports the mismatch.
4. **Given** missing or exhausted media, **when** a transfer needs more words, **then** the machine stops without silently inserting zeroes.

### User Story 3 - Control and observe the machine (Priority: P2)

An operator starts, pauses, steps, resets, examines or changes a RAM word while stopped, and sees the current instruction, omega sign, displayed words, and stop reason. The controls expose documented behavior without requiring a visual hardware simulation.

**Why this priority**: The console is the practical way to diagnose historical programs and reflects surviving Strela controls.

**Independent Test**: Step through a branching program, edit one word while stopped, resume, and observe the branch and displayed stop operands.

**Acceptance Scenarios**:

1. **Given** a stopped machine, **when** the operator enters a 43-bit word into writable RAM, **then** a later read returns that word; writes to zero cell or constant memory follow their documented protection rules.
2. **Given** stop opcode 40 or mismatch-stop opcode 26, **when** it executes, **then** the operands named by the command are available for display.
3. **Given** the operator requests a run for a simulated-time budget, **when** the budget is reached, **then** the machine pauses on a completed instruction boundary with the exact consumed time reported.

### User Story 4 - Resume a reproducible session (Priority: P2)

A researcher saves the machine state and media position, closes the emulator, then restores and continues with the same result as an uninterrupted run.

**Why this priority**: Historical programs can run for long simulated periods and need repeatable investigation.

**Independent Test**: Compare a continuous run with a run saved and restored halfway through a group or media operation.

**Acceptance Scenarios**:

1. **Given** a running program, **when** a snapshot is saved and restored, **then** memory, constants, media, control state, stop state, and simulated time agree.
2. **Given** an incompatible or corrupt snapshot, **when** it is opened, **then** it is rejected without partially changing the current machine.
3. **Given** two runs from the same snapshot and media, **when** they execute with the same operator actions, **then** their final states agree.

### User Story 5 - Explore the historical machine (Priority: P3)

A user can optionally use a console view informed by museum photographs to browse RAM and control execution. The visual view reports the same state as the command-line interface.

**Why this priority**: It makes the project approachable without changing the execution contract.

**Independent Test**: Run the same sample in the CLI and view, then compare displayed state and simulated time.

**Acceptance Scenarios**:

1. **Given** a loaded program, **when** the user steps in the view, **then** the displayed program counter, word, omega sign, stop reason, and time match the core.
2. **Given** no graphical environment, **when** the CLI is used, **then** all execution and media features remain available.

### Edge Cases

- Word writes must mask values to 43 bits; address fields remain 12 bits.
- RAM address 0000 always reads zero, including after an attempted write.
- In-place and overlapping group transfers preserve the source values required by the chosen documented semantics.
- An omega-producing command followed by another command must follow the documented one-instruction lifetime of omega.
- Exponent overflow stops; exponent underflow yields machine zero according to the book. Negative zero and non-normalized operands require explicit rules.
- Group commands must not modify their stored successor instruction when incrementing address fields.
- A control stop inside a repeated group operation must preserve enough state to resume correctly.
- Tape-zone addresses outside 4001–4777 and 5001–5777, empty zones, card end-of-deck, and partial transfers require explicit status.
- Standard-function batch commands may overlap source and destination ranges and may stop partway through a batch.

## Requirements

### Functional Requirements

- **FR-001**: The system MUST represent 2048 RAM cells of 43 bits, a fixed zero cell, and the documented constant-address range.
- **FR-002**: The system MUST decode the three-address instruction word, check bit, and octal opcode without losing any bit.
- **FR-003**: The system MUST execute every documented instruction in the architecture article or report a named unsupported state while its rule is unresolved; release completion requires no documented opcode left unsupported.
- **FR-004**: The system MUST reproduce the documented omega conditions, branch choices, address-field operations, control stops, and normal stops.
- **FR-005**: The system MUST reproduce documented binary and decimal number formats, normalization, exceptional stops, and zero behavior. Any remaining numerical approximation MUST be identified by operation and accuracy class.
- **FR-006**: The system MUST track simulated time independently of host wall time, recording the sourced 220 microsecond main tact separately from a throughput-derived 500 microsecond nominal instruction interval, with documented or explicitly labeled estimated costs for longer operations.
- **FR-007**: The system MUST support card decks with 12 words per card, two tape units with addressable zones, and deterministic word transfers to and from RAM.
- **FR-008**: The system MUST support checked group transfer and expose checksum mismatch separately from other stop reasons.
- **FR-009**: The system MUST allow operator start, pause, step, reset, memory inspection and editing while stopped, and control-stop switching.
- **FR-010**: The system MUST provide a portable, versioned snapshot that restores all state required for deterministic continuation.
- **FR-011**: The system MUST expose normal stop operands and mismatch operands to an operator interface.
- **FR-012**: The system MUST accept and emit a documented octal listing or image format without losing any 43-bit word.
- **FR-013**: A graphical console, if supplied, MUST use the same machine state and behavior as the core and remain optional.
- **FR-014**: Each historical rule or estimate used by the implementation MUST have a recorded source and confidence level.
- **FR-015**: The optional browser console MUST show the core's PC, instruction, omega, stop cause and operands, RAM, media state, and simulated time without executing machine instructions in browser code.
- **FR-016**: The browser console MUST start a new listing before its first instruction and support portable snapshot download and restore.
- **FR-017**: Browser operator actions MUST produce the same snapshot as equivalent CLI actions.
- **FR-018**: The optional static playground MUST run the C emulator inside the browser without a native API server.

### Key Entities

- **Machine**: RAM, constants, current instruction location, omega sign, control switches, stop reason, simulated time, and pending multiword operation.
- **Word**: A 43-bit value interpreted as an instruction, binary number, decimal number, or opaque data.
- **Card deck**: Ordered cards of 12 words plus a current read or punch position.
- **Tape unit**: Mounted image of addressable zones, their word sequences, current supplied zone, and movement state.
- **Timing profile**: Basic cycle and per-operation or per-device durations with source and confidence.
- **Snapshot**: Versioned record of all state and media positions needed to resume.
- **Evidence record**: A rule, source reference, chosen interpretation, and confidence level.

## Success Criteria

### Measurable Outcomes

- **SC-001**: All documented non-device opcodes pass representative book-derived cases for output words, omega, and stop behavior.
- **SC-002**: The sample program and a second multi-branch program can be loaded, run, stepped, and inspected without editing source code.
- **SC-003**: Under the default throughput-derived profile, 2,000 nominal instruction intervals equal one simulated second; group and batch operations consume their specified additional cycles.
- **SC-004**: A two-card and two-tape round trip reproduces every 43-bit word in order, including a checked transfer.
- **SC-005**: A resumed run produces the same final words, stop reason, media positions, and simulated time as an uninterrupted run.
- **SC-006**: Every documented opcode and modeled device action has an acceptance case or an explicit unresolved evidence entry.
- **SC-007**: The CLI and optional view report the same machine state for a shared sample run.
- **SC-008**: A browser step from a fresh sample session yields a snapshot byte-for-byte identical to the CLI step; snapshot import and reset preserve the expected state.
- **SC-009**: A static page loads the WebAssembly machine and Step and Reset change the displayed PC and simulated time in a real browser.

## Assumptions

- The target is an instruction-stepped system emulator, not an electrical or memory-refresh simulation.
- The 1959 Kitov/Krinitsky book and Russian Virtual Computer Museum architecture article govern machine-code behavior. Secondary articles and museum exhibits inform operation and appearance when consistent.
- The existing C library and CLI are a prototype baseline. This feature may revise their internals, while preserving a practical migration path for the public header.
- The Ershov-Shura-Bura table gives a 220 microsecond main tact; the Computer Museum gives 500 microseconds as clock time and 2,000 commands/s. The exact mapping of tacts to opcodes is unresolved. RAM access timing is not exposed as an independent instruction clock.
- The first deliverable remains headless. The optional museum-inspired view follows functional console behavior; the browser version can run the same C core as WebAssembly in a static site, with the native bridge retained for local development.
- Where sources disagree or omit a rule, the implementation chooses a documented profile or stops explicitly until evidence supports a default.
