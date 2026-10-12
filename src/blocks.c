#include "strela_internal.h"
#include <stddef.h>

enum { CARD=0, RAM=1, TAPE=2 };

static uint64_t end_around(uint64_t a, uint64_t b)
{
    uint64_t sum=a+b;
    return ((sum&STRELA_WORD_MASK)+(sum>>43))&STRELA_WORD_MASK;
}
static bool ram_range(unsigned start, unsigned count)
{
    return start<STRELA_RAM_WORDS && count<=STRELA_RAM_WORDS-start;
}
static bool card_ready(const strela_t *m)
{
    return m->card_mounted || m->card_read;
}
static bool source_ready(strela_t *m)
{
    if (m->block_source_kind==CARD && !card_ready(m)) {
        m->status=STRELA_MEDIA_ABSENT; return false;
    }
    if (m->block_source_kind==TAPE &&
        !m->tape[m->block_source_tape][m->block_source_zone]) {
        m->status=STRELA_MEDIA_ABSENT; return false;
    }
    return true;
}
void strela_block_start(strela_t *m, strela_instruction i, uint16_t location)
{
    unsigned n=(unsigned)i.b+1,k,tape=0,zone=0;
    m->block_word=strela_pack(i);
    m->block_location=location;
    m->block_opcode=i.opcode;
    m->block_a=i.a; m->block_c=i.c;
    m->block_source_kind=RAM; m->block_dest_kind=RAM;
    m->block_source_tape=m->block_dest_tape=0;
    m->block_source_zone=m->block_dest_zone=0;
    m->block_media_kind=0;
    m->block_recorded=m->block_calculated=0;
    switch (i.opcode) {
    case 041:
        if (i.a || !ram_range(i.c,n)) goto bad_address;
        m->block_source_kind=CARD; m->block_media_kind=1;
        break;
    case 043:
        if (!strela_tape_address(i.a,&tape,&zone) ||
            !ram_range(i.c,n)) goto bad_address;
        m->block_source_kind=TAPE; m->block_media_kind=2;
        m->block_source_tape=(uint8_t)tape;
        m->block_source_zone=(uint16_t)zone;
        break;
    case 044:
        if (i.c || !ram_range(i.a,n)) goto bad_address;
        m->block_dest_kind=CARD; m->block_media_kind=1;
        for (k=0;k<n;++k) m->block_words[k]=m->ram[i.a+k];
        break;
    case 045:
        if (!ram_range(i.a,n) || !ram_range(i.c,n)) goto bad_address;
        for (k=0;k<n;++k) m->block_words[k]=m->ram[i.a+k];
        break;
    case 046:
        if (!strela_tape_address(i.a,&tape,&zone) ||
            !ram_range(i.c,n)) goto bad_address;
        m->block_dest_kind=TAPE; m->block_media_kind=2;
        m->block_dest_tape=(uint8_t)tape;
        m->block_dest_zone=(uint16_t)zone;
        for (k=0;k<n;++k) m->block_words[k]=m->ram[i.c+k];
        break;
    case 060:
        if (i.b==0 || i.b>=STRELA_RAM_WORDS) goto bad_address;
        n=(unsigned)i.b+1;
        if (i.a==0) m->block_source_kind=CARD;
        else if (i.a<STRELA_RAM_WORDS) m->block_source_kind=RAM;
        else if (strela_tape_address(i.a,&tape,&zone)) {
            m->block_source_kind=TAPE;
            m->block_source_tape=(uint8_t)tape;
            m->block_source_zone=(uint16_t)zone;
        } else goto bad_address;
        if (i.c==0) m->block_dest_kind=CARD;
        else if (i.c<STRELA_RAM_WORDS) m->block_dest_kind=RAM;
        else if (strela_tape_address(i.c,&tape,&zone)) {
            m->block_dest_kind=TAPE;
            m->block_dest_tape=(uint8_t)tape;
            m->block_dest_zone=(uint16_t)zone;
        } else goto bad_address;
        if ((m->block_source_kind!=RAM && m->block_dest_kind!=RAM) ||
            (m->block_source_kind==RAM && !ram_range(i.a,i.b)) ||
            (m->block_dest_kind==RAM && !ram_range(i.c,n)))
            goto bad_address;
        m->block_media_kind=(m->block_source_kind==TAPE ||
                             m->block_dest_kind==TAPE) ? 2 :
                            (m->block_source_kind==CARD ||
                             m->block_dest_kind==CARD) ? 1 : 0;
        if (m->block_source_kind==RAM) {
            for (k=0;k<i.b;++k) {
                m->block_words[k+1]=m->ram[i.a+k];
                m->block_recorded=end_around(m->block_recorded,
                                               m->block_words[k+1]);
            }
            m->block_words[0]=m->block_recorded;
        }
        break;
    default:
        m->status=STRELA_BAD_OPCODE; return;
    }
    m->block_count=(uint16_t)n;
    m->block_index=0;
    m->block_remaining=(uint16_t)n;
    if (!source_ready(m)) return;
    if (m->block_source_kind==TAPE &&
        !strela_timing_wait_tape(m,m->block_source_tape)) return;
    if (m->block_dest_kind==TAPE &&
        !strela_timing_wait_tape(m,m->block_dest_tape)) return;
    if (m->block_media_kind &&
        !strela_timing_charge_media(m,m->block_media_kind==2,0)) return;
    strela_block_step(m,true);
    return;
bad_address:
    m->status=STRELA_BAD_ADDRESS;
}
void strela_block_step(strela_t *m, bool first)
{
    unsigned k=m->block_index;
    uint64_t word;
    if (!m->block_remaining || k>=m->block_count) {
        m->status=STRELA_BAD_ADDRESS; return;
    }
    if (m->block_source_kind==CARD) {
        if (strela_media_read_cards(m,&word,1)!=1) return;
    } else if (m->block_source_kind==TAPE) {
        unsigned unit=m->block_source_tape,zone=m->block_source_zone;
        if (k>=m->tape_length[unit][zone]) {
            m->status=STRELA_MEDIA_EXHAUSTED; return;
        }
        word=m->tape[unit][zone][k];
    } else {
        word=m->block_words[k];
    }
    if (m->block_opcode==060) {
        if (k==0 && m->block_source_kind!=RAM) m->block_recorded=word;
        if (k) m->block_calculated=end_around(m->block_calculated,word);
    }
    if (m->block_dest_kind==CARD) {
        if (!strela_media_punch(m,&word,1)) return;
    } else if (m->block_dest_kind==TAPE) {
        m->block_words[k]=word;
        if (k+1==m->block_count &&
            !strela_media_store_zone(m,m->block_dest_tape,
                                     m->block_dest_zone,m->block_words,
                                     m->block_count)) {
            m->status=STRELA_IO_UNAVAILABLE; return;
        }
    } else {
        if (!strela_write(m,(uint16_t)(m->block_c+k),word)) {
            m->status=STRELA_BAD_ADDRESS; return;
        }
    }
    m->cycles+=first ? 0u : 1u;
    ++m->block_index;
    --m->block_remaining;
    if (m->block_media_kind &&
        !strela_timing_charge_word(m,m->block_media_kind==2)) return;
    if (m->block_opcode==060 && !m->block_remaining &&
        m->block_recorded!=m->block_calculated) {
        m->display_a=m->block_recorded;
        m->display_b=m->block_calculated;
        m->status=STRELA_CHECKSUM_MISMATCH;
    }
}
