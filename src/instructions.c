#include "strela.h"
#include "strela_internal.h"
#include <math.h>
#include <stdlib.h>
#define MANTISSA_SIGN (UINT64_C(1) << 42)
#define EXPONENT_SIGN (UINT64_C(1) << 6)

static bool read_operand(strela_t *m, uint16_t address, uint64_t *word)
{
    if (strela_read(m, address, word)) return true;
    m->status = (address >= STRELA_CDD_FIRST && address <= STRELA_CDD_LAST) ?
                STRELA_UNKNOWN_CONSTANT : STRELA_BAD_ADDRESS;
    return false;
}

static bool write_result(strela_t *m, uint16_t address, uint64_t word)
{
    if (strela_write(m, address, word)) return true;
    m->status = STRELA_BAD_ADDRESS;
    return false;
}

static bool numeric_result(strela_t *m, uint16_t address, double value,
                           uint8_t flag_kind)
{
    uint64_t word;
    double stored;
    if (!strela_from_double(value, &word)) {
        m->status = STRELA_ARITHMETIC_STOP;
        return false;
    }
    if (!write_result(m, address, word)) return false;
    stored = strela_to_double(word);
    if (flag_kind == 1) m->omega = (stored < 0.0);
    if (flag_kind == 2) m->omega = (fabs(stored) >= 1.0);
    if (flag_kind == 3) m->omega = (stored >= 1.0);
    return true;
}

static bool decimal_to_double(uint64_t word, double *value)
{
    uint64_t digits = (word >> 6) & ((UINT64_C(1) << 36) - 1);
    uint64_t mantissa = 0;
    unsigned place;
    int exponent = (int)(word & 037);
    for (place = 0; place < 9; ++place) {
        unsigned digit = (unsigned)((digits >> (32 - 4 * place)) & 15);
        if (digit > 9) return false;
        mantissa = mantissa * 10 + digit;
    }
    if (word & (UINT64_C(1) << 5)) exponent = -exponent;
    /* Inferred limit: 10^19 is about the binary range 2^63; the five
       exponent bits alone would allow 31. No located source states 19. */
    if (exponent > 19 || exponent < -19) return false;
    *value = ((double)mantissa / 1000000000.0) * pow(10.0, exponent);
    if (word & MANTISSA_SIGN) *value = -*value;
    return true;
}

static bool double_to_decimal(double value, uint64_t *word)
{
    int exponent;
    uint64_t mantissa, digits = 0;
    unsigned place;
    double fraction;
    if (!isfinite(value)) return false;
    if (value == 0.0) { *word = 0; return true; }
    exponent = (int)floor(log10(fabs(value))) + 1;
    if (exponent > 19) return false;
    if (exponent < -19) { *word = 0; return true; }
    fraction = fabs(value) / pow(10.0, exponent);
    mantissa = (uint64_t)floor(fraction * 1000000000.0 + 0.5);
    if (mantissa >= 1000000000) {
        mantissa = 100000000;
        ++exponent;
        if (exponent > 19) return false;
    }
    for (place = 0; place < 9; ++place) {
        digits |= (mantissa % 10) << (place * 4);
        mantissa /= 10;
    }
    *word = (value < 0 ? MANTISSA_SIGN : 0) | (digits << 6) |
            (exponent < 0 ? (UINT64_C(1) << 5) : 0) |
            (uint64_t)abs(exponent);
    return true;
}

void strela_batch_step(strela_t *m, bool first)
{
    unsigned n=m->batch_index,per_value;
    uint64_t output;
    double x,value;
    unsigned op=m->batch_opcode;
    /* Nominal intervals per element: 62-67 from the Carr report reprinted
       by Bell and Newell (approximate); 70-74 are estimates (docs/TIMING.md). */
    switch (op) {
    case 062: per_value=20; break;
    case 063: case 064: per_value=40; break;
    case 066: per_value=60; break;
    case 067: per_value=25; break;
    case 070: case 072: per_value=50; break;
    case 073: case 074: per_value=40; break;
    default: m->status=STRELA_BAD_OPCODE; return;
    }
    if (op==072) {
        if (!decimal_to_double(m->batch_source[n],&x)) {
            m->status=STRELA_DOMAIN_STOP; return;
        }
    } else x=strela_to_double(m->batch_source[n]);
    switch (op) {
    case 062:
        if (x==0.0) { m->status=STRELA_DOMAIN_STOP; return; }
        value=1.0/x; break;
    case 063:
        if (x<0.0) { m->status=STRELA_DOMAIN_STOP; return; }
        value=sqrt(x); break;
    case 064: value=exp(x); break;
    case 066:
        if (x<=0.0) { m->status=STRELA_DOMAIN_STOP; return; }
        value=log(x); break;
    case 067: value=sin(x); break;
    case 070: case 072: value=x; break;
    case 073: value=atan(x); break;
    case 074:
        if (fabs(x)>1.0) { m->status=STRELA_DOMAIN_STOP; return; }
        value=asin(x); break;
    default: return;
    }
    if (op==070) {
        if (!double_to_decimal(value,&output)) {
            m->status=STRELA_ARITHMETIC_STOP; return;
        }
        if (!write_result(m,(uint16_t)(m->batch_c+n),output)) return;
    } else if (!numeric_result(m,(uint16_t)(m->batch_c+n),value,0)) return;
    if (n+1==m->batch_count && op==063 && x>0.0)
        if (!numeric_result(m,0002,1.0/sqrt(x),0)) return;
    if (n+1==m->batch_count && op==074)
        if (!numeric_result(m,0002,acos(x),0)) return;
    m->cycles+=per_value-(first ? 1u : 0u);
    ++m->batch_index;
    --m->batch_remaining;
}
static void standard_function(strela_t *m, strela_instruction i,
                              uint16_t location)
{
    unsigned n,count=(unsigned)i.b+1;
    if (i.a>=STRELA_RAM_WORDS || i.c>=STRELA_RAM_WORDS ||
        (unsigned)i.a+i.b>=STRELA_RAM_WORDS ||
        (unsigned)i.c+i.b>=STRELA_RAM_WORDS) {
        m->status=STRELA_BAD_ADDRESS; return;
    }
    m->batch_word=strela_pack(i);
    m->batch_location=location;
    m->batch_count=(uint16_t)count;
    m->batch_index=0;
    m->batch_remaining=(uint16_t)count;
    m->batch_a=i.a;
    m->batch_c=i.c;
    m->batch_opcode=i.opcode;
    for (n=0;n<count;++n) m->batch_source[n]=m->ram[i.a+n];
    strela_batch_step(m,true);
}

void strela_execute(strela_t *m, strela_instruction i, uint16_t location);

static void group(strela_t *m, strela_instruction preliminary)
{
    uint64_t word;
    strela_instruction main;
    uint16_t location = m->pc;
    if (preliminary.a || location >= STRELA_RAM_WORDS) {
        m->status = STRELA_BAD_ADDRESS; return;
    }
    if (!read_operand(m, location, &word)) return;
    main = strela_unpack(word);
    if (main.opcode > 017) { m->status = STRELA_BAD_OPCODE; return; }
    /* Unverified implementation rule: a nonzero third address of the
       preliminary command receives zero, like opcode 20. The book's group
       examples (printed pp. 139-140) all use c = 0000. */
    if (preliminary.c && !write_result(m, preliminary.c, 0)) return;
    m->pc = (uint16_t)((m->pc + 1) & 07777);
    m->group_word = word;
    m->group_location = location;
    m->group_remaining = (uint16_t)(preliminary.b + 1);
    m->group_increment = preliminary.opcode & 7;
}

void strela_execute(strela_t *m, strela_instruction i, uint16_t location)
{
    uint64_t a = 0, b = 0, result = 0;
    unsigned shift;
    uint8_t previous_omega = m->omega;
    ++m->cycles;
    m->omega = 0;
    switch (i.opcode) {
    case 000: break;
    case 001: case 003: case 004: case 005: case 006: case 007:
    case 010: case 012:
        if (!read_operand(m, i.a, &a) || !read_operand(m, i.b, &b)) return;
        if (i.opcode == 006 || i.opcode == 007) {
            int ea = (int)(a & 077), eb = (int)(b & 077), e;
            if (a & EXPONENT_SIGN) ea = -ea;
            if (b & EXPONENT_SIGN) eb = -eb;
            e = (i.opcode == 006) ? ea + eb : ea - eb;
            if (e > 63) { m->status = STRELA_ARITHMETIC_STOP; return; }
            result = e < -63 ? 0 : (a & ~UINT64_C(127)) |
                     (e < 0 ? EXPONENT_SIGN : 0) | (uint64_t)abs(e);
            if (write_result(m, i.c, result)) m->omega = (e >= 1);
            return;
        }
        if (i.opcode == 010) {
            result = (a & ~MANTISSA_SIGN) | (b & MANTISSA_SIGN);
            if (write_result(m, i.c, result))
                m->omega = ((result & EXPONENT_SIGN) == 0 && (result & 077) >= 1);
            return;
        }
        if (i.opcode == 005) {
            if (!strela_word_multiply(a, b, &result)) {
                m->status = STRELA_ARITHMETIC_STOP; return;
            }
            if (write_result(m, i.c, result))
                m->omega = ((result & EXPONENT_SIGN) == 0 && (result & 077) >= 1 &&
                            ((result >> 7) & ((UINT64_C(1) << 35) - 1)) != 0);
            return;
        }
        if (!strela_word_add(a, b, i.opcode, &result)) {
            m->status = STRELA_ARITHMETIC_STOP; return;
        }
        if (write_result(m, i.c, result))
            m->omega = i.opcode == 012 ? (result == 0) :
                       ((result & MANTISSA_SIGN) != 0);
        return;
    case 002: case 015:
        if (!read_operand(m, i.a, &a) || !read_operand(m, i.b, &b)) return;
        {
            strela_instruction left = strela_unpack(a), right = strela_unpack(b);
            left.a = (left.a + (i.opcode == 002 ? right.a : 010000 - right.a)) & 07777;
            left.b = (left.b + (i.opcode == 002 ? right.b : 010000 - right.b)) & 07777;
            left.c = (left.c + (i.opcode == 002 ? right.c : 010000 - right.c)) & 07777;
            result = strela_pack(left);
        }
        write_result(m, i.c, result);
        return;
    case 011: case 013: case 016: case 017: case 026:
        if (!read_operand(m, i.a, &a) || !read_operand(m, i.b, &b)) return;
        if (i.opcode == 011) result = a & b;
        else if (i.opcode == 013) result = a | b;
        else if (i.opcode == 016 || i.opcode == 026) result = a ^ b;
        else {
            uint64_t sum = a + b;
            result = ((sum & STRELA_WORD_MASK) + (sum >> 43)) & STRELA_WORD_MASK;
        }
        if (!write_result(m, i.c, result)) return;
        if (i.opcode == 011 || i.opcode == 013) m->omega = (result == 0);
        if (i.opcode == 016 || i.opcode == 026) m->omega = (result != 0);
        if (i.opcode == 026 && m->omega) {
            m->display_a = a; m->display_b = b;
            m->status = STRELA_MISMATCH_STOP;
        }
        return;
    case 014:
        if (!read_operand(m, i.a, &a)) return;
        if (i.b < 04000) {
            if (!read_operand(m, i.b, &b)) return;
            shift = (unsigned)(b & 077);
            result = (b & EXPONENT_SIGN) ? (a >> shift) : ((a << shift) & STRELA_WORD_MASK);
        } else {
            unsigned base = i.b & ~077u;
            if (base != 04000 && base != 04100 && base != 05000 && base != 05100) {
                m->status = STRELA_BAD_ADDRESS;
                return;
            }
            shift = i.b & 077;
            result = (base == 04100 || base == 05100) ? (a >> shift) :
                     ((a << shift) & STRELA_WORD_MASK);
        }
        /* Unverified implementation rule: counts above 37 give zero. The 1959
           book, printed p. 136, puts the count in bits 37-42 and shows only
           small shifts; no located source states this cutoff. */
        if (shift > 37) result = 0;
        if (write_result(m, i.c, result)) m->omega = (result == 0);
        return;
    case 020: case 027:
        if (i.a >= STRELA_RAM_WORDS || i.b >= STRELA_RAM_WORDS) {
            m->status = STRELA_BAD_ADDRESS; return;
        }
        m->pc = previous_omega ? i.b : i.a;
        if (i.opcode == 020) result = 0;
        else {
            strela_instruction ret = {(uint16_t)((location + 1) & 07777),
                                      (uint16_t)((location + 1) & 07777), i.c, 0, 020};
            result = strela_pack(ret);
        }
        write_result(m, i.c, result);
        return;
    case 030: case 031: case 032: case 033:
    case 034: case 035: case 036: case 037:
        group(m, i); return;
    case 040:
        if (!read_operand(m, i.a, &m->display_a) ||
            !read_operand(m, i.b, &m->display_b)) return;
        m->status = STRELA_STOPPED; return;
    case 041: case 043: case 044: case 045: case 046: case 060:
        strela_block_start(m,i,location); return;
    case 025:
        {
            unsigned tape, zone;
            if (!strela_tape_address(i.a, &tape, &zone) || i.b || i.c)
                m->status = STRELA_BAD_ADDRESS;
            else strela_timing_schedule_supply(m, tape, zone);
        }
        return;
    case 062: case 063: case 064: case 066: case 067:
    case 070: case 072: case 073: case 074:
        standard_function(m, i, location); return;
    default: m->status = STRELA_BAD_OPCODE; return;
    }
}


