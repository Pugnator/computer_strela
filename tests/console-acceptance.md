# Console acceptance: CLI and view

Configure and build with -DSTRELA_BUILD_MONITOR=ON, then run ctest
--test-dir build --output-on-failure. The monitor_cli_parity test steps the
frozen Kitov mid-batch snapshot once through each interface and compares the
complete versioned snapshot byte for byte. This covers PC, RAM, instruction,
omega, stop reason and operands, media/event cursors, and simulated time.

For a visual check from the repository root (on Windows, add .exe to binary
names if needed):

    build/strela_run inspect tests/fixtures/book/kitov-midbatch-v1.sts --ram 0052
    build/strela_mon tests/fixtures/book/kitov-midbatch-v1.sts --state --ram 0052
    build/strela_run operate tests/fixtures/book/kitov-midbatch-v1.sts --step 1 --dump 0052 --save-snapshot build/cli-step.sts
    build/strela_mon tests/fixtures/book/kitov-midbatch-v1.sts --step 1 --ram 0052 --save-snapshot build/monitor-step.sts
    cmake -E compare_files build/cli-step.sts build/monitor-step.sts

At the frozen starting point, both views report PC 0021, omega 0,
25,000 simulated microseconds, five batch elements remaining, and RAM
0052 equal to zero. After one step, the snapshot files must match exactly.
The monitor's last and next instruction lines are raw 43-bit words plus
the four/four/four/one/two octal field interpretation. The CLI's --dump
line uses that same field interpretation.
