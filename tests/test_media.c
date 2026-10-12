#include "strela.h"
#include "strela_internal.h"
#include <assert.h>
#include <stdio.h>

static void command(strela_t *m, unsigned at, unsigned a, unsigned b,
                    unsigned c, unsigned opcode)
{
    strela_instruction i={(uint16_t)a,(uint16_t)b,(uint16_t)c,0,(uint8_t)opcode};
    assert(strela_write(m,(uint16_t)at,strela_pack(i)));
}
int main(void)
{
    strela_t m;
    uint64_t deck[24], one, two;
    unsigned n;
    for (n=0;n<24;++n) deck[n]=n+1;
    strela_init(&m);
    assert(strela_media_mount_cards(&m,deck,24));
    assert(!strela_media_mount_cards(&m,deck,13));
    command(&m,010,0,014,0100,041);
    command(&m,011,0,013,0200,041);
    m.pc=010;
    assert(strela_step(&m)==STRELA_RUNNING);
    assert(m.card_position==13 && m.ram[0100]==1 && m.ram[0114]==13);
    assert(strela_step(&m)==STRELA_MEDIA_EXHAUSTED);
    assert(m.card_position==24 && m.ram[0200]==14 && m.ram[0212]==24);
    assert(m.ram[0213]==0);
    strela_destroy(&m);

    strela_init(&m);
    one=012345; two=07777;
    assert(strela_write(&m,0100,one) && strela_write(&m,0101,two));
    command(&m,010,0100,2,04001,060);
    command(&m,011,04001,2,0200,060);
    command(&m,012,0100,2,05001,060);
    command(&m,013,05001,2,0300,060);
    command(&m,014,0100,2,0,060);
    m.pc=010;
    assert(strela_step(&m)==STRELA_RUNNING);
    assert(m.tape_length[0][1]==3 && m.tape_total_words[0]==3);
    assert(m.tape[0][1][0]==one+two);
    assert(strela_step(&m)==STRELA_RUNNING);
    assert(m.ram[0200]==one+two && m.ram[0201]==one && m.ram[0202]==two);
    assert(strela_step(&m)==STRELA_RUNNING);
    assert(m.tape_length[1][1]==3 && m.tape_total_words[1]==3);
    assert(strela_step(&m)==STRELA_RUNNING);
    assert(m.ram[0301]==one && m.ram[0302]==two);
    assert(strela_step(&m)==STRELA_RUNNING);
    assert(m.card_output_words==3 && m.card_output[0]==one+two);
    assert(strela_media_finalize_output(&m));
    assert(m.card_output_words==12 && m.card_output[11]==0);
    assert(!strela_media_store_zone(&m,0,1,&one,0));
    assert(m.tape_length[0][1]==3 && m.tape_total_words[0]==3);
    m.tape[0][1][0] ^= 1;
    command(&m,015,04001,2,0400,060);
    assert(strela_step(&m)==STRELA_CHECKSUM_MISMATCH);
    assert(m.display_a != m.display_b && m.ram[0401]==one);
    strela_destroy(&m);

    strela_init(&m);
    {
        uint64_t checked_card[12]={012, 5, 5, 0};
        assert(strela_media_mount_cards(&m,checked_card,12));
        command(&m,010,0,2,0200,060);
        m.pc=010;
        assert(strela_step(&m)==STRELA_RUNNING);
        assert(m.ram[0200]==012 && m.ram[0201]==5 && m.ram[0202]==5);
        assert(m.card_position==3);
    }
    strela_destroy(&m);

    strela_init(&m);
    {
        uint64_t bad_card[12]={011, 5, 5, 0};
        assert(strela_media_mount_cards(&m,bad_card,12));
        command(&m,010,0,2,0200,060);
        m.pc=010;
        assert(strela_step(&m)==STRELA_CHECKSUM_MISMATCH);
        assert(m.display_a==011 && m.display_b==012);
        assert(m.ram[0201]==5 && m.ram[0202]==5);
    }
    strela_destroy(&m);

    strela_init(&m);
    assert(strela_write(&m,0100,5) && strela_write(&m,0101,7));
    command(&m,010,0100,2,0101,060);
    m.pc=010;
    assert(strela_step(&m)==STRELA_RUNNING);
    assert(m.ram[0101]==014 && m.ram[0102]==5 && m.ram[0103]==7);
    strela_destroy(&m);

    strela_init(&m);
    command(&m,010,04002,0,0100,043);
    m.pc=010;
    assert(strela_step(&m)==STRELA_MEDIA_ABSENT);
    strela_destroy(&m);
    strela_init(&m);
    {
        uint64_t output[13], loaded;
        strela_t other;
        FILE *bad;
        for (n=0;n<13;++n) output[n]=100+n;
        assert(strela_media_punch(&m,output,13));
        assert(strela_media_save_cards_text(&m,"round_partial.cards"));
        strela_init(&other);
        assert(!strela_media_load_cards_text(&other,"round_partial.cards"));
        assert(!other.card_mounted);
        assert(strela_media_finalize_output(&m));
        assert(strela_media_save_cards_text(&m,"round.cards"));
        assert(strela_media_load_cards_text(&other,"round.cards"));
        assert(other.card_deck_words==24 && other.card_deck[12]==112 &&
               other.card_deck[13]==0);
        assert(strela_media_store_zone(&m,0,1,output,13));
        assert(strela_media_store_zone(&m,1,2,output,13));
        assert(strela_media_save_tape_text(&m,0,"round1.stt"));
        assert(strela_media_save_tape_text(&m,1,"round2.stt"));
        assert(strela_media_load_tape_text(&other,0,"round1.stt"));
        assert(strela_media_load_tape_text(&other,1,"round2.stt"));
        assert(other.tape_length[0][1]==13 && other.tape_length[1][2]==13);
        assert(other.tape[1][2][12]==112);
        bad=fopen("bad.stt","wb"); assert(bad);
        assert(fputs("STRELA-TAPE 1 UNIT=1 CAPACITY=HISTORICAL-100000\n"
                     "ZONE 4001 1\nWORD 0000 0000 0000 0 00\n"
                     "ZONE 4001 1\nWORD 0000 0000 0000 0 00\n",bad)>=0);
        assert(fclose(bad)==0);
        assert(!strela_media_load_tape_text(&other,0,"bad.stt"));
        assert(other.tape_length[0][1]==13 && other.tape[0][1][12]==112);
        bad=fopen("bad.stt","wb"); assert(bad);
        assert(fputs("STRELA-TAPE 1 UNIT=1 CAPACITY=HISTORICAL-100000\\n"
                     "ZONE 4001 1\\nWORD 0 0000 0000 0 00\\n",bad)>=0);
        assert(fclose(bad)==0);
        assert(!strela_media_load_tape_text(&other,0,"bad.stt"));
        assert(other.tape_length[0][1]==13 && other.tape[0][1][12]==112);
        assert(strela_media_read_cards(&other,&loaded,1)==1 && loaded==100);
        strela_destroy(&other);
    }
    strela_destroy(&m);
    remove("round_partial.cards"); remove("round.cards");
    remove("round1.stt"); remove("round2.stt"); remove("bad.stt");
    puts("card, tape and checksum tests passed");
    return 0;
}
