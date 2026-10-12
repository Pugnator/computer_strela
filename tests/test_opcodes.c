#include "strela.h"
#include "strela/system.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static uint64_t cmd(unsigned a,unsigned b,unsigned c,unsigned check,unsigned op)
{
    strela_instruction i={(uint16_t)a,(uint16_t)b,(uint16_t)c,
                          (uint8_t)check,(uint8_t)op};
    return strela_pack(i);
}
static uint64_t execute(unsigned op,uint64_t a,uint64_t b,
                        strela_machine_state *state)
{
    strela_machine *m=strela_machine_create(NULL);
    uint64_t output=0;
    assert(m);
    assert(strela_machine_write_word(m,010,cmd(0100,0101,0200,0,op)));
    assert(strela_machine_write_word(m,0100,a));
    assert(strela_machine_write_word(m,0101,b));
    assert(strela_machine_set_pc(m,010));
    assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
    assert(strela_machine_read_word(m,0200,&output));
    strela_machine_read_state(m,state);
    strela_machine_destroy(m);
    return output;
}
static void close_to(uint64_t word,double expected)
{
    assert(fabs(strela_to_double(word)-expected)<1e-8);
}
int main(void)
{
    strela_machine_state state;
    uint64_t one,two,four,minus_one,w,second;
    unsigned op;
    assert(strela_from_double(1.0,&one));
    assert(strela_from_double(2.0,&two));
    assert(strela_from_double(4.0,&four));
    assert(strela_from_double(-1.0,&minus_one));

    {
        uint64_t half,tiny;
        assert(strela_from_double(0.5,&half));
        assert(strela_from_double(ldexp(1.0,-35),&tiny));
        w=execute(001,one,one,&state);
        assert(w==two && state.omega==0);
        w=execute(003,one,two,&state);
        close_to(w,-1.0);
        assert(state.omega==1);
        w=execute(004,one,two,&state);
        close_to(w,-1.0);
        assert(state.omega==1);
        w=execute(005,two,two,&state);
        close_to(w,4.0);
        assert(state.omega==1);
        w=execute(005,one,half,&state);
        close_to(w,0.5);
        assert(state.omega==0);
        w=execute(012,one,tiny,&state);
        assert(w==one && state.omega==0);
    }
    w=execute(002,cmd(07777,0100,0200,1,013),
              cmd(1,2,3,0,077),&state);
    assert(w==cmd(0,0102,0203,1,013) && state.omega==0);
    w=execute(015,cmd(0,0102,0203,1,013),
              cmd(1,2,3,0,077),&state);
    assert(w==cmd(07777,0100,0200,1,013) && state.omega==0);
    w=execute(006,two,four,&state);
    assert((w&077)==5 && state.omega==1);
    w=execute(007,two,four,&state);
    assert((w&0177)==0101 && state.omega==0);
    w=execute(010,two,minus_one,&state);
    close_to(w,-2.0);
    assert(state.omega==1);
    w=execute(013,012,021,&state);
    assert(w==(012|021) && state.omega==0);
    w=execute(014,1,two,&state);
    assert(w==4 && state.omega==0);
    w=execute(016,012,012,&state);
    assert(w==0 && state.omega==0);
    w=execute(016,012,021,&state);
    assert(w==(012^021) && state.omega==1);

    /* Printed section 28: 20 and 27 consume the immediately prior omega. */
    {
        strela_machine *m=strela_machine_create(NULL);
        assert(m);
        assert(strela_machine_write_word(m,010,cmd(0100,0101,0200,0,016)));
        assert(strela_machine_write_word(m,011,cmd(012,014,0210,0,027)));
        assert(strela_machine_write_word(m,0100,1));
        assert(strela_machine_write_word(m,0101,2));
        assert(strela_machine_set_pc(m,010));
        assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
        strela_machine_read_state(m,&state);
        assert(state.omega==1);
        assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
        strela_machine_read_state(m,&state);
        assert(state.pc==014 && state.omega==0);
        assert(strela_machine_read_word(m,0210,&w));
        assert(w==cmd(012,012,0210,0,020));
        strela_machine_destroy(m);
    }
    /* Address-field addition changes the command fetched next. */
    {
        strela_machine *m=strela_machine_create(NULL);
        assert(m);
        assert(strela_machine_write_word(m,010,cmd(011,0101,011,0,002)));
        assert(strela_machine_write_word(m,011,cmd(012,012,0,0,020)));
        assert(strela_machine_write_word(m,013,cmd(0,0,0,0,040)));
        assert(strela_machine_write_word(m,0101,cmd(1,1,0,0,000)));
        assert(strela_machine_set_pc(m,010));
        assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
        assert(strela_machine_read_word(m,011,&w));
        assert(w==cmd(013,013,0,0,020));
        assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
        strela_machine_read_state(m,&state);
        assert(state.pc==013);
        assert(strela_machine_step(m)==STRELA_SYS_NORMAL_STOP);
        strela_machine_destroy(m);
    }
    /* An intervening no-op clears omega before a conditional branch. */
    {
        strela_machine *m=strela_machine_create(NULL);
        assert(m);
        assert(strela_machine_write_word(m,010,cmd(0100,0101,0200,0,016)));
        assert(strela_machine_write_word(m,011,cmd(0,0,0,0,000)));
        assert(strela_machine_write_word(m,012,cmd(014,016,0,0,020)));
        assert(strela_machine_write_word(m,0100,1));
        assert(strela_machine_write_word(m,0101,2));
        assert(strela_machine_set_pc(m,010));
        assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
        strela_machine_read_state(m,&state);
        assert(state.omega==1);
        assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
        strela_machine_read_state(m,&state);
        assert(state.omega==0);
        assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
        strela_machine_read_state(m,&state);
        assert(state.pc==014);
        strela_machine_destroy(m);
    }
    /* All eight documented preliminary group increments. */
    for (op=030;op<=037;++op) {
        strela_t m;
        strela_instruction last;
        strela_init(&m);
        assert(strela_write(&m,010,cmd(0,1,0,0,op)));
        assert(strela_write(&m,011,cmd(0100,0200,0300,0,000)));
        m.pc=010;
        assert(strela_step(&m)==STRELA_RUNNING);
        assert(m.cycles==3 && m.group_remaining==0 && m.pc==012);
        assert(m.ram[011]==cmd(0100,0200,0300,0,000));
        last=strela_unpack(m.group_word);
        assert(last.a==0100+((op&4)?2:0));
        assert(last.b==0200+((op&2)?2:0));
        assert(last.c==0300+((op&1)?2:0));
        strela_destroy(&m);
    }
    /* The ROM-backed functions have declared approximate numeric accuracy. */
    {
        static const struct { unsigned op; double input,expected; } cases[] = {
            {062,2.0,0.5}, {063,4.0,2.0}, {064,0.0,1.0},
            {066,1.0,0.0}, {067,0.0,0.0}, {073,0.0,0.0},
            {074,0.0,0.0}
        };
        unsigned k;
        for (k=0;k<sizeof(cases)/sizeof(cases[0]);++k) {
            strela_machine *m=strela_machine_create(NULL);
            assert(m);
            assert(strela_from_double(cases[k].input,&w));
            assert(strela_machine_write_word(m,010,
                   cmd(0100,0,0200,0,cases[k].op)));
            assert(strela_machine_write_word(m,0100,w));
            assert(strela_machine_set_pc(m,010));
            assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
            assert(strela_machine_read_word(m,0200,&w));
            close_to(w,cases[k].expected);
            if (cases[k].op==074) {
                assert(strela_machine_read_word(m,0002,&second));
                close_to(second,acos(0.0));
            }
            strela_machine_destroy(m);
        }
    }
    puts("opcode arithmetic, control, group and estimated functions passed");
    return 0;
}
