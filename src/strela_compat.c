#include "strela_internal.h"
#include <stdlib.h>
#include <string.h>

/* The original callback contract transfers a requested batch in one call.
   The system engine commits media words separately, so this adapter stages
   one legacy command's callback data and flushes its punched batch at end. */
strela_status strela_step(strela_t *m)
{
    strela_status status;
    strela_instruction i;
    bool stage_read=false,stage_write=false;
    strela_card_write_fn old_write=NULL;
    size_t output_start=0;
    if (!m) return STRELA_BAD_ADDRESS;
    if (m->status!=STRELA_RUNNING) return m->status;
    if (m->pc<STRELA_RAM_WORDS && !m->group_remaining &&
        !m->batch_remaining && !m->block_remaining) {
        i=strela_unpack(m->ram[m->pc]);
        stage_read=m->card_read && !m->card_mounted &&
                   (i.opcode==041 || (i.opcode==060 && i.a==0));
        stage_write=m->card_write &&
                    (i.opcode==044 || (i.opcode==060 && i.c==0));
        if (stage_read) {
            unsigned count=(unsigned)i.b+1;
            unsigned padded=(count+11)/12*12;
            uint64_t *words=(uint64_t *)calloc(padded,sizeof(*words));
            bool ok;
            if (!words) { m->status=STRELA_IO_UNAVAILABLE; return m->status; }
            ok=m->card_read(m->card_user,words,count) &&
               strela_media_mount_cards(m,words,padded);
            free(words);
            if (!ok) { m->status=STRELA_MEDIA_EXHAUSTED; return m->status; }
        }
        if (stage_write) {
            old_write=m->card_write;
            m->card_write=NULL;
            output_start=m->card_output_words;
        }
    }
    status=strela_step_boundary(m);
    while (status==STRELA_RUNNING &&
           (m->group_remaining || m->batch_remaining || m->block_remaining))
        status=strela_step_boundary(m);
    if (stage_write) {
        size_t count=m->card_output_words-output_start;
        m->card_write=old_write;
        if (count && !old_write(m->card_user,m->card_output+output_start,
                                (unsigned)count)) {
            m->status=STRELA_IO_UNAVAILABLE;
            status=m->status;
        }
        m->card_output_words=output_start;
    }
    if (stage_read) {
        free(m->card_deck);
        m->card_deck=NULL;
        m->card_deck_words=0;
        m->card_position=0;
        m->card_mounted=false;
    }
    return status;
}
