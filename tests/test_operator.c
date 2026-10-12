#include "strela.h"
#include "strela/system.h"
#include <assert.h>
#include <stdio.h>

static uint64_t command(unsigned a,unsigned b,unsigned c,unsigned check,unsigned op)
{
    strela_instruction i={(uint16_t)a,(uint16_t)b,(uint16_t)c,
                          (uint8_t)check,(uint8_t)op};
    return strela_pack(i);
}
int main(void)
{
    strela_machine *m=strela_machine_create(NULL);
    strela_machine_state s;
    uint64_t one=0,two=0,w=0;
    assert(m && strela_from_double(1.0,&one) && strela_from_double(2.0,&two));
    assert(strela_machine_write_word(m,010,command(0,1,0,0,031)));
    assert(strela_machine_write_word(m,011,command(0100,0101,0200,0,001)));
    assert(strela_machine_write_word(m,012,command(0200,0201,0,0,040)));
    assert(strela_machine_write_word(m,0100,one));
    assert(strela_machine_write_word(m,0101,0));
    assert(strela_machine_set_pc(m,010));
    assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
    strela_machine_read_state(m,&s);
    assert(s.group_remaining==2 && s.pc==012 && s.elapsed_us==500);
    assert(strela_machine_write_word(m,0101,0));
    assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
    assert(strela_machine_read_word(m,0200,&w) && w==one);
    assert(strela_machine_write_word(m,0101,one));
    assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
    assert(strela_machine_read_word(m,0201,&w) && w==two);
    assert(strela_machine_step(m)==STRELA_SYS_NORMAL_STOP);
    strela_machine_read_state(m,&s);
    assert(s.display_a==one && s.display_b==two &&
           s.last_location==012 && s.pc==013 &&
           s.cycles==4 && s.elapsed_us==2000);
    strela_machine_pause(m);
    assert(strela_machine_reset(m,010));
    strela_machine_read_state(m,&s);
    assert(s.cycles==0 && s.group_remaining==0 && s.pc==010);
    assert(strela_machine_write_word(m,010,command(0,1,0,1,031)));
    strela_machine_set_check_stops(m,true);
    strela_machine_resume(m);
    assert(!strela_machine_write_word(m,0101,0));
    assert(strela_machine_step(m)==STRELA_SYS_NORMAL_STOP);
    strela_machine_read_state(m,&s);
    assert(s.legacy_reason==STRELA_CONTROL_STOP &&
           s.group_remaining==2 && s.check_stops);
    strela_machine_resume(m);
    assert(strela_machine_step(m)==STRELA_SYS_RUNNING);
    strela_machine_pause(m);
    strela_machine_destroy(m);
    m=strela_machine_create(NULL);
    assert(m);
    assert(strela_machine_write_word(m,010,command(0100,0101,0200,0,026)));
    assert(strela_machine_write_word(m,0100,one));
    assert(strela_machine_write_word(m,0101,two));
    assert(strela_machine_set_pc(m,010));
    assert(strela_machine_step(m)==STRELA_SYS_FAULT);
    strela_machine_read_state(m,&s);
    assert(s.legacy_reason==STRELA_MISMATCH_STOP &&
           s.display_a==one && s.display_b==two &&
           s.last_location==010 && s.pc==011 && s.omega==1);
    /* A stopped machine accepts operator edits and reset without a pause. */
    assert(strela_machine_write_word(m,0101,one));
    assert(strela_machine_reset(m,010));
    assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
    strela_machine_read_state(m,&s);
    assert(s.legacy_reason==STRELA_RUNNING && s.pc==011 && s.omega==0);
    strela_machine_destroy(m);
    puts("operator pause, step, reset, control and mismatch displays passed");
    return 0;
}

