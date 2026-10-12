#include "strela.h"
#include "strela/system.h"
#include "strela_internal.h"
#include <assert.h>
#include <stdio.h>

static uint64_t command(unsigned a, unsigned b, unsigned c, unsigned op)
{
    strela_instruction i={(uint16_t)a,(uint16_t)b,(uint16_t)c,0,(uint8_t)op};
    return strela_pack(i);
}
int main(void)
{
    strela_timing_profile profile=strela_timing_default();
    strela_machine_state state;
    strela_machine *m;
    uint64_t tape_word=012345, observed;
    profile.tape_supply_us=2000;
    profile.tape_start_us=0;
    profile.tape_word_us=0;
    m=strela_machine_create(&profile);
    assert(m);
    assert(strela_machine_mount_zone(m,04001,&tape_word,1));
    assert(strela_machine_write_word(m,010,command(04001,0,0,025)));
    assert(strela_machine_write_word(m,011,command(0,0,0,000)));
    assert(strela_machine_write_word(m,012,command(04001,0,0100,043)));
    assert(strela_machine_write_word(m,013,command(0100,0,0,040)));
    assert(strela_machine_set_pc(m,010));
    strela_machine_resume(m);
    assert(strela_machine_step(m)==STRELA_SYS_RUNNING);
    strela_machine_read_state(m,&state);
    assert(state.timing_estimate_flags==7);
    assert(state.elapsed_us==500 && state.pending_events==1 &&
           state.tape_supplied[0]==0 && state.main_tact_us==220);
    assert(strela_machine_run_for_us(m,750)==STRELA_SYS_BUDGET);
    strela_machine_read_state(m,&state);
    assert(state.elapsed_us==3000 && state.pending_events==0 &&
           state.tape_supplied[0]==1 && state.events_delivered==1);
    assert(strela_machine_read_word(m,0100,&observed) && observed==tape_word);
    assert(strela_machine_step(m)==STRELA_SYS_NORMAL_STOP);
    assert(state.instruction_us==500);
    strela_machine_destroy(m);

    profile=strela_timing_default();
    profile.instruction_us=220;
    profile.card_start_us=profile.card_word_us=0;
    m=strela_machine_create(&profile);
    assert(m);
    assert(strela_machine_write_word(m,010,command(0,0,0,000)));
    assert(strela_machine_write_word(m,011,command(0,0,0,040)));
    assert(strela_machine_set_pc(m,010));
    strela_machine_resume(m);
    assert(strela_machine_step(m)==STRELA_SYS_RUNNING);
    assert(strela_machine_step(m)==STRELA_SYS_NORMAL_STOP);
    strela_machine_read_state(m,&state);
    assert(state.elapsed_us==440 && state.main_tact_us==220);
    strela_machine_destroy(m);
    {
        strela_t legacy;
        strela_init(&legacy);
        assert(strela_timing_schedule_supply(&legacy,0,1));
        assert(strela_timing_schedule_supply(&legacy,1,2));
        assert(legacy.event_count==2 && legacy.events[0].unit==0 &&
               legacy.events[1].unit==1 &&
               legacy.events[0].due_us==legacy.events[1].due_us);
        legacy.elapsed_us=legacy.events[0].due_us;
        strela_timing_deliver_due(&legacy);
        assert(legacy.events_delivered==2 && legacy.tape_supplied[0]==1 &&
               legacy.tape_supplied[1]==2);
        strela_destroy(&legacy);
    }
    {
        strela_timing_profile p=strela_timing_default();
        uint64_t deck[24]={0};
        p.card_start_us=10; p.card_word_us=1;
        m=strela_machine_create(&p);
        assert(m);
        assert(strela_machine_mount_cards(m,deck,24));
        assert(strela_machine_write_word(m,010,command(0,014,0100,041)));
        assert(strela_machine_write_word(m,011,command(0,0,0,040)));
        assert(strela_machine_set_pc(m,010));
        strela_machine_resume(m);
        assert(strela_machine_step(m)==STRELA_SYS_RUNNING);
        strela_machine_read_state(m,&state);
        assert(state.elapsed_us==511 && state.block_remaining==12);
        assert(strela_machine_run_for_us(m,750)==STRELA_SYS_BUDGET);
        strela_machine_read_state(m,&state);
        assert(state.elapsed_us==1513 && state.block_remaining==10 &&
               strela_machine_card_position(m)==3);
        strela_machine_destroy(m);
    }
    puts("timing profiles, event queue and per-word block budget passed");
    return 0;
}
