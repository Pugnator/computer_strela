# Optional Strela operator console

Build with:

    cmake -S . -B build -DSTRELA_BUILD_MONITOR=ON
    cmake --build build

Open a snapshot with strela_mon session.sts. The command interface has four
areas:

| Area | What it displays | Source cue |
| --- | --- | --- |
| Control panel | Run state, PC, omega, check switch, stop operands and microseconds | The surviving manual control panel was used to start, stop and monitor commands. |
| Address generator / instruction | Last and next instruction as raw 43-bit octal words and decoded fields | The museum identifies a surviving nine-tube number/address generator panel. |
| Memory inspection | Eight RAM words beginning at a selected octal address | The surviving inspection block could view the contents of a storage tube. |
| Media / events | Card positions, tape supply, pending events and an optional zone | This is a modern system-state summary, without a claimed panel equivalent. |

The [Polytechnic Museum collection record and its photo gallery](https://polymus.ru/collection/detail/bloki-elektronnoy-vychislitelnoy-mashiny-strela)
show the three surviving modules. The gallery informs the grouping and names.
It does not document a complete control panel, wiring, lamp meanings, or
instruction timing. The emulator's execution rules come from the local books
and the core, not from the photos. This text layout is a functional operator
view, not a physical replica.

Commands are show, ram 0052, zone 4001, step 1, run 1000, pause,
resume, reset 0020, set 0052 000000000000001, check 1,
save next.sts, help, and quit. Run pauses at the end of the budget or
machine stop. Word edits, the check switch and reset require a paused or stopped machine. Octal words have
15 digits; addresses have four. A snapshot can also be inspected or stepped
without entering the prompt:

    strela_mon session.sts --state --ram 0052
    strela_mon session.sts --step 1 --ram 0052 --save-snapshot next.sts

Only strela/system.h is included by the monitor. The optional target uses
the same opaque machine functions as the CLI and has no graphical or wall-clock
runtime dependency. The 220 us main tact is shown separately from the
throughput-derived nominal instruction interval. See
[tests/console-acceptance.md](../../tests/console-acceptance.md) for the shared
CLI/view comparison.
