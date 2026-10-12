#include "strela.h"
#include "strela/system.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t command(unsigned a,unsigned b,unsigned c,unsigned op)
{
    strela_instruction i={(uint16_t)a,(uint16_t)b,(uint16_t)c,0,(uint8_t)op};
    return strela_pack(i);
}
static strela_machine *setup(void)
{
    strela_timing_profile p=strela_timing_default();
    strela_machine *m;
    uint64_t deck[24]={0},one,tape=012345;
    unsigned i;
    p.tape_supply_us=3000;
    p.tape_start_us=0; p.tape_word_us=0;
    p.card_start_us=0; p.card_word_us=0;
    m=strela_machine_create(&p);
    assert(m && strela_from_double(1.0,&one));
    for (i=0;i<24;++i) deck[i]=i+1;
    assert(strela_machine_mount_cards(m,deck,24));
    assert(strela_machine_mount_zone(m,04001,&tape,1));
    assert(strela_machine_write_word(m,0006,command(0,0,0300,041)));
    assert(strela_machine_write_word(m,0007,command(04001,0,0,025)));
    assert(strela_machine_write_word(m,0010,command(0,2,0,031)));
    assert(strela_machine_write_word(m,0011,command(0100,0101,0200,001)));
    assert(strela_machine_write_word(m,0012,command(04001,0,0301,043)));
    assert(strela_machine_write_word(m,0013,command(0300,1,0,044)));
    assert(strela_machine_write_word(m,0014,command(0300,0301,0,040)));
    assert(strela_machine_write_word(m,0100,one));
    assert(strela_machine_set_pc(m,0006));
    strela_machine_resume(m);
    return m;
}
static void finish(strela_machine *m)
{
    strela_system_status status;
    do { status=strela_machine_run_for_us(m,100000); }
    while (status==STRELA_SYS_BUDGET);
    assert(status==STRELA_SYS_NORMAL_STOP);
}
static void equal_state(strela_machine *a,strela_machine *b)
{
    strela_machine_state x,y;
    uint64_t va,vb;
    unsigned i;
    strela_machine_read_state(a,&x);
    strela_machine_read_state(b,&y);
    assert(x.pc==y.pc && x.omega==y.omega &&
           x.instructions==y.instructions && x.cycles==y.cycles &&
           x.elapsed_us==y.elapsed_us && x.legacy_reason==y.legacy_reason &&
           x.group_remaining==y.group_remaining &&
           x.pending_events==y.pending_events &&
           x.events_delivered==y.events_delivered &&
           x.tape_supplied[0]==y.tape_supplied[0] &&
           x.display_a==y.display_a && x.display_b==y.display_b);
    assert(strela_machine_card_position(a)==strela_machine_card_position(b));
    assert(strela_machine_punched_words(a)==strela_machine_punched_words(b));
    for (i=0;i<STRELA_RAM_WORDS;++i) {
        assert(strela_machine_read_word(a,(uint16_t)i,&va));
        assert(strela_machine_read_word(b,(uint16_t)i,&vb));
        assert(va==vb);
    }
    for (i=0;i<strela_machine_punched_words(a);++i) {
        assert(strela_machine_read_punched(a,i,&va));
        assert(strela_machine_read_punched(b,i,&vb));
        assert(va==vb);
    }
}
int main(int argc, char **argv)
{
    const char *file="snapshot-test.sts";
    const char *bad="snapshot-corrupt.sts";
    strela_machine *live=setup(),*replay=strela_machine_create(NULL);
    strela_machine_state before,after;
    FILE *f,*out;
    unsigned char buffer[65536];
    size_t n;
    uint64_t sentinel=0777,observed;
    assert(replay);
    assert(strela_machine_step(live)==STRELA_SYS_RUNNING);
    assert(strela_machine_step(live)==STRELA_SYS_RUNNING);
    assert(strela_machine_step(live)==STRELA_SYS_RUNNING);
    assert(strela_machine_step(live)==STRELA_SYS_RUNNING);
    strela_machine_read_state(live,&before);
    assert(before.group_remaining==2 && before.pending_events==1 &&
           before.elapsed_us==2000 && strela_machine_card_position(live)==1);
    assert(strela_machine_save_snapshot(live,file));
    assert(strela_machine_load_snapshot(replay,file));
    equal_state(live,replay);
    finish(live); finish(replay);
    equal_state(live,replay);
    strela_machine_read_state(live,&after);
    assert(after.events_delivered==1 && after.tape_supplied[0]==1 &&
           after.elapsed_us>=4000 && strela_machine_punched_words(live)==2);
    assert(strela_machine_read_word(live,0301,&observed) && observed==012345);
    assert(strela_machine_save_snapshot(live,file));
    assert(strela_machine_load_snapshot(replay,file));
    equal_state(live,replay);

    f=fopen(file,"rb"); assert(f);
    n=fread(buffer,1,sizeof(buffer),f); assert(n>30 && feof(f));
    fclose(f);
    buffer[30]^=1;
    out=fopen(bad,"wb"); assert(out);
    assert(fwrite(buffer,1,n,out)==n && fclose(out)==0);
    strela_machine_read_state(replay,&before);
    assert(!strela_machine_load_snapshot(replay,bad));
    strela_machine_read_state(replay,&after);
    assert(before.elapsed_us==after.elapsed_us && before.pc==after.pc);
    assert(strela_machine_read_word(replay,0301,&observed) && observed==012345);
    buffer[30]^=1;
    buffer[8]=2;
    out=fopen(bad,"wb"); assert(out);
    assert(fwrite(buffer,1,n,out)==n && fclose(out)==0);
    strela_machine_read_state(replay,&before);
    assert(!strela_machine_load_snapshot(replay,bad));
    strela_machine_read_state(replay,&after);
    assert(before.elapsed_us==after.elapsed_us && before.pc==after.pc &&
           before.legacy_reason==after.legacy_reason &&
           before.status==after.status);
    /* The replay has stopped, and a stopped machine accepts operator edits. */
    assert(after.status!=STRELA_SYS_RUNNING);
    assert(strela_machine_write_word(replay,0100,sentinel));
    assert(strela_machine_read_word(replay,0100,&observed) && observed==sentinel);
    remove(file); remove(bad);
    strela_machine_destroy(live); strela_machine_destroy(replay);
    {
        strela_machine *a=strela_machine_create(NULL);
        strela_machine *b=strela_machine_create(NULL);
        uint64_t input[3],expected[3],got;
        unsigned i;
        assert(a && b);
        assert(strela_from_double(2.0,&input[0]));
        assert(strela_from_double(4.0,&input[1]));
        assert(strela_from_double(8.0,&input[2]));
        assert(strela_from_double(0.5,&expected[0]));
        assert(strela_from_double(0.25,&expected[1]));
        assert(strela_from_double(0.125,&expected[2]));
        assert(strela_machine_write_word(a,010,command(0100,2,0200,062)));
        assert(strela_machine_write_word(a,011,command(0,0,0,040)));
        for (i=0;i<3;++i)
            assert(strela_machine_write_word(a,(uint16_t)(0100+i),input[i]));
        assert(strela_machine_set_pc(a,010));
        strela_machine_resume(a);
        assert(strela_machine_step(a)==STRELA_SYS_RUNNING);
        strela_machine_read_state(a,&before);
        assert(before.batch_remaining==2 && before.elapsed_us==10000);
        assert(strela_machine_save_snapshot(a,file));
        assert(strela_machine_load_snapshot(b,file));
        finish(a); finish(b);
        equal_state(a,b);
        for (i=0;i<3;++i) {
            assert(strela_machine_read_word(b,(uint16_t)(0200+i),&got));
            assert(got==expected[i]);
        }
        remove(file);
        strela_machine_destroy(a); strela_machine_destroy(b);
    }
    {
        strela_machine *a=strela_machine_create(NULL);
        strela_machine *b=strela_machine_create(NULL);
        uint64_t deck[24],word;
        unsigned i;
        assert(a && b);
        for (i=0;i<24;++i) deck[i]=i+1;
        assert(strela_machine_mount_cards(a,deck,24));
        assert(strela_machine_write_word(a,010,command(0,014,0100,041)));
        assert(strela_machine_write_word(a,011,command(0100,0114,0,040)));
        assert(strela_machine_set_pc(a,010));
        strela_machine_resume(a);
        assert(strela_machine_step(a)==STRELA_SYS_RUNNING);
        strela_machine_read_state(a,&before);
        assert(before.block_remaining==12 &&
               strela_machine_card_position(a)==1);
        assert(strela_machine_save_snapshot(a,file));
        assert(strela_machine_load_snapshot(b,file));
        equal_state(a,b);
        finish(a); finish(b);
        equal_state(a,b);
        assert(strela_machine_card_position(b)==13);
        assert(strela_machine_read_word(b,0114,&word) && word==13);
        remove(file);
        strela_machine_destroy(a); strela_machine_destroy(b);
    }
    if (argc>1) {
        strela_machine *frozen=strela_machine_create(NULL);
        assert(frozen && strela_machine_load_snapshot(frozen,argv[1]));
        strela_machine_read_state(frozen,&before);
        assert(before.pc==0021 && before.batch_remaining==5 &&
               before.elapsed_us==25000 && before.main_tact_us==220);
        if (argc>2) {
            strela_machine *cli=strela_machine_create(NULL);
            assert(cli && strela_machine_load_snapshot(cli,argv[2]));
            assert(strela_machine_step(frozen)==STRELA_SYS_PAUSED);
            equal_state(frozen,cli);
            strela_machine_destroy(cli);
            assert(strela_machine_load_snapshot(frozen,argv[1]));
        }
        strela_machine_resume(frozen);
        finish(frozen);
        assert(strela_machine_read_word(frozen,0052,&observed) &&
               observed==command(0400,0,0,001));
        strela_machine_destroy(frozen);
    }
    puts("snapshot replay, pending group/event, media and corrupt load passed");
    return 0;
}

