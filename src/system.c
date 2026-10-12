#include "strela/system.h"
#include "strela.h"
#include "strela_internal.h"
#include "system_internal.h"
#include <stdlib.h>


strela_timing_profile strela_timing_default(void)
{
    strela_timing_profile p = {7, STRELA_MAIN_TACT_US, STRELA_CYCLE_US,
                               10000, 1000, 20000, 500, 50000};
    return p;
}

strela_machine *strela_machine_create(const strela_timing_profile *profile)
{
    strela_machine *m;
    strela_timing_profile p = profile ? *profile : strela_timing_default();
    if (!p.main_tact_us || !p.instruction_us) return NULL;
    m = (strela_machine *)malloc(sizeof(*m));
    if (!m) return NULL;
    strela_init(&m->core);
    m->core.main_tact_us = p.main_tact_us;
    m->core.instruction_us = p.instruction_us;
    m->core.card_start_us = p.card_start_us;
    m->core.card_word_us = p.card_word_us;
    m->core.tape_start_us = p.tape_start_us;
    m->core.tape_word_us = p.tape_word_us;
    m->core.tape_supply_us = p.tape_supply_us;
    m->core.timing_estimate_flags = p.estimate_flags;
    m->paused = true;
    return m;
}

void strela_machine_destroy(strela_machine *m)
{
    if (!m) return;
    strela_destroy(&m->core);
    free(m);
}

/* Operator edits are accepted while the machine is paused or stopped, as
   in the data model; they are rejected only while it is running. */
static bool idle(const strela_machine *m)
{ return m && (m->paused || m->core.status != STRELA_RUNNING); }

bool strela_machine_read_word(const strela_machine *m, uint16_t a, uint64_t *w)
{ return m && strela_read(&m->core, a, w); }

bool strela_machine_write_word(strela_machine *m, uint16_t a, uint64_t w)
{ return idle(m) && strela_write(&m->core, a, w); }

bool strela_machine_set_constant(strela_machine *m, uint16_t a, uint64_t w)
{ return idle(m) && !m->core.instructions && strela_set_constant(&m->core, a, w); }

strela_constant_source strela_machine_constant_source(const strela_machine *m, uint16_t a)
{
    if (!m || a < STRELA_CDD_FIRST || a > STRELA_CDD_LAST) return STRELA_CONSTANT_UNKNOWN;
    switch (m->core.cdd_valid[a - STRELA_CDD_FIRST]) {
    case 1: return STRELA_CONSTANT_BUILTIN;
    case 2: return STRELA_CONSTANT_SUPPLIED;
    default: return STRELA_CONSTANT_UNKNOWN;
    }
}

void strela_machine_set_check_stops(strela_machine *m, bool enabled)
{
    if (idle(m)) m->core.check_stops = enabled;
}

bool strela_machine_set_pc(strela_machine *m, uint16_t a)
{
    if (!idle(m) || a >= STRELA_RAM_WORDS ||
        m->core.group_remaining || m->core.batch_remaining ||
        m->core.block_remaining) return false;
    m->core.pc = a;
    return true;
}

static strela_system_status status_of(const strela_machine *m)
{
    if (m->paused) return STRELA_SYS_PAUSED;
    if (m->core.status == STRELA_RUNNING) return STRELA_SYS_RUNNING;
    if (m->core.status == STRELA_STOPPED || m->core.status == STRELA_CONTROL_STOP) return STRELA_SYS_NORMAL_STOP;
    return STRELA_SYS_FAULT;
}

void strela_machine_read_state(const strela_machine *m, strela_machine_state *s)
{
    if (!m || !s) return;
    s->pc = m->core.pc;
    s->omega = m->core.omega;
    s->instructions = m->core.instructions;
    s->cycles = m->core.cycles;
    s->elapsed_us = m->core.elapsed_us;
    s->last_location = m->core.last_location;
    s->last_instruction = m->core.last_instruction;
    s->display_a = m->core.display_a;
    s->display_b = m->core.display_b;
    s->group_remaining = m->core.group_remaining;
    s->batch_remaining = m->core.batch_remaining;
    s->block_remaining = m->core.block_remaining;
    s->main_tact_us = m->core.main_tact_us;
    s->instruction_us = m->core.instruction_us;
    s->check_stops = m->core.check_stops;
    s->tape_supplied[0] = m->core.tape_supplied[0];
    s->tape_supplied[1] = m->core.tape_supplied[1];
    s->pending_events = m->core.event_count;
    s->events_delivered = m->core.events_delivered;
    s->timing_estimate_flags = m->core.timing_estimate_flags;
    s->status = status_of(m);
    s->legacy_reason = (int)m->core.status;
}

const char *strela_machine_reason_name(const strela_machine *m)
{
    if (!m) return "invalid machine";
    switch (m->core.status) {
    case STRELA_RUNNING: return "running";
    case STRELA_STOPPED: return "normal stop";
    case STRELA_BAD_ADDRESS: return "bad address";
    case STRELA_BAD_OPCODE: return "unknown opcode";
    case STRELA_ARITHMETIC_STOP: return "arithmetic overflow";
    case STRELA_IO_UNAVAILABLE: return "I/O failure";
    case STRELA_CONTROL_STOP: return "control stop";
    case STRELA_MISMATCH_STOP: return "comparison mismatch";
    case STRELA_UNKNOWN_CONSTANT: return "unknown constant";
    case STRELA_DOMAIN_STOP: return "numeric domain stop";
    case STRELA_MEDIA_ABSENT: return "media absent";
    case STRELA_MEDIA_EXHAUSTED: return "media exhausted";
    case STRELA_CHECKSUM_MISMATCH: return "checksum mismatch";
    case STRELA_TIME_OVERFLOW: return "simulated time overflow";
    }
    return "unknown status";
}
strela_system_status strela_machine_step(strela_machine *m)
{
    bool was_paused;
    if (!m) return STRELA_SYS_FAULT;
    was_paused=m->paused;
    if (m->core.status != STRELA_RUNNING) {
        m->paused=false;
        return status_of(m);
    }
    strela_step_boundary(&m->core);
    m->paused=was_paused && m->core.status==STRELA_RUNNING;
    return status_of(m);
}

strela_system_status strela_machine_run_for_us(strela_machine *m, uint64_t budget_us)
{
    uint64_t start;
    if (!m) return STRELA_SYS_FAULT;
    if (m->paused) return STRELA_SYS_PAUSED;
    start = m->core.elapsed_us;
    while (m->core.status == STRELA_RUNNING &&
           m->core.elapsed_us - start < budget_us) {
        strela_step_boundary(&m->core);
        if (m->core.elapsed_us == UINT64_MAX) break;
    }
    if (m->core.status != STRELA_RUNNING) return status_of(m);
    return STRELA_SYS_BUDGET;
}

bool strela_machine_reset(strela_machine *m, uint16_t start)
{
    strela_t *c;
    if (!idle(m) || start>=STRELA_RAM_WORDS) return false;
    c=&m->core;
    c->pc=start; c->omega=0; c->status=STRELA_RUNNING;
    c->instructions=0; c->cycles=0; c->elapsed_us=0;
    c->group_word=0; c->group_location=0;
    c->group_remaining=0; c->group_increment=0;
    c->batch_word=0; c->batch_location=0;
    c->batch_count=0; c->batch_index=0; c->batch_remaining=0;
    c->batch_a=0; c->batch_c=0; c->batch_opcode=0;
    c->block_word=0; c->block_location=0;
    c->block_count=0; c->block_index=0; c->block_remaining=0;
    c->block_a=0; c->block_c=0;
    c->block_source_zone=0; c->block_dest_zone=0;
    c->block_opcode=0; c->block_source_kind=0; c->block_dest_kind=0;
    c->block_media_kind=0; c->block_source_tape=0; c->block_dest_tape=0;
    c->block_recorded=0; c->block_calculated=0;
    c->last_location=0; c->last_instruction=0;
    c->display_a=0; c->display_b=0;
    c->tape_supplied[0]=c->tape_supplied[1]=0;
    c->event_count=0; c->events_delivered=0; c->next_event_sequence=0;
    c->card_position=0;
    c->card_output_words=0;
    m->paused=true; /* reset leaves the operator at the start address */
    return true;
}
void strela_machine_pause(strela_machine *m) { if (m) m->paused = true; }
void strela_machine_resume(strela_machine *m)
{
    if (!m) return;
    if (m->core.status == STRELA_CONTROL_STOP) m->core.status = STRELA_RUNNING;
    m->paused = false;
}

bool strela_machine_mount_cards(strela_machine *m, const uint64_t *words, size_t count)
{
    return idle(m) && strela_media_mount_cards(&m->core, words, count);
}

size_t strela_machine_card_position(const strela_machine *m)
{ return m ? m->core.card_position : 0; }

bool strela_machine_finalize_punched(strela_machine *m)
{ return idle(m) && strela_media_finalize_output(&m->core); }

size_t strela_machine_punched_words(const strela_machine *m)
{ return m ? m->core.card_output_words : 0; }

bool strela_machine_read_punched(const strela_machine *m, size_t index, uint64_t *word)
{
    if (!m || !word || index >= m->core.card_output_words) return false;
    *word = m->core.card_output[index];
    return true;
}

bool strela_machine_mount_zone(strela_machine *m, uint16_t address,
                               const uint64_t *words, unsigned count)
{
    unsigned tape, zone;
    return idle(m) && strela_tape_address(address, &tape, &zone) &&
           strela_media_store_zone(&m->core, tape, zone, words, count);
}

bool strela_machine_load_cards(strela_machine *m, const char *path)
{ return idle(m) && strela_media_load_cards_text(&m->core,path); }

bool strela_machine_save_punched(strela_machine *m, const char *path)
{ return idle(m) && strela_media_save_cards_text(&m->core,path); }

bool strela_machine_load_tape(strela_machine *m, unsigned unit, const char *path)
{ return idle(m) && strela_media_load_tape_text(&m->core,unit,path); }

bool strela_machine_save_tape(strela_machine *m, unsigned unit, const char *path)
{ return idle(m) && strela_media_save_tape_text(&m->core,unit,path); }


unsigned strela_machine_zone_length(const strela_machine *m, uint16_t address)
{
    unsigned tape,zone;
    if (!m || !strela_tape_address(address,&tape,&zone)) return 0;
    return m->core.tape_length[tape][zone];
}

bool strela_machine_read_zone_word(const strela_machine *m, uint16_t address,
                                   unsigned index, uint64_t *word)
{
    unsigned tape,zone;
    if (!m || !word || !strela_tape_address(address,&tape,&zone) ||
        index>=m->core.tape_length[tape][zone]) return false;
    *word=m->core.tape[tape][zone][index];
    return true;
}
