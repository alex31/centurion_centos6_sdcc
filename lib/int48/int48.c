#include "int48.h"

void u48_from_u32(uint48_t *out, uint32_t value)
{
    out->hi = 0; out->mid = (uint16_t)(value >> 16); out->lo = (uint16_t)value;
}
void i48_from_i32(int48_t *out, int32_t value)
{
    out->hi = value < 0 ? 65535U : 0;
    out->mid = (uint16_t)((uint32_t)value >> 16); out->lo = (uint16_t)value;
}
void u48_copy(uint48_t *out, const uint48_t *value)
{
    out->hi = value->hi; out->mid = value->mid; out->lo = value->lo;
}
void i48_copy(int48_t *out, const int48_t *value)
{
    out->hi = value->hi; out->mid = value->mid; out->lo = value->lo;
}
int u48_cmp(const uint48_t *a, const uint48_t *b)
{
    if (a->hi != b->hi) return a->hi < b->hi ? -1 : 1;
    if (a->mid != b->mid) return a->mid < b->mid ? -1 : 1;
    if (a->lo != b->lo) return a->lo < b->lo ? -1 : 1;
    return 0;
}
int i48_cmp(const int48_t *a, const int48_t *b)
{
    if ((a->hi ^ b->hi) & 32768U) return a->hi & 32768U ? -1 : 1;
    if (a->hi != b->hi) return a->hi < b->hi ? -1 : 1;
    if (a->mid != b->mid) return a->mid < b->mid ? -1 : 1;
    if (a->lo != b->lo) return a->lo < b->lo ? -1 : 1;
    return 0;
}

#define DEFINE_ADD(name, type) \
void name(type *out, const type *a, const type *b) { \
    uint32_t sum = (uint32_t)a->lo + b->lo; \
    out->lo = (uint16_t)sum; \
    sum = (uint32_t)a->mid + b->mid + (sum >> 16); \
    out->mid = (uint16_t)sum; \
    out->hi = a->hi + b->hi + (uint16_t)(sum >> 16); \
}
#define DEFINE_SUB(name, type) \
void name(type *out, const type *a, const type *b) { \
    uint32_t subtrahend = b->lo; uint16_t old = a->lo, borrow; \
    out->lo = old - (uint16_t)subtrahend; borrow = old < subtrahend; \
    subtrahend = (uint32_t)b->mid + borrow; old = a->mid; \
    out->mid = old - (uint16_t)subtrahend; borrow = old < subtrahend; \
    out->hi = a->hi - b->hi - borrow; \
}
#define DEFINE_NEG(name, type) \
void name(type *out, const type *value) { \
    uint16_t lo = 0U - value->lo; \
    uint16_t mid = (uint16_t)~value->mid + (lo == 0); \
    uint16_t hi = (uint16_t)~value->hi + (lo == 0 && mid == 0); \
    out->hi = hi; out->mid = mid; out->lo = lo; \
}
DEFINE_ADD(u48_add, uint48_t)
DEFINE_ADD(i48_add, int48_t)
DEFINE_SUB(u48_sub, uint48_t)
DEFINE_SUB(i48_sub, int48_t)
DEFINE_NEG(u48_neg, uint48_t)
DEFINE_NEG(i48_neg, int48_t)

#define DEFINE_SHL(name, type) \
void name(type *out, const type *value, uint16_t count) { \
    uint16_t hi = value->hi, mid = value->mid, lo = value->lo; \
    if (count >= 48) hi = mid = lo = 0; \
    else { \
        while (count >= 16) { hi = mid; mid = lo; lo = 0; count -= 16; } \
        if (count) { \
            hi = (hi << count) | (mid >> (16 - count)); \
            mid = (mid << count) | (lo >> (16 - count)); lo <<= count; \
        } \
    } \
    out->hi = hi; out->mid = mid; out->lo = lo; \
}
DEFINE_SHL(u48_shl, uint48_t)
DEFINE_SHL(i48_shl, int48_t)

#define DEFINE_SHR(name, type, sign) \
void name(type *out, const type *value, uint16_t count) { \
    uint16_t hi = value->hi, mid = value->mid, lo = value->lo; \
    uint16_t fill = (sign) && (hi & 32768U) ? 65535U : 0; \
    if (count >= 48) hi = mid = lo = fill; \
    else { \
        while (count >= 16) { lo = mid; mid = hi; hi = fill; count -= 16; } \
        if (count) { \
            lo = (lo >> count) | (mid << (16 - count)); \
            mid = (mid >> count) | (hi << (16 - count)); \
            hi = (hi >> count) | (fill << (16 - count)); \
        } \
    } \
    out->hi = hi; out->mid = mid; out->lo = lo; \
}
DEFINE_SHR(u48_shr, uint48_t, 0)
DEFINE_SHR(i48_shr, int48_t, 1)

#ifndef INT48_USE_ASM
void u48_double(uint48_t *value) { u48_shl(value, value, 1); }
uint16_t u48_div2(uint48_t *value)
{
    uint16_t bit = value->lo & 1;
    u48_shr(value, value, 1);
    return bit;
}
void u48_mul(uint48_t *out, const uint48_t *a, const uint48_t *b)
{
    uint48_t left, right, result;
    unsigned int i;
    u48_copy(&left, a); u48_copy(&right, b); u48_from_u32(&result, 0);
    for (i = 0; i < 48; i++) {
        if (u48_div2(&right)) u48_add(&result, &result, &left);
        u48_double(&left);
    }
    u48_copy(out, &result);
}
void i48_mul(int48_t *out, const int48_t *a, const int48_t *b)
{
    uint48_t left, right, result;
    left.hi = a->hi; left.mid = a->mid; left.lo = a->lo;
    right.hi = b->hi; right.mid = b->mid; right.lo = b->lo;
    u48_mul(&result, &left, &right);
    out->hi = result.hi; out->mid = result.mid; out->lo = result.lo;
}
static void divide_general(uint48_t *q, uint48_t *r, const uint48_t *n, const uint48_t *d)
{
    unsigned int i, incoming, carry;
    u48_copy(q, n); u48_from_u32(r, 0);
    for (i = 0; i < 48; i++) {
        incoming = q->hi >> 15; carry = r->hi >> 15;
        u48_double(r); r->lo += incoming; u48_double(q);
        /* The carry is the 49th remainder bit, needed for divisors >= 2^47. */
        if (carry || u48_cmp(r, d) >= 0) { u48_sub(r, r, d); q->lo++; }
    }
}
#else
void u48_divraw(uint48_t *q, uint48_t *r, const uint48_t *n, const uint48_t *d);
#define divide_general u48_divraw
#endif

static int power_bit(const uint48_t *value)
{
    uint16_t word;
    int bit;
    if (!value->hi && !value->mid) { word = value->lo; bit = 0; }
    else if (!value->hi && !value->lo) { word = value->mid; bit = 16; }
    else if (!value->mid && !value->lo) { word = value->hi; bit = 32; }
    else return -1;
    if (!word || (word & (word - 1U))) return -1;
    while (word > 1) { word >>= 1; bit++; }
    return bit;
}

int u48_divmod(uint48_t *q, uint48_t *r, const uint48_t *n, const uint48_t *d)
{
    uint48_t quotient, remainder, divisor;
    int bit;
    if (!d->hi && !d->mid && !d->lo) return INT48_DIVZERO;
    if (q && q == r) return INT48_ALIAS;
    u48_copy(&divisor, d);
    bit = power_bit(&divisor);
    if (bit >= 0) {
        u48_shr(&quotient, n, (uint16_t)bit);
        remainder.hi = remainder.mid = remainder.lo = 0;
        if (bit < 16) remainder.lo = n->lo & ((1U << bit) - 1U);
        else {
            remainder.lo = n->lo; bit -= 16;
            if (bit < 16) remainder.mid = n->mid & ((1U << bit) - 1U);
            else { remainder.mid = n->mid; bit -= 16; remainder.hi = n->hi & ((1U << bit) - 1U); }
        }
    } else divide_general(&quotient, &remainder, n, &divisor);
    if (q) u48_copy(q, &quotient);
    if (r) u48_copy(r, &remainder);
    return INT48_OK;
}

int i48_divmod(int48_t *q, int48_t *r, const int48_t *n, const int48_t *d)
{
    uint48_t numerator, divisor, quotient, remainder;
    uint16_t negative_n = n->hi & 32768U, negative_d = d->hi & 32768U;
    int status;
    if (!d->hi && !d->mid && !d->lo) return INT48_DIVZERO;
    if (q && q == r) return INT48_ALIAS;
    if (n->hi == 32768U && !n->mid && !n->lo &&
        d->hi == 65535U && d->mid == 65535U && d->lo == 65535U) return INT48_OVERFLOW;
    numerator.hi = n->hi; numerator.mid = n->mid; numerator.lo = n->lo;
    divisor.hi = d->hi; divisor.mid = d->mid; divisor.lo = d->lo;
    if (negative_n) u48_neg(&numerator, &numerator);
    if (negative_d) u48_neg(&divisor, &divisor);
    status = u48_divmod(&quotient, &remainder, &numerator, &divisor);
    if (status) return status;
    if (negative_n != negative_d) u48_neg(&quotient, &quotient);
    if (negative_n) u48_neg(&remainder, &remainder);
    if (q) { q->hi = quotient.hi; q->mid = quotient.mid; q->lo = quotient.lo; }
    if (r) { r->hi = remainder.hi; r->mid = remainder.mid; r->lo = remainder.lo; }
    return INT48_OK;
}

unsigned int u48_format_dec(char *out, const uint48_t *value)
{
    static const uint48_t ten = {0, 0, 10};
    uint48_t number, quotient, remainder;
    char reverse[15];
    unsigned int length = 0, i;
    u48_copy(&number, value);
    do {
        u48_divmod(&quotient, &remainder, &number, &ten);
        reverse[length++] = (char)('0' + remainder.lo);
        u48_copy(&number, &quotient);
    } while (number.hi || number.mid || number.lo);
    for (i = 0; i < length; i++) out[i] = reverse[length - 1 - i];
    out[length] = 0;
    return length;
}
unsigned int i48_format_dec(char *out, const int48_t *value)
{
    uint48_t magnitude;
    magnitude.hi = value->hi; magnitude.mid = value->mid; magnitude.lo = value->lo;
    if (value->hi & 32768U) {
        u48_neg(&magnitude, &magnitude); out[0] = '-';
        return 1 + u48_format_dec(out + 1, &magnitude);
    }
    return u48_format_dec(out, &magnitude);
}
