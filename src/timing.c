#include "strela_internal.h"
#include <stddef.h>

uint64_t strela_elapsed_us(const strela_t *m)
{ return m ? m->elapsed_us : 0; }

void strela_timing_deliver_due(strela_t *m)
{
    while (m->event_count && m->events[0].due_us <= m->elapsed_us) {
        unsigned n;
        unsigned unit = m->events[0].unit;
        m->tape_supplied[unit] = m->events[0].zone;
        ++m->events_delivered;
        for (n = 1; n < m->event_count; ++n) m->events[n-1] = m->events[n];
        --m->event_count;
    }
}

bool strela_timing_schedule_supply(strela_t *m, unsigned unit, unsigned zone)
{
    uint64_t delay;
    unsigned n, pos;
    if (!m || unit >= 2 || !zone) return false;
    delay = (uint64_t)m->instruction_us + m->tape_supply_us;
    if (delay > UINT64_MAX - m->elapsed_us) {
        m->status = STRELA_TIME_OVERFLOW; return false;
    }
    for (n = 0; n < m->event_count; ++n)
        if (m->events[n].unit == unit) {
            for (; n + 1 < m->event_count; ++n) m->events[n] = m->events[n+1];
            --m->event_count;
            break;
        }
    if (m->event_count >= 8 || m->next_event_sequence == UINT64_MAX) {
        m->status = STRELA_TIME_OVERFLOW; return false;
    }
    pos = m->event_count++;
    while (pos && m->events[pos-1].due_us > m->elapsed_us + delay) {
        m->events[pos] = m->events[pos-1];
        --pos;
    }
    m->events[pos].due_us = m->elapsed_us + delay;
    m->events[pos].sequence = ++m->next_event_sequence;
    m->events[pos].zone = (uint16_t)zone;
    m->events[pos].unit = (uint8_t)unit;
    return true;
}

bool strela_timing_wait_tape(strela_t *m, unsigned unit)
{
    unsigned n;
    if (!m || unit >= 2) return false;
    for (n = 0; n < m->event_count; ++n)
        if (m->events[n].unit == unit) {
            m->elapsed_us = m->events[n].due_us;
            strela_timing_deliver_due(m);
            break;
        }
    return true;
}

bool strela_timing_charge_media(strela_t *m, bool tape, unsigned count)
{
    uint64_t extra = tape ? (uint64_t)m->tape_start_us +
                                 (uint64_t)m->tape_word_us * count :
                             (uint64_t)m->card_start_us +
                                 (uint64_t)m->card_word_us * count;
    if (extra > UINT64_MAX - m->elapsed_us) {
        m->status = STRELA_TIME_OVERFLOW; return false;
    }
    m->elapsed_us += extra;
    strela_timing_deliver_due(m);
    return true;
}

bool strela_timing_charge_word(strela_t *m, bool tape)
{
    uint64_t extra=tape ? m->tape_word_us : m->card_word_us;
    if (extra>UINT64_MAX-m->elapsed_us) {
        m->status=STRELA_TIME_OVERFLOW; return false;
    }
    m->elapsed_us+=extra;
    strela_timing_deliver_due(m);
    return true;
}
