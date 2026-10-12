# Strela system emulator

A C99 instruction and system emulator for Strela. It models 43-bit words, 2048 RAM cells, the constant display device (CDD), ordered card decks, two tape units, operator controls, per-word transfer cursors, portable snapshots, and simulated time. The separately documented **220 us main tact** is retained in the timing profile. The default **500 us nominal instruction interval** follows the historical 2,000 operations/s figure and is an inference about average throughput, not a reconstructed hardware microcycle.

## Build and test

    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build --output-on-failure

The library builds as static strela and shared strela_shared. [include/strela/system.h](include/strela/system.h) is the opaque API; [include/strela.h](include/strela.h) remains available to older users.

## Run a program

    build/strela_run run tests/fixtures/book/kitov-linear.strela --start 0020 --max-us 1000000 --dump 0052
    build/strela_run run examples/programs/two-branches.strela --max-us 100000

A listing line is ADDRESS AAAA BBBB CCCC X OO, with four octal digits for each address, a single check bit, and a two-digit octal opcode. START 0010 in a listing selects a start address; --start overrides it. Invalid or duplicate entries fail before execution.

The older positional command still works:

    build/strela_run examples/hello.strela 0010 100 0100

## Media and saved sessions

    build/strela_run run program.strela --cards input.cards --tape1 tape1.stt --tape2 tape2.stt --max-us 1000000 --out-cards output.cards --save-snapshot session.sts
    build/strela_run inspect session.sts --ram 0100 --tape1-zone 4001
    build/strela_run resume session.sts --max-us 1000000 --save-snapshot next.sts

Card images are twelve raw words per CARD; tape images contain ordered ZONE sections. The text formats and snapshot binary layout are described in [the CLI and image contract](specs/001-strela-system-emulator/contracts/cli-and-images.md). Exit 0 is a normal or control stop, 1 a machine fault, 2 invalid input/output, and 3 a time or step limit.

## Optional operator console

    cmake -S . -B build -DSTRELA_BUILD_MONITOR=ON
    cmake --build build
    build/strela_mon tests/fixtures/book/kitov-midbatch-v1.sts

The [museum-informed terminal console](tools/strela_mon/README.md) shows controls, instruction fields, RAM, media state and simulated time from the same opaque API as the CLI. Use step, run, ram, zone, set, reset and save at the prompt. The [CLI/view acceptance](tests/console-acceptance.md) compares complete snapshots after the same operator action. The target is optional and disabled by default.

## Browser operator console

    ./web/build-wasm.ps1
    node web/test-wasm.mjs
    node web/serve-static.mjs

Open http://127.0.0.1:4318. The [virtual operator console](web/README.md)
runs the same C emulator as WebAssembly in the browser. It loads octal
listings and card/tape images; provides step, run, reset and word editing;
shows memory, stop state, media and simulated time; and saves/restores
snapshots. Emscripten is needed to build the static files. The earlier
localhost native-server mode remains available for development.
## Scope and evidence

The [opcode matrix](docs/OPCODES.md) gives the implemented behavior and source confidence for each documented command. The [book inventory](docs/BOOKS.md) explains the local scans. In particular, the 1958 Kitov and Krinitsky book supplies the complete linear-equation program used by the end-to-end test. The [timing notes](docs/TIMING.md) distinguish sourced figures from estimated media and longer-operation costs. Standard mathematical functions use host math and raw-word conversion, so their last bits and exceptional edge cases remain approximations.

Primary references: [Kitov and Krinitsky's 1959 Strela chapter](docs/ecm_i_prog.pdf), [Kitov and Krinitsky's 1958 machine description and program](docs/kitov_evm.pdf), and the [Russian Virtual Computer Museum instruction table](https://www.computer-museum.ru/english/strcomm.htm). The [Ershov archive timing table](https://ershov.iis.nsk.su/ru/foundation/index), [Habr history](https://habr.com/en/companies/ua-hosting/articles/371785/), and [Polytechnic Museum blocks](https://polymus.ru/collection/detail/bloki-elektronnoy-vychislitelnoy-mashiny-strela) inform timing and operation. See [source notes](docs/SOURCE-NOTES.md) for conflicts.

The headless CLI also supports operate for snapshot-based reset, RAM edit, control-switch changes and single-step. The [Spec Kit tasks](specs/001-strela-system-emulator/tasks.md) record implementation and validation evidence.
