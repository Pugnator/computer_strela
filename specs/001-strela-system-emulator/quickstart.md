# Quickstart and validation guide

This guide describes implemented headless checks and the optional museum-informed terminal console.

## Current baseline

From the project root:

    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build --output-on-failure
    build/strela_run examples/hello.strela 0010 100 0100

Expected output includes a normal stop after two instructions, two simulated cycles, 1000 microseconds, and RAM 0100 containing the constant one. This is a smoke check, not completion of the feature.

## Gate A: instruction and number semantics

Run the book-derived case suite and a multi-branch sample program. The suite must cover every documented basic opcode, omega lifetime, zero cell, CDD validity, in-place operands, opcode 12 versus 01, exponent overflow and underflow, group repetitions, and control stops. Each case identifies its book page or museum table row. Inspect docs/OPCODES.md for any unimplemented row.

Expected: output words, branch target, stop reason, and cycle count agree with each fixture. No documented opcode silently acts as a no-op.

## Gate B: media

Create one input deck of two 12-word cards and two tape images. Run a program that reads 13 card words, writes a tape zone, reads the zone into a new RAM range, punches a card, and executes checked transfer 60. Compare all words in canonical octal form.

Expected: ordered round trip with no changed high bits; incorrect checksum yields the dedicated mismatch stop; empty zone and exhausted deck yield dedicated media stops. The original input files remain unchanged.

## Gate C: clock and operator controls

Start from a listing with a group command and tape supply. Run in small simulated-time budgets, inspect state after each pause, then resume. Toggle the control-stop switch and single-step across a marked command.

Expected: 2,000 nominal instruction intervals consume one simulated second under the default throughput-derived profile. Group repetitions and batch elements advance time independently. Tape-supply completion follows its configured estimated delay and can overlap permitted CPU work. Pausing never repeats a committed element. Stop operands and omega are visible.

## Gate D: persistence

Run the same deterministic program two ways: continuously and with a snapshot/restore halfway through a group or media operation. Compare final RAM, constants, card/tape positions and contents, stop reason, omega, PC, cycle count, and simulated microseconds.

Expected: exact match. A snapshot with a changed checksum or unsupported version is rejected and does not modify the loaded machine.

## Gate E: optional console

For the terminal view, build with -DSTRELA_BUILD_MONITOR=ON and follow tests/console-acceptance.md. For the graphical browser console, run web/build-wasm.ps1 and node web/test-wasm.mjs, then serve web/dist/ as described in web/README.md. The native development mode still uses -DSTRELA_BUILD_WEB_BRIDGE=ON and node web/server.mjs. Both browser backends compare snapshots byte for byte with the CLI.

Expected: the displayed word, instruction, omega, stop reason, and simulated time agree. Headless operation needs no graphical dependency.

## Completion report

A feature is complete when Gates A–D pass on the current Windows toolchain, all FR-001 through FR-014 are traced to acceptance evidence, docs/OPCODES.md has no documented opcode without a decision, and all remaining numeric or device estimates are named in docs/TIMING.md and docs/SOURCE-NOTES.md. Gate E is a separate optional deliverable after the headless system is complete.
