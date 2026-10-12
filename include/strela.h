#ifndef STRELA_H
#define STRELA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STRELA_WORD_MASK UINT64_C(0x7ffffffffff)
#define STRELA_RAM_WORDS 2048
#define STRELA_CDD_FIRST 07400
#define STRELA_CDD_LAST 07777
#define STRELA_MAIN_TACT_US 220
/* Legacy nominal instruction interval inferred from 2,000 operations/s.
   It is distinct from the 220 us main tact. */
#define STRELA_CYCLE_US 500

typedef bool (*strela_card_read_fn)(void *user, uint64_t *words, unsigned count);
typedef bool (*strela_card_write_fn)(void *user, const uint64_t *words,
                                      unsigned count);

typedef enum {
    STRELA_RUNNING = 0,
    STRELA_STOPPED,
    STRELA_BAD_ADDRESS,
    STRELA_BAD_OPCODE,
    STRELA_ARITHMETIC_STOP,
    STRELA_IO_UNAVAILABLE,
    STRELA_CONTROL_STOP,
    STRELA_MISMATCH_STOP,
    STRELA_UNKNOWN_CONSTANT,
    STRELA_DOMAIN_STOP,
    STRELA_MEDIA_ABSENT,
    STRELA_MEDIA_EXHAUSTED,
    STRELA_CHECKSUM_MISMATCH,
    STRELA_TIME_OVERFLOW
} strela_status;

typedef struct {
    uint64_t ram[STRELA_RAM_WORDS];
    uint64_t cdd[256];
    uint8_t cdd_valid[256]; /* 0 unknown, 1 built-in, 2 operator-supplied */
    uint64_t *tape[2][512];
    uint16_t tape_length[2][512];
    uint32_t tape_total_words[2];
    uint64_t *card_deck;
    size_t card_deck_words;
    size_t card_position;
    bool card_mounted;
    uint64_t *card_output;
    size_t card_output_words;

    strela_card_read_fn card_read;
    strela_card_write_fn card_write;
    void *card_user;
    uint16_t pc;
    uint8_t omega;
    bool check_stops;
    strela_status status;
    uint64_t instructions;
    uint64_t cycles;
    uint64_t elapsed_us;
    uint32_t instruction_us;
    uint32_t main_tact_us;
    uint64_t group_word;
    uint16_t group_location;
    uint16_t group_remaining;
    uint8_t group_increment;
    uint64_t batch_word;
    uint16_t batch_location;
    uint16_t batch_count;
    uint16_t batch_index;
    uint16_t batch_remaining;
    uint16_t batch_a, batch_c;
    uint8_t batch_opcode;
    uint64_t batch_source[STRELA_RAM_WORDS];
    uint64_t block_word;
    uint16_t block_location, block_count, block_index, block_remaining;
    uint16_t block_a, block_c;
    uint16_t block_source_zone, block_dest_zone;
    uint8_t block_opcode, block_source_kind, block_dest_kind, block_media_kind;
    uint8_t block_source_tape, block_dest_tape;
    uint64_t block_recorded, block_calculated;
    uint64_t block_words[STRELA_RAM_WORDS + 1];
    uint16_t last_location;
    uint64_t last_instruction;
    uint64_t display_a;
    uint64_t display_b;
    uint32_t card_start_us, card_word_us;
    uint32_t tape_start_us, tape_word_us, tape_supply_us;
    uint16_t tape_supplied[2];
    struct {
        uint64_t due_us;
        uint64_t sequence;
        uint16_t zone;
        uint8_t unit;
    } events[8];
    uint8_t event_count;
    uint64_t next_event_sequence;
    uint64_t events_delivered;
    uint8_t timing_estimate_flags;
} strela_t;

typedef struct {
    uint16_t a, b, c;
    uint8_t check, opcode;
} strela_instruction;

/* Initialize fresh storage. Call strela_destroy before reinitializing a machine
   that may contain allocated tape zones. */
void strela_init(strela_t *machine);
void strela_destroy(strela_t *machine);
bool strela_read(const strela_t *machine, uint16_t address, uint64_t *word);
bool strela_write(strela_t *machine, uint16_t address, uint64_t word);
bool strela_set_constant(strela_t *machine, uint16_t address, uint64_t word);
uint64_t strela_pack(strela_instruction instruction);
strela_instruction strela_unpack(uint64_t word);
double strela_to_double(uint64_t word);
bool strela_from_double(double value, uint64_t *word);
strela_status strela_step(strela_t *machine);

#ifdef __cplusplus
}
#endif
#endif
