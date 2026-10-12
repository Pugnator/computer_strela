# Checked transfer profile

The selected opcode 60 profile stores the end-around sum before the data array. The 1959 Kitov-Krinitsky book, section 28, and the Russian Virtual Computer Museum opcode table describe this placement, while noting that some machine variants stored the sum after the array.

The executable vectors are in tests/test_media.c: two RAM words 012345 and 007777 sum to 022344; RAM to either tape, tape to RAM, RAM to card, card to RAM, overlapping RAM copy, and deliberately corrupted sums are checked. Transfers commit one word at a time. An exhausted deck keeps the words already read by unchecked transfer 41. Checked transfer 60 also writes RAM destinations as each data word arrives and compares the sums only after the last word, so a mismatch stop leaves the data in RAM; tests/test_media.c asserts this. A tape destination is stored as one zone when the last word arrives, so a stop before then leaves the zone unchanged.
