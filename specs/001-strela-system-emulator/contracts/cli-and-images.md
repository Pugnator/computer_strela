# CLI and image contract

## CLI

The existing positional invocation stays valid during migration:

    strela_run listing.txt [start-octal] [step-limit] [dump-octal]

Explicit subcommands:

    strela_run run program.strela --start 0010 --cards input.cards --tape1 tape1.stt --tape2 tape2.stt --max-us 5000000 --dump 0100
    strela_run resume session.sts --max-us 5000000 --save-snapshot next.sts
    strela_run inspect session.sts --ram 0100 --tape1-zone 4001

- All machine addresses, words, and opcode fields are octal. Time budgets are unsigned decimal microseconds.
- Output includes the stop reason recorded in the machine, or paused or budget when there is none, plus instruction location, next location, omega, nominal intervals, simulated microseconds, and requested word dumps. inspect reports the stop that ended the saved run, even though a saved snapshot is paused.
- Exit 0 means a normal or requested control stop. Exit 1 is a machine fault, 2 invalid input or output, and 3 a time budget or legacy step limit. --help prints the same mapping.
- A missing media file is an input error before execution. An exhausted mounted deck is a machine stop during execution.
- The CLI never changes source program or input media files unless an explicit output path is supplied.

## Octal program listing

One command or data word per line:

    ADDRESS AAAA BBBB CCCC X OO

ADDRESS and A/B/C are four octal digits. X is one binary digit. OO is two octal digits. The five fields encode any 43-bit word, including non-instructions. Blank lines and lines beginning with # are ignored. Duplicate RAM addresses, out-of-range fields, and trailing unparsed data are errors. An optional separate directive selects the start address; a command-line --start overrides it.

The parser validates the whole listing before loading any word. Writing address 0000 is allowed syntactically but still reads back as zero.

## Card deck file, version 1

Text starts with STRELA-CARDS 1. Each CARD section has 12 WORD rows, where each WORD row contains the same AAAA BBBB CCCC X OO fields without an address. A saved partial punched card is represented separately as CARD-PARTIAL with count 1–11; it cannot be mounted as a completed input card unless an explicit finalize operation pads the remaining slots. Parsing is transactional.

## Tape image file, version 1

The first line is exactly STRELA-TAPE 1 UNIT=1 CAPACITY=HISTORICAL-100000, or the same with UNIT=2. The CAPACITY token names the only implemented capacity profile, 100,000 words per tape, and is required. Each ZONE section names its full octal address and a decimal word count from 1 to 2048, followed by that many WORD rows. Zones are serialized in ascending address order. Duplicate or invalid zone addresses and capacity violations are errors.

## Snapshot file, version 1

A snapshot begins with eight ASCII bytes STRELAST, unsigned little-endian version 1 (16 bits), unsigned payload length (32 bits), and an unsigned FNV-1a-64 checksum (64 bits) over the payload only. The FNV offset basis is 14695981039346656037; multiply each XOR-updated byte hash by 1099511628211 modulo 2^64. The total header is 22 bytes.

The payload contains seven required, increasing sections and one recognized optional section. Every section starts with unsigned little-endian ID (16 bits), flags (16 bits, zero in version 1), and body length (32 bits). ID 1 is machine and timing scalar state, 2 is all RAM and CDD words plus one CDD source byte per address (0 unknown, 1 built-in, 2 operator-supplied), 3 is mounted and punched card words and positions, 4 and 5 are sparse tape units, 6 is supplied zones and pending timed events, and 7 is the standard-function batch source snapshot. Readers built before the source byte gained value 2 reject a snapshot that contains an operator-supplied constant; that rejection is safe and leaves the machine unchanged. Optional ID 8 stores a per-word block-transfer cursor and its source word snapshot; older version 1 files without ID 8 restore with no pending block. Group cursor fields are in section 1. All words occupy unsigned 64-bit slots but must have high 21 bits clear. No host pointer or callback is stored.

IDs 1 through 7 are required; ID 8 is recognized and optional. Unknown sections, nonzero flags, trailing bytes, unsupported versions, malformed lengths, invalid addresses, capacity violations, or checksum mismatch are rejected. The decoder validates into a separate machine and swaps state only after complete success. Save writes a temporary sibling file and replaces the target after successful close. The frozen [version 1 example](../../tests/fixtures/book/kitov-midbatch-v1.sts) is the 1958 Kitov and Krinitsky program paused after the first element of opcode 72 (BCD-to-binary input conversion) at 0020: PC 0021, five batch elements left, elapsed 25,000 us. tests/test_snapshot.c loads and replays it.

The explicit CLI supports run, resume, inspect, operate, and prepare. prepare writes a paused snapshot of a listing before its first instruction. In run, --check-stops alone turns the control-stop switch on, and --check-stops 0 or 1 sets it explicitly, as in operate. run and resume can write --save-snapshot; resume also accepts --dump and media output paths. inspect accepts --ram, --tape1-zone and --tape2-zone. A time budget ends after a completed group or batch element and may overshoot by that element's cost. Exit codes are 0 normal/control stop, 1 machine fault, 2 input/output error, and 3 budget or legacy step limit.

The operate command applies operator actions to a saved session in argument order:

    strela_run operate session.sts --reset 0010 --set 0100 000000000000001 --check-stops 1 --step 1 --dump 0100 --save-snapshot next.sts

The --set word is exactly 15 octal digits (a raw 43-bit word). Edits and reset occur while paused. --step counts completed basic, group, batch, or block elements; a new snapshot retains all pending cursors. The CLI/API parity test replays the frozen Kitov session after one step.

