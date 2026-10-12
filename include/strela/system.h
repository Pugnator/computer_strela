#ifndef STRELA_SYSTEM_H
#define STRELA_SYSTEM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct strela_machine strela_machine;

typedef enum {
    STRELA_SYS_RUNNING, STRELA_SYS_NORMAL_STOP, STRELA_SYS_FAULT,
    STRELA_SYS_BUDGET, STRELA_SYS_PAUSED
} strela_system_status;

/* main_tact_us is source-documented (220). instruction_us is a
   throughput-derived estimate (500 by default), not a tact count. */
enum {
    STRELA_TIMING_INSTRUCTION_ESTIMATE = 1,
    STRELA_TIMING_CARD_ESTIMATE = 2,
    STRELA_TIMING_TAPE_ESTIMATE = 4
};

/* Where a CDD word came from. Unknown addresses cannot be read. */
typedef enum {
    STRELA_CONSTANT_UNKNOWN, STRELA_CONSTANT_BUILTIN, STRELA_CONSTANT_SUPPLIED
} strela_constant_source;

typedef struct {
    uint8_t estimate_flags;
    uint32_t main_tact_us;
    uint32_t instruction_us;
    uint32_t card_start_us, card_word_us;
    uint32_t tape_start_us, tape_word_us, tape_supply_us;
} strela_timing_profile;

typedef struct {
    uint16_t pc;
    uint8_t omega;
    uint64_t instructions;
    uint64_t cycles;
    uint64_t elapsed_us;
    uint16_t last_location;
    uint64_t last_instruction;
    uint64_t display_a;
    uint64_t display_b;
    uint16_t group_remaining;
    uint16_t batch_remaining;
    uint16_t block_remaining;
    uint32_t main_tact_us;
    uint32_t instruction_us;
    bool check_stops;
    uint16_t tape_supplied[2];
    uint8_t pending_events;
    uint64_t events_delivered;
    uint8_t timing_estimate_flags;
    strela_system_status status;
    int legacy_reason;
} strela_machine_state;

strela_timing_profile strela_timing_default(void);
strela_machine *strela_machine_create(const strela_timing_profile *profile);
void strela_machine_destroy(strela_machine *machine);
bool strela_machine_read_word(const strela_machine *machine, uint16_t address, uint64_t *word);
bool strela_machine_write_word(strela_machine *machine, uint16_t address, uint64_t word);
bool strela_machine_set_constant(strela_machine *machine, uint16_t address, uint64_t word);
strela_constant_source strela_machine_constant_source(const strela_machine *machine,
                                                      uint16_t address);
void strela_machine_set_check_stops(strela_machine *machine, bool enabled);
bool strela_machine_set_pc(strela_machine *machine, uint16_t address);
void strela_machine_read_state(const strela_machine *machine, strela_machine_state *state);
const char *strela_machine_reason_name(const strela_machine *machine);
strela_system_status strela_machine_step(strela_machine *machine);
strela_system_status strela_machine_run_for_us(strela_machine *machine, uint64_t budget_us);
bool strela_machine_load_cards(strela_machine *machine, const char *path);
bool strela_machine_save_punched(strela_machine *machine, const char *path);
bool strela_machine_load_tape(strela_machine *machine, unsigned unit, const char *path);
bool strela_machine_save_tape(strela_machine *machine, unsigned unit, const char *path);
bool strela_machine_mount_cards(strela_machine *machine,
                                const uint64_t *words, size_t count);
size_t strela_machine_card_position(const strela_machine *machine);
bool strela_machine_finalize_punched(strela_machine *machine);
size_t strela_machine_punched_words(const strela_machine *machine);
bool strela_machine_read_punched(const strela_machine *machine, size_t index, uint64_t *word);
bool strela_machine_mount_zone(strela_machine *machine, uint16_t zone_address,
                               const uint64_t *words, unsigned count);
bool strela_machine_read_zone_word(const strela_machine *machine, uint16_t address,
                                   unsigned index, uint64_t *word);
unsigned strela_machine_zone_length(const strela_machine *machine, uint16_t address);
bool strela_machine_reset(strela_machine *machine, uint16_t start_address);
void strela_machine_pause(strela_machine *machine);
void strela_machine_resume(strela_machine *machine);
bool strela_machine_save_snapshot(const strela_machine *machine, const char *path);
bool strela_machine_load_snapshot(strela_machine *machine, const char *path);
#ifdef __cplusplus
}
#endif
#endif


