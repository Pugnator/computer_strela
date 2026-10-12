#include "strela.h"
#include "strela/system.h"
#include <assert.h>
#include <stdio.h>

static uint64_t cmd(unsigned a, unsigned b, unsigned c, unsigned check, unsigned op)
{
    strela_instruction i = {(uint16_t)a, (uint16_t)b, (uint16_t)c,
                            (uint8_t)check, (uint8_t)op};
    return strela_pack(i);
}
static uint64_t word_at(strela_machine *m, unsigned a)
{
    uint64_t w = 0;
    assert(strela_machine_read_word(m, (uint16_t)a, &w));
    return w;
}
int main(void)
{
    strela_machine *m = strela_machine_create(NULL);
    strela_machine_state s;
    uint64_t output;
    assert(m);
    strela_machine_read_state(m, &s);
    assert(s.main_tact_us == 220 && s.instruction_us == 500);
    assert(s.status == STRELA_SYS_PAUSED);
    assert(!strela_machine_read_word(m, 07666, &output));
    assert(strela_machine_write_word(m, 010, cmd(0, 2, 0, 0, 035)));
    assert(strela_machine_write_word(m, 011, cmd(0100, 0, 0101, 0, 013)));
    assert(strela_machine_write_word(m, 012, cmd(0103, 0, 0, 0, 040)));
    assert(strela_machine_write_word(m, 0100, 012345));
    assert(strela_machine_set_pc(m, 010));
    strela_machine_resume(m);
    assert(strela_machine_run_for_us(m, 750) == STRELA_SYS_BUDGET);
    strela_machine_read_state(m, &s);
    assert(s.elapsed_us == 1000 && s.group_remaining == 2 && s.pc == 012);
    assert(s.last_location == 011 && word_at(m, 0101) == 012345);
    assert(word_at(m, 011) == cmd(0100, 0, 0101, 0, 013));
    assert(!strela_machine_write_word(m, 0101, 0777));
    strela_machine_pause(m);
    assert(strela_machine_write_word(m, 0101, 0777));
    strela_machine_resume(m);
    assert(strela_machine_step(m) == STRELA_SYS_RUNNING);
    assert(word_at(m, 0102) == 0777);
    assert(strela_machine_step(m) == STRELA_SYS_RUNNING);
    assert(word_at(m, 0103) == 0777);
    assert(strela_machine_step(m) == STRELA_SYS_NORMAL_STOP);
    strela_machine_read_state(m, &s);
    assert(s.group_remaining == 0 && s.display_a == 0777);
    assert(s.elapsed_us == 2500 && s.last_location == 012);
    strela_machine_destroy(m);

    {
        strela_t legacy;
        strela_init(&legacy);
        assert(strela_write(&legacy, 010, cmd(0, 2, 0, 0, 035)));
        assert(strela_write(&legacy, 011, cmd(0100, 0, 0101, 1, 013)));
        assert(strela_write(&legacy, 0100, 012345));
        legacy.check_stops = true;
        legacy.pc = 010;
        assert(strela_step(&legacy) == STRELA_CONTROL_STOP);
        assert(legacy.group_remaining == 2);
        assert(legacy.elapsed_us == 1000);
        strela_destroy(&legacy);
    }
    {
        /* Kitov/Krinitsky (1959), printed p. 141, table 26. */
        static const struct {
            unsigned address,a,b,c,check,opcode;
        } constants[] = {
            {07400,02000,0,0,0,001}, {07423,1,0,0,0,0},
            {07424,0,1,0,0,0}, {07425,0,0,1,0,0},
            {07426,1,1,0,0,0}, {07427,0,1,1,0,0},
            {07430,1,0,1,0,0}, {07431,1,1,1,0,0},
            {07434,07777,07777,07777,0,0},
            {07435,03777,07777,07777,1,077},
            {07436,0,0,0,1,077}, {07437,0,0,0,0,077},
            {07440,0,0,0,1,0},
            {07446,07777,0,0,0,0}, {07447,0,07777,0,0,0},
            {07450,0,0,07777,0,0},
            {07451,07777,07777,0,0,0},
            {07452,0,07777,07777,0,0},
            {07453,07777,0,07777,0,0},
            {07455,02000,0,0,0,002},
            {07461,02000,0,0,0,0},
            {07523,02650,01171,04640,0,001},
            {07560,03110,03755,02420,0,001},
            {07575,04000,0,0,0,043}
        };
        size_t k;
        uint64_t w;
        m=strela_machine_create(NULL);
        assert(m);
        for (k=0;k<sizeof(constants)/sizeof(constants[0]);++k) {
            assert(strela_machine_read_word(m,(uint16_t)constants[k].address,&w));
            assert(w==cmd(constants[k].a,constants[k].b,constants[k].c,
                          constants[k].check,constants[k].opcode));
            assert(!strela_machine_write_word(m,(uint16_t)constants[k].address,0));
        }
        assert(!strela_machine_read_word(m,07666,&w));
        assert(strela_machine_constant_source(m,07400)==STRELA_CONSTANT_BUILTIN);
        assert(strela_machine_constant_source(m,07666)==STRELA_CONSTANT_UNKNOWN);
        assert(strela_machine_set_constant(m,07666,012345));
        assert(strela_machine_read_word(m,07666,&w) && w==012345);
        assert(strela_machine_constant_source(m,07666)==STRELA_CONSTANT_SUPPLIED);
        assert(strela_machine_constant_source(m,0100)==STRELA_CONSTANT_UNKNOWN);
        assert(strela_machine_write_word(m,010,cmd(0,0,0,0,040)));
        assert(strela_machine_set_pc(m,010));
        strela_machine_resume(m);
        assert(strela_machine_step(m)==STRELA_SYS_NORMAL_STOP);
        strela_machine_pause(m);
        assert(!strela_machine_set_constant(m,07667,1));
        strela_machine_destroy(m);
    }
    {
        static const struct { unsigned a,b,c,op; strela_status reason; } cases[] = {
            {0,0,0,077,STRELA_BAD_OPCODE},
            {04000,0,0100,001,STRELA_BAD_ADDRESS},
            {07666,0,0100,001,STRELA_UNKNOWN_CONSTANT},
            {0100,0,0101,063,STRELA_DOMAIN_STOP}
        };
        size_t k;
        uint64_t neg;
        assert(strela_from_double(-1.0,&neg));
        for (k=0;k<sizeof(cases)/sizeof(cases[0]);++k) {
            m=strela_machine_create(NULL);
            assert(m);
            assert(strela_machine_write_word(m,010,
                   cmd(cases[k].a,cases[k].b,cases[k].c,0,cases[k].op)));
            assert(strela_machine_write_word(m,0100,neg));
            assert(strela_machine_set_pc(m,010));
            assert(strela_machine_step(m)==STRELA_SYS_FAULT);
            strela_machine_read_state(m,&s);
            assert(s.legacy_reason==(int)cases[k].reason &&
                   s.last_location==010 && s.pc==011);
            strela_machine_destroy(m);
        }
    }
    {
        uint64_t maxnum=(UINT64_C(1)<<41)|077;
        m=strela_machine_create(NULL);
        assert(m);
        assert(strela_machine_write_word(m,010,cmd(0100,0101,0200,0,001)));
        assert(strela_machine_write_word(m,0100,maxnum));
        assert(strela_machine_write_word(m,0101,maxnum));
        assert(strela_machine_set_pc(m,010));
        assert(strela_machine_step(m)==STRELA_SYS_FAULT);
        strela_machine_read_state(m,&s);
        assert(s.legacy_reason==STRELA_ARITHMETIC_STOP &&
               s.last_location==010 && s.pc==011);
        strela_machine_destroy(m);
    }
    {
        uint64_t quarter,root,reciprocal;
        assert(strela_from_double(0.25,&quarter));
        assert(strela_from_double(0.5,&root));
        assert(strela_from_double(2.0,&reciprocal));
        m=strela_machine_create(NULL);
        assert(m);
        assert(strela_machine_write_word(m,010,cmd(0100,0,0200,0,063)));
        assert(strela_machine_write_word(m,0100,quarter));
        assert(strela_machine_set_pc(m,010));
        assert(strela_machine_step(m)==STRELA_SYS_PAUSED);
        assert(word_at(m,0200)==root && word_at(m,0002)==reciprocal);
        strela_machine_destroy(m);
        m=strela_machine_create(NULL);
        assert(m);
        /* Opcode 72 rejects a nondecimal nibble in the nine-digit BCD view. */
        assert(strela_machine_write_word(m,010,cmd(0100,0,0200,0,072)));
        assert(strela_machine_write_word(m,0100,UINT64_C(0xa)<<38));
        assert(strela_machine_set_pc(m,010));
        assert(strela_machine_step(m)==STRELA_SYS_FAULT);
        strela_machine_read_state(m,&s);
        assert(s.legacy_reason==STRELA_DOMAIN_STOP);
        strela_machine_destroy(m);
    }
    puts("program cursor, stops, CDD and standard functions passed");
    return 0;
}

