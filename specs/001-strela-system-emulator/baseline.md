# Baseline: 2026-10-08

Command: cmake -S . -B build -G Ninja; cmake --build build; ctest --test-dir build --output-on-failure
Result: configure/build succeeded; strela_core 1/1 passed.
Command: build/strela_run.exe examples/hello.strela 0010 100 0100
Result: stopped at 0012 after 2 instructions, 2 cycles (1000 us simulated); 0100 contained 2000 0000 0000 0 01.

The sole test exercises pack/unpack indirectly, selected arithmetic/logic, groups, card callbacks, sparse tape, and estimated standard functions. It does not establish complete opcode or media correctness.
