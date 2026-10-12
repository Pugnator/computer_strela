# Data model

## Word and instruction

**Word**: unsigned 43-bit payload stored in a 64-bit container, with upper 21 bits zero. All instruction, numeric, and media views refer to this raw payload.

**Instruction view**: first, second, and third 12-bit addresses; one check bit; one 6-bit opcode. Each field has its documented bit position. A decoder never changes the raw word.

**Binary numeric view**: mantissa sign, 35 fractional mantissa bits, exponent sign, six exponent magnitude bits. The full raw payload is retained, including negative zero or non-normalized patterns.

**Decimal numeric view**: mantissa sign, nine four-bit decimal digits, exponent sign, five exponent magnitude bits. Opcode 72 stops with the numeric domain stop for either an invalid digit or an exponent beyond ±19. The ±19 limit is inferred from the binary range, not sourced.

Validation: writes mask to 43 bits; octal text must have exactly valid digits and field widths; numeric conversion never destroys the original raw word unless an opcode writes a result.

## Machine

Owns RAM[2048], constant words[256] plus validity/source metadata, current instruction location, next instruction location, omega, control-stop switch, status, stop record, basic-cycle counter, simulated microseconds, and pending operation.

Machine status transitions:

```text
stopped -- start --> running -- pause/budget --> paused
running -- stop opcode/control check/error --> stopped
paused -- step/start --> running or stopped
stopped/paused -- reset --> paused at the selected start address
```

A stop record contains cause, faulting instruction address, next address, displayed operand words if applicable, and optional detail such as bad address or checksum pair. Resume continues after a control stop. Other stops need reset. Operator edits, constant loads, the check switch, media changes and reset are accepted while paused or stopped and rejected while running.

## Pending operation

Represents a group, batch function, block transfer, or device wait after the initial command has been fetched. Fields: original instruction word and address, current element index, effective address fields, source snapshot if overlap requires it, next event time, and committed element count. The stored instruction word remains unchanged. A snapshot includes this record.

Validation: element index is within declared count; effective addresses remain 12-bit; restoring a pending operation cannot repeat committed output.

## Timing profile and event

**Timing profile**: sourced main tact = 220 microseconds, nominal instruction interval = 500 microseconds by default (throughput-derived estimate), opcode or standard-function cycle costs, card start and word costs, tape supply and per-word costs, and confidence/source for each nonbasic value. The profile has no name or version field. A snapshot stores every timing value, and estimate flags mark the instruction, card and tape values as estimates.

**Event**: deadline in simulated microseconds, kind, unit/zone, sequence number for deterministic ordering, and operation reference. Ties resolve by sequence number. Pending events survive snapshots.

Validation: no negative duration; addition cannot overflow the 64-bit time counter; run budgets end on a safe boundary and report actual consumed time.

## Card deck

Ordered cards, each with exactly 12 raw words, plus read position and punched output position. A final partially filled output card may exist while punching. It is saved as CARD-PARTIAL unless finalize first pads it with zero words. Input and output decks are separate entities.

Validation: a read beyond the deck returns end-of-deck; it does not fabricate a word. File parsing rejects a malformed card without changing the mounted deck.

## Tape unit and zone

Two units. Unit one addresses 4001–4777; unit two addresses 5001–5777. Each addressed zone contains 1–2048 raw words. Checksums exist only as data words used by opcode 60; zones carry no checksum metadata. A unit also has its supplied zone and any pending supply event. The only implemented capacity profile limits total recorded words per tape to 100,000, from the 1958 book, and is named HISTORICAL-100000 in tape images. A relaxed profile is not implemented.

Validation: address 4000/5000 and other gaps are invalid zones. A read may request only a recorded prefix. Replacing a zone updates capacity atomically.

## Constant profile

The built-in profile contains documented CDD words and marks all other CDD addresses unknown. Additional constants may be loaded before the first instruction or after reset. The API reports each address as built-in, operator-supplied or unknown; no free-text source is stored. Program writes to CDD addresses fail with a bad-address stop.

## Media image

Portable interchange representation with format version, machine word order, tape unit/zone addresses or card boundaries, raw octal words, and optional human comments. The parser is strict; on any error the old mounted media remains intact. A writer emits canonical order.

## Snapshot

Versioned binary record with magic, fixed little-endian definition, payload length, checksum, timing profile values, RAM, constants and validity, media contents and positions, pending operation, event queue, stop record, and simulated time. Host callback pointers and process addresses are excluded.

Validation: read the full snapshot into temporary state, validate counts, addresses, lengths, version, and checksum, then atomically replace the machine. Failed restore leaves the current machine unchanged.

## Evidence record

A documentation row links an opcode or timing rule to a book page or URL, confidence (documented/inferred/estimated), currently chosen interpretation, and one acceptance fixture. This record is maintained in docs/OPCODES.md and docs/TIMING.md rather than serialized into every machine.
