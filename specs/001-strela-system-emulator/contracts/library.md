# C library contract

This is the contract of the opaque API in include/strela/system.h as built. The older include/strela.h remains available; see docs/EMBEDDING.md. Items the original plan named but this release does not provide are listed at the end.

## Lifecycle and ownership

- strela_machine_create(profile) returns a paused machine with the built-in constant profile and the given timing profile. NULL selects strela_timing_default(). It returns NULL, with nothing allocated, if the main tact or nominal instruction interval is zero or memory is exhausted.
- strela_machine_destroy releases RAM, tapes, cards and pending state.
- strela_machine_reset(machine, start) clears execution state, counters, simulated time, events, the card read position and punched output. It keeps RAM, constants and mounted media, and leaves the machine paused at start.
- A machine is used by one caller at a time. The core has no global mutable state.

## Operator state

A machine is idle while it is paused or after it has stopped. Word edits, constant loads, PC changes, the control-stop switch, reset, and media mounts or saves are accepted only while idle. After strela_machine_run_for_us returns because the budget was reached, the machine is still running; call strela_machine_pause before editing.

## Word access

- strela_machine_read_word returns false for addresses outside RAM and CDD, and for CDD addresses without a known value.
- strela_machine_write_word accepts RAM addresses only and masks to 43 bits. A write to 0000 is accepted and ignored. CDD addresses are rejected; use strela_machine_set_constant.
- strela_machine_set_constant is accepted before the first instruction or after reset. The address becomes readable and its source becomes supplied.
- strela_machine_constant_source reports unknown, built-in or supplied for an address.
- Bulk loading is repeated strela_machine_write_word. The CLI validates a whole listing before writing any word.

## Execution and time

- strela_machine_step advances one safe boundary: one basic command, one group repetition, one standard-function element, or one block-transfer word. From a paused machine it stays paused unless a stop occurs.
- strela_machine_run_for_us(budget) runs a resumed machine until a stop or until simulated time since the call reaches the budget. It ends on a completed boundary and may overshoot by one element. It returns budget, normal stop or fault. A paused machine returns paused without running. The consumed time is the change in elapsed_us.
- strela_machine_read_state returns PC, last instruction word and location, omega, instruction and interval counts, simulated microseconds, timing values and estimate flags, the control-stop switch, group, batch and block counts, supplied tape zones, event counts, status and the underlying stop reason.
- strela_machine_pause takes effect between calls, which are synchronous. strela_machine_resume clears a control stop and continues from the next boundary, keeping any pending cursor.
- display_a and display_b hold the two words of opcode 40 or 26. After a checksum mismatch in opcode 60 they hold the recorded and calculated sums.
- Simulated time is independent of wall-clock speed. Run calls do not sleep.

## Media

- strela_machine_mount_cards copies a deck whose length is a multiple of 12. strela_machine_load_cards and strela_machine_save_punched read and write card images. strela_machine_finalize_punched pads the last punched card with zero words. A failed mount or load leaves the old deck unchanged.
- strela_machine_card_position, strela_machine_punched_words and strela_machine_read_punched expose the deck positions and output.
- strela_machine_mount_zone, strela_machine_load_tape and strela_machine_save_tape replace or write tape zones. strela_machine_zone_length and strela_machine_read_zone_word read them. The supplied zone of each unit is in the machine state.
- Reading beyond the deck gives the media-exhausted stop. A missing zone gives the media-absent stop.
- Transfers commit word by word. Unchecked card read 41 keeps the words it read before the deck ran out. Opcode 60 writes RAM destinations as words arrive and checks the sum after the last word. A tape destination is stored when its last word arrives.

## Snapshots

- strela_machine_save_snapshot writes a temporary sibling file and replaces the destination after success.
- strela_machine_load_snapshot validates version, checksum, lengths, addresses and pending events in a separate machine, then swaps state. A failed load leaves the current machine unchanged.
- Snapshots contain no function pointers, handles, host wall time or UI state.

## Stop reasons

strela_system_status gives running, normal stop, fault, budget or paused. The underlying reason is legacy_reason in the state, and strela_machine_reason_name names it: normal stop, control stop, comparison mismatch, arithmetic overflow, numeric domain stop, bad address, unknown constant, unknown opcode, I/O failure, media absent, media exhausted, checksum mismatch, or simulated time overflow. Exponent underflow produces machine zero without a stop. Bad image, bad snapshot and invalid path are reported by the call returning false.

## Not provided in this release

- A free-text provenance string per constant. Only the source class is stored.
- A bulk load_words call and explicit eject calls. Mounting replaces media, and reset keeps it.
- An event record returned by step. Read the state after the call instead.
- Distinct result codes for bad image and bad snapshot, and separate stops for an invalid BCD digit and an out-of-range BCD exponent.
- A reset that clears RAM to a power-on state. Create a new machine instead.

## Compatibility rule

The legacy include/strela.h functions remain as wrappers through the first complete headless release. Any incompatibility is listed in README.md with replacement calls and an example before the old surface is removed.
