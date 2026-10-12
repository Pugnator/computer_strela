#include "strela.h"
#include "strela_internal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define SIGN (UINT64_C(1) << 42)
#define MANT (UINT64_C(1) << 34)

int main(void)
{
    uint64_t one, half, tiny, result, maxnum;
    assert(strela_from_double(1.0, &one));
    assert(strela_from_double(0.5, &half));
    assert(strela_from_double(ldexp(1.0, -35), &tiny));
    assert(strela_word_add(one, tiny, 001, &result));
    assert(result == one + (UINT64_C(1) << 7));
    assert(strela_word_add(one, tiny, 012, &result));
    assert(result == one);
    assert(strela_word_add(one, one | SIGN, 001, &result) && result == 0);
    assert(strela_word_multiply(one, one, &result) && result == one);
    assert(strela_word_multiply(one, half, &result) && result == half);
    assert(strela_word_multiply(one | SIGN, half, &result) && result == (half | SIGN));
    assert(strela_word_multiply(one, MANT << 6, &result) && result == 0);
    maxnum = (MANT << 7) | 077;
    assert(!strela_word_add(maxnum, maxnum, 001, &result));
    assert(strela_word_multiply((MANT << 7) | 0100 | 077, half, &result));
    assert(result == 0);
    /* Kitov/Krinitsky, printed pp. 124-127: signed zero and exponent bounds. */
    assert(signbit(strela_to_double(SIGN)));
    assert(strela_word_add(SIGN,0,001,&result) && result==0);
    assert(strela_from_double(ldexp(1.0,62),&result));
    assert((result&077)==077);
    assert(!strela_from_double(ldexp(1.0,63),&result));
    assert(strela_from_double(ldexp(1.0,-65),&result) && result==0);
    assert(strela_word_multiply(0,one,&result) && result==0);
    puts("raw word arithmetic and number boundaries passed");
    return 0;
}
