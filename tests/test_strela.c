#include "strela.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void command(strela_t *m, unsigned where, unsigned a, unsigned b,
                    unsigned c, unsigned opcode)
{
    strela_instruction i = {(uint16_t)a, (uint16_t)b, (uint16_t)c, 0,
                            (uint8_t)opcode};
    assert(strela_write(m, (uint16_t)where, strela_pack(i)));
}

static uint64_t at(strela_t *m, unsigned address)
{
    uint64_t word = 0;
    assert(strela_read(m, (uint16_t)address, &word));
    return word;
}

typedef struct { uint64_t input[12], output[12]; } cards_t;

static bool read_card(void *user, uint64_t *words, unsigned count)
{
    cards_t *cards = (cards_t *)user;
    unsigned i;
    if (count > 12) return false;
    for (i = 0; i < count; ++i) words[i] = cards->input[i];
    return true;
}

static bool write_card(void *user, const uint64_t *words, unsigned count)
{
    cards_t *cards = (cards_t *)user;
    unsigned i;
    if (count > 12) return false;
    for (i = 0; i < count; ++i) cards->output[i] = words[i];
    return true;
}

int main(void)
{
    strela_t m;
    uint64_t one, negative;
    strela_instruction decoded;
    strela_init(&m);
    assert(strela_from_double(1.0, &one));
    assert(at(&m, 07400) == one);
    assert(strela_from_double(-2.5, &negative));
    assert(fabs(strela_to_double(negative) + 2.5) < 1e-10);
    assert(strela_write(&m, 0, one) && at(&m, 0) == 0);
    assert(!strela_write(&m, 07400, one));
    command(&m, 010, 0100, 07423, 0101, 002);
    command(&m, 011, 0101, 07446, 0102, 011);
    command(&m, 012, 0102, 0100, 0103, 016);
    command(&m, 013, 015, 016, 0, 020);
    command(&m, 015, 0, 0, 0, 040);
    command(&m, 016, 0, 0, 0, 040);
    command(&m, 0100, 0100, 0200, 0300, 013);
    m.pc = 010;
    assert(strela_step(&m) == STRELA_RUNNING);
    decoded = strela_unpack(at(&m, 0101));
    assert(decoded.a == 0101 && decoded.b == 0200 && decoded.c == 0300);
    assert(strela_step(&m) == STRELA_RUNNING);
    assert(strela_unpack(at(&m, 0102)).a == 0101);
    assert(strela_step(&m) == STRELA_RUNNING && m.omega == 1);
    assert(strela_step(&m) == STRELA_RUNNING && m.pc == 016);
    assert(strela_step(&m) == STRELA_STOPPED);
    assert(m.cycles == 5);

    strela_destroy(&m);
    strela_init(&m);
    command(&m, 010, 0, 2, 0, 034);
    command(&m, 011, 0100, 0101, 0101, 001);
    command(&m, 012, 0, 0, 0, 040);
    assert(strela_from_double(0.5, &one));
    strela_write(&m, 0100, one);
    strela_write(&m, 0101, 0);
    strela_write(&m, 0102, one);
    strela_write(&m, 0103, one);
    m.pc = 010;
    assert(strela_step(&m) == STRELA_RUNNING);
    assert(m.pc == 012);
    assert(m.cycles == 4);
    assert(fabs(strela_to_double(at(&m, 0101)) - 1.5) < 1e-9);
    assert(strela_step(&m) == STRELA_STOPPED);

    strela_destroy(&m);
    strela_init(&m);
    command(&m, 010, 0100, 0101, 0102, 017);
    strela_write(&m, 0100, STRELA_WORD_MASK);
    strela_write(&m, 0101, 1);
    m.pc = 010;
    assert(strela_step(&m) == STRELA_RUNNING);
    assert(at(&m, 0102) == 1);

    strela_destroy(&m);
    strela_init(&m);
    /* Kitov & Krinitsky, section 28, printed p. 135: isolate address III. */
    command(&m, 0020, 0125, 0675, 0115, 001);
    command(&m, 0376, 0, 0, 07777, 0);
    command(&m, 010, 0020, 0376, 0500, 011);
    m.pc = 010;
    assert(strela_step(&m) == STRELA_RUNNING);
    decoded = strela_unpack(at(&m, 0500));
    assert(decoded.a == 0 && decoded.b == 0 && decoded.c == 0115);
    assert(decoded.check == 0 && decoded.opcode == 0);

    strela_destroy(&m);
    strela_init(&m);
    {
        cards_t cards = {{0}, {0}};
        cards.input[0] = 012345;
        cards.input[1] = 0670;
        m.card_read = read_card;
        m.card_write = write_card;
        m.card_user = &cards;
        command(&m, 010, 0, 1, 0100, 041);
        command(&m, 011, 04001, 1, 0100, 046);
        command(&m, 012, 0100, 1, 0200, 045);
        command(&m, 013, 04001, 1, 0300, 043);
        command(&m, 014, 0300, 1, 0, 044);
        m.pc = 010;
        assert(strela_step(&m) == STRELA_RUNNING);
        assert(at(&m, 0100) == cards.input[0]);
        assert(strela_step(&m) == STRELA_RUNNING);
        assert(strela_step(&m) == STRELA_RUNNING);
        assert(strela_step(&m) == STRELA_RUNNING);
        assert(strela_step(&m) == STRELA_RUNNING);
        assert(cards.output[0] == cards.input[0]);
        assert(cards.output[1] == cards.input[1]);
        assert(m.cycles == 10);
    }
    strela_destroy(&m);

    strela_init(&m);
    assert(strela_from_double(2.0, &one));
    strela_write(&m, 0100, one);
    command(&m, 010, 0100, 0, 0101, 062);
    command(&m, 011, 0101, 0, 0102, 070);
    command(&m, 012, 0102, 0, 0103, 072);
    m.pc = 010;
    assert(strela_step(&m) == STRELA_RUNNING);
    assert(fabs(strela_to_double(at(&m, 0101)) - 0.5) < 1e-9);
    assert(m.cycles == 20);
    assert(strela_step(&m) == STRELA_RUNNING && m.cycles == 70);
    assert(strela_step(&m) == STRELA_RUNNING && m.cycles == 120);
    assert(fabs(strela_to_double(at(&m, 0103)) - 0.5) < 1e-8);
    strela_destroy(&m);

    puts("strela core tests passed");
    return 0;
}
