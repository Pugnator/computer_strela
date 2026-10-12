#ifndef STRELA_INTERNAL_H
#define STRELA_INTERNAL_H
#include "strela.h"
bool strela_media_load_cards_text(strela_t *m, const char *path);
bool strela_media_save_cards_text(const strela_t *m, const char *path);
bool strela_media_load_tape_text(strela_t *m, unsigned unit, const char *path);
bool strela_media_save_tape_text(const strela_t *m, unsigned unit, const char *path);
bool strela_media_mount_cards(strela_t *m, const uint64_t *words, size_t count);
unsigned strela_media_read_cards(strela_t *m, uint64_t *words, unsigned count);
bool strela_media_finalize_output(strela_t *m);
bool strela_media_punch(strela_t *m, const uint64_t *words, unsigned count);
bool strela_media_store_zone(strela_t *m, unsigned tape, unsigned zone,
                             const uint64_t *words, unsigned count);
bool strela_tape_address(uint16_t address, unsigned *tape, unsigned *zone);
bool strela_timing_schedule_supply(strela_t *m, unsigned unit, unsigned zone);
void strela_timing_deliver_due(strela_t *m);
bool strela_timing_wait_tape(strela_t *m, unsigned unit);
bool strela_timing_charge_media(strela_t *m, bool tape, unsigned count);
bool strela_timing_charge_word(strela_t *m, bool tape);
void strela_block_start(strela_t *m, strela_instruction i, uint16_t location);
void strela_block_step(strela_t *m, bool first);
bool strela_word_add(uint64_t a, uint64_t b, unsigned opcode, uint64_t *out);
bool strela_word_multiply(uint64_t a, uint64_t b, uint64_t *out);
strela_status strela_step_boundary(strela_t *m);
void strela_execute(strela_t *m, strela_instruction i, uint16_t location);
void strela_batch_step(strela_t *m, bool first);
#endif
