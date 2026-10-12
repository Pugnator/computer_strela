#include "strela.h"
#include "strela_internal.h"
#include <stdlib.h>
#include <string.h>

bool strela_read(const strela_t *m, uint16_t address, uint64_t *word)
{
    if (!m || !word) return false;
    if (address < STRELA_RAM_WORDS) {
        *word = m->ram[address];
        return true;
    }
    if (address >= STRELA_CDD_FIRST && address <= STRELA_CDD_LAST) {
        if (!m->cdd_valid[address - STRELA_CDD_FIRST]) return false;
        *word = m->cdd[address - STRELA_CDD_FIRST];
        return true;
    }
    return false;
}

bool strela_write(strela_t *m, uint16_t address, uint64_t word)
{
    if (!m || address >= STRELA_RAM_WORDS) return false;
    if (address) m->ram[address] = word & STRELA_WORD_MASK;
    return true;
}

bool strela_set_constant(strela_t *m, uint16_t address, uint64_t word)
{
    if (!m || address < STRELA_CDD_FIRST || address > STRELA_CDD_LAST) return false;
    m->cdd[address - STRELA_CDD_FIRST] = word & STRELA_WORD_MASK;
    m->cdd_valid[address - STRELA_CDD_FIRST] = 2;
    return true;
}

static void constant(strela_t *m, uint16_t address, uint16_t a, uint16_t b,
                     uint16_t c, uint8_t check, uint8_t opcode)
{
    strela_instruction i = {a, b, c, check, opcode};
    strela_set_constant(m, address, strela_pack(i));
    m->cdd_valid[address - STRELA_CDD_FIRST] = 1; /* built-in profile */
}

void strela_init(strela_t *m)
{
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->instruction_us = STRELA_CYCLE_US;
    m->main_tact_us = STRELA_MAIN_TACT_US;
    m->card_start_us = 10000; m->card_word_us = 1000;
    m->tape_start_us = 20000; m->tape_word_us = 500;
    m->tape_supply_us = 50000;
    m->timing_estimate_flags = 7;
    /* Kitov and Krinitsky (1959), printed p. 141, table 26. */
    constant(m, 07400, 02000, 0, 0, 0, 001);
    constant(m, 07423, 1, 0, 0, 0, 0);
    constant(m, 07424, 0, 1, 0, 0, 0);
    constant(m, 07425, 0, 0, 1, 0, 0);
    constant(m, 07426, 1, 1, 0, 0, 0);
    constant(m, 07427, 0, 1, 1, 0, 0);
    constant(m, 07430, 1, 0, 1, 0, 0);
    constant(m, 07431, 1, 1, 1, 0, 0);
    constant(m, 07434, 07777, 07777, 07777, 0, 0);
    constant(m, 07435, 03777, 07777, 07777, 1, 077);
    constant(m, 07436, 0, 0, 0, 1, 077);
    constant(m, 07437, 0, 0, 0, 0, 077);
    constant(m, 07440, 0, 0, 0, 1, 0);
    constant(m, 07446, 07777, 0, 0, 0, 0);
    constant(m, 07447, 0, 07777, 0, 0, 0);
    constant(m, 07450, 0, 0, 07777, 0, 0);
    constant(m, 07451, 07777, 07777, 0, 0, 0);
    constant(m, 07452, 0, 07777, 07777, 0, 0);
    constant(m, 07453, 07777, 0, 07777, 0, 0);
    constant(m, 07455, 02000, 0, 0, 0, 002);
    constant(m, 07461, 02000, 0, 0, 0, 0);
    constant(m, 07523, 02650, 01171, 04640, 0, 001);
    constant(m, 07560, 03110, 03755, 02420, 0, 001);
    constant(m, 07575, 04000, 0, 0, 0, 043);
}

void strela_destroy(strela_t *m)
{
    unsigned tape, zone;
    if (!m) return;
    free(m->card_deck);
    free(m->card_output);
    m->card_deck = m->card_output = NULL;
    m->card_deck_words = m->card_position = m->card_output_words = 0;
    m->card_mounted = false;
    for (tape = 0; tape < 2; ++tape)
        for (zone = 0; zone < 512; ++zone) {
            free(m->tape[tape][zone]);
            m->tape[tape][zone] = NULL;
            m->tape_length[tape][zone] = 0;
        }
    m->tape_total_words[0] = m->tape_total_words[1] = 0;
}

strela_status strela_step_boundary(strela_t *m)
{
    uint64_t word, previous_cycles;
    strela_instruction instruction;
    uint16_t location;
    if (!m) return STRELA_BAD_ADDRESS;
    if (m->status != STRELA_RUNNING) return m->status;
    previous_cycles = m->cycles;
    if (m->group_remaining) {
        instruction = strela_unpack(m->group_word);
        location = m->group_location;
        m->last_location = location;
        m->last_instruction = m->group_word;
        strela_execute(m, instruction, location);
        --m->group_remaining;
        if (m->group_increment & 4) instruction.a = (instruction.a + 1) & 07777;
        if (m->group_increment & 2) instruction.b = (instruction.b + 1) & 07777;
        if (m->group_increment & 1) instruction.c = (instruction.c + 1) & 07777;
        m->group_word = strela_pack(instruction);
    } else if (m->batch_remaining) {
        instruction = strela_unpack(m->batch_word);
        m->last_location = m->batch_location;
        m->last_instruction = m->batch_word;
        strela_batch_step(m, false);
    } else if (m->block_remaining) {
        instruction = strela_unpack(m->block_word);
        m->last_location = m->block_location;
        m->last_instruction = m->block_word;
        strela_block_step(m, false);
    } else {
        if (!strela_read(m, m->pc, &word)) {
            m->status = STRELA_BAD_ADDRESS; return m->status;
        }
        instruction = strela_unpack(word);
        location = m->pc;
        m->last_location = location;
        m->last_instruction = word;
        m->pc = (uint16_t)((m->pc + 1) & 07777);
        ++m->instructions;
        strela_execute(m, instruction, location);
    }
    if (m->instruction_us &&
        m->cycles - previous_cycles > (UINT64_MAX - m->elapsed_us) / m->instruction_us) {
        m->status = STRELA_TIME_OVERFLOW;
        return m->status;
    }
    m->elapsed_us += (m->cycles - previous_cycles) * m->instruction_us;
    strela_timing_deliver_due(m);
    if (m->status == STRELA_RUNNING && instruction.check && m->check_stops)
        m->status = STRELA_CONTROL_STOP;
    return m->status;
}

