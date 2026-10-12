# Strela Constitution

## Core Principles

### I. System behavior is the product
The project models a usable Strela computer: execution, memory, media, operator controls, and simulated time. Instruction stepping is the minimum scheduling unit. Transistor, valve, and signal edge simulation are outside scope. Every emulated component must have a documented observable behavior.

### II. Historical evidence is traceable
Each architecture, opcode, numeric, or timing rule must cite a page of the local 1959 book or a direct source URL. Distinguish documented behavior, inferred behavior, and estimates in specifications and code comments. Contradictory secondary sources must be recorded with the chosen interpretation. Unknown behavior must produce an explicit status or configurable estimate rather than silently succeeding.

### III. Determinism before visual fidelity
Given the same initial state, media, operator actions, and timing profile, execution must produce the same words, stop reason, and simulated time. Console appearance may draw on museum photographs, but the core must remain usable without a graphical interface.

### IV. Small embeddable core
Keep the machine in portable C99 with a stable public C interface. Separate instruction semantics, memory/media behavior, and host presentation so CLI and future front ends can use the same engine. Do not add a dependency to the core to implement a front end or a device.

### V. Evidence based verification
Use worked examples from the book and independent historical descriptions where possible. Test observable state transitions, numeric boundary behavior, media transfer, and time accounting. A test must verify a historical rule or a meaningful integration path; avoid tests that only repeat an implementation expression.

## Scope and quality gates

- CMake must build the static library, shared library, and CLI on the current Windows toolchain. The core should remain compatible with other C99 toolchains.
- Numeric results must state their accuracy class. Host floating point approximations cannot be described as machine exact.
- Simulated time is independent of host wall time. Device latency assumptions are separately configurable and documented.
- Unsupported or uncertain opcodes and media operations stop with an inspectable reason. Errors must never look like successful completion.
- Source documents and Spec Kit artifacts live in the repository. The book scan remains an input reference; derived text dumps and build outputs are excluded.

## Development workflow

For each bounded slice, update the feature specification, plan, and tasks before implementation. Check the constitution against each plan. Mark tasks complete only after the stated acceptance check passes. Preserve the current public API unless a plan explicitly describes migration. Record new source evidence in docs/SOURCE-NOTES.md and update timing assumptions in docs/TIMING.md.

## Governance

This constitution governs planning and implementation in this workspace. Amendments require an explanation in the affected plan and a version increment: major for a principle change, minor for a new principle or gate, patch for clarification. The user may override a project rule for a specific task.

**Version**: 1.0.0 | **Ratified**: 2026-10-08 | **Last Amended**: 2026-10-08
