#include "strela.h"
#include <math.h>
#include <stdlib.h>
#define MANTISSA_MASK ((UINT64_C(1) << 35) - 1)
#define MANTISSA_SIGN (UINT64_C(1) << 42)
#define EXPONENT_SIGN (UINT64_C(1) << 6)

uint64_t strela_pack(strela_instruction i)
{
    return (((uint64_t)i.a & 07777) << 31) |
           (((uint64_t)i.b & 07777) << 19) |
           (((uint64_t)i.c & 07777) << 7) |
           (((uint64_t)i.check & 1) << 6) | (i.opcode & 077);
}

strela_instruction strela_unpack(uint64_t word)
{
    strela_instruction i;
    i.a = (uint16_t)((word >> 31) & 07777);
    i.b = (uint16_t)((word >> 19) & 07777);
    i.c = (uint16_t)((word >> 7) & 07777);
    i.check = (uint8_t)((word >> 6) & 1);
    i.opcode = (uint8_t)(word & 077);
    return i;
}

double strela_to_double(uint64_t word)
{
    int exponent = (int)(word & 077);
    double fraction = (double)((word >> 7) & MANTISSA_MASK) / 34359738368.0;
    if (word & EXPONENT_SIGN) exponent = -exponent;
    return ldexp((word & MANTISSA_SIGN) ? -fraction : fraction, exponent);
}

bool strela_from_double(double value, uint64_t *word)
{
    int exponent;
    double fraction, rounded;
    uint64_t mantissa;
    if (!word || !isfinite(value)) return false;
    if (value == 0.0) {
        *word = 0;
        return true;
    }
    fraction = frexp(fabs(value), &exponent);
    rounded = floor(ldexp(fraction, 35) + 0.5);
    mantissa = (uint64_t)rounded;
    if (mantissa == (UINT64_C(1) << 35)) {
        mantissa >>= 1;
        ++exponent;
    }
    if (exponent > 63) return false;
    if (exponent < -63) {
        *word = 0;
        return true;
    }
    *word = ((value < 0.0) ? MANTISSA_SIGN : 0) |
            (mantissa << 7) |
            ((exponent < 0) ? EXPONENT_SIGN : 0) |
            (uint64_t)abs(exponent);
    return true;
}


/* Raw arithmetic keeps the 35-bit mantissa as the source of truth. */
static int word_exponent(uint64_t w)
{
    int e = (int)(w & 077);
    return (w & EXPONENT_SIGN) ? -e : e;
}

static uint64_t word_encode(uint64_t mag, int exponent, int negative)
{
    return ((negative && mag) ? MANTISSA_SIGN : 0) |
           (mag << 7) | ((exponent < 0) ? EXPONENT_SIGN : 0) |
           (uint64_t)(exponent < 0 ? -exponent : exponent);
}

/* Align a 35-bit mantissa to a larger order, as the book describes for
   addition (1959, printed p. 140): digits that leave the 35-digit grid are
   lost. Opcode 12 drops them; 01, 03 and 04 round by adding one to the last
   kept digit when the highest lost digit is one. */
static uint64_t align(uint64_t mantissa, int shift, bool round)
{
    uint64_t kept;
    if (shift <= 0) return mantissa;
    kept = shift >= 35 ? 0 : mantissa >> shift;
    if (round && shift <= 35 && ((mantissa >> (shift - 1)) & 1)) ++kept;
    return kept;
}

bool strela_word_add(uint64_t a, uint64_t b, unsigned opcode, uint64_t *out)
{
    uint64_t ma = (a >> 7) & MANTISSA_MASK;
    uint64_t mb = (b >> 7) & MANTISSA_MASK;
    uint64_t x, y, magnitude;
    int ea = word_exponent(a), eb = word_exponent(b), e;
    int sa = !!(a & MANTISSA_SIGN), sb = !!(b & MANTISSA_SIGN), sign;
    bool round = opcode != 012;
    if (opcode == 003) sb = !sb;
    if (opcode == 004) { sa = 0; sb = 1; }
    /* Orders are equalized to the larger one even when that operand's
       mantissa is zero; cell 7575 (-0 with order 35) relies on this. */
    e = ea > eb ? ea : eb;
    x = align(ma, e - ea, round);
    y = align(mb, e - eb, round);
    if (sa == sb) { magnitude = x + y; sign = sa; }
    else if (x >= y) { magnitude = x - y; sign = sa; }
    else { magnitude = y - x; sign = sb; }
    if (!magnitude) { *out = 0; return true; }
    while (magnitude >= (UINT64_C(1) << 35)) {
        /* Carry out of the mantissa: shift right one place, rounding the
           lost digit like an alignment shift. */
        magnitude = align(magnitude, 1, round);
        ++e;
    }
    while (magnitude < (UINT64_C(1) << 34)) { magnitude <<= 1; --e; }
    if (e > 63) return false;
    if (e < -63) { *out = 0; return true; }
    *out = word_encode(magnitude, e, sign);
    return true;
}

bool strela_word_multiply(uint64_t a, uint64_t b, uint64_t *out)
{
    uint64_t ma = (a >> 7) & MANTISSA_MASK;
    uint64_t mb = (b >> 7) & MANTISSA_MASK;
    uint64_t p00, cross, add, lo, hi, mantissa;
    int e, shift, sign = !!((a ^ b) & MANTISSA_SIGN);
    if (!(ma & (UINT64_C(1) << 34)) || !(mb & (UINT64_C(1) << 34))) {
        *out = 0;
        return true;
    }
    p00 = (ma & UINT64_C(0xffffffff)) * (mb & UINT64_C(0xffffffff));
    cross = (ma >> 32) * (mb & UINT64_C(0xffffffff)) +
            (mb >> 32) * (ma & UINT64_C(0xffffffff));
    add = cross << 32;
    lo = p00 + add;
    hi = (ma >> 32) * (mb >> 32) + (cross >> 32) + (lo < p00);
    shift = (hi & (UINT64_C(1) << 5)) ? 35 : 34;
    e = word_exponent(a) + word_exponent(b) - (shift == 34);
    mantissa = (hi << (64 - shift)) | (lo >> shift);
    if (lo & (UINT64_C(1) << (shift - 1))) ++mantissa;
    if (mantissa >= (UINT64_C(1) << 35)) { mantissa >>= 1; ++e; }
    if (e > 63) return false;
    if (e < -63) { *out = 0; return true; }
    *out = word_encode(mantissa, e, sign);
    return true;
}
