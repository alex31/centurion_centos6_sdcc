#include <cpu6bits.h>
#include <stdint.h>
#include <stdio.h>

_Static_assert(__builtin_popcountll(0x8000000000000001ULL) == 2, "pop64 constant");
_Static_assert(__builtin_clz(1) == 15, "16-bit unsigned int");
_Static_assert(__builtin_clzg((unsigned char)0, -37) == -37, "signed fallback");
_Static_assert(__builtin_stdc_first_leading_zero((unsigned char)0xff) == 0, "no zero");
_Static_assert(__builtin_stdc_rotate_left((unsigned char)0x81, 9) == 3, "rotate width");
_Static_assert(bswap16(0xabcd) == 0xcdab, "swap16 constant");
_Static_assert(bswap32(0x12345678UL) == 0x78563412UL, "swap32 constant");
_Static_assert(_Generic(__builtin_stdc_bit_floor((unsigned char)7), unsigned char: 1, default: 0), "floor type");
_Static_assert(_Generic(__builtin_stdc_bit_width(3UL), unsigned int: 1, default: 0), "width type");
_Static_assert(_Generic(__builtin_stdc_has_single_bit(1U), _Bool: 1, default: 0), "predicate type");

_Static_assert(_Generic(__builtin_stdc_rotate_right((unsigned short)3, 2),
                        unsigned short: 1, default: 0), "preserve short");
_Static_assert(__builtin_stdc_count_zeros(0x1000000000000001ULL) == 62, "exact high word");
static volatile unsigned value_reads, count_reads;
static volatile unsigned long long sequence = 0x0000ffffffffffffULL;
static volatile unsigned rotations = 17;
static volatile signed char negative_byte = -1;
static volatile struct { unsigned int u:4; signed int s:5; } fields;
static volatile uint32_t source = 0x12345678UL;
static uint32_t value(void) { ++value_reads; return source; }
static unsigned count(void) { ++count_reads; return 36; }

int main(void)
{
    if (bswap16(value()) != 0x7856 || bswap32(value()) != 0x78563412UL) return 1;
    if (__builtin_stdc_rotate_left(value(), count()) != 0x23456781UL) return 2;
    if (__builtin_stdc_bit_ceil(value()) != 0x20000000UL) return 3;
    if (__builtin_ctzg(value(), (int)count()) != 3) return 4;
    if (value_reads != 5 || count_reads != 2) return 5;
    source = 0;
    if (__builtin_clzg(value(), -(int)count()) != -36) return 6;
    if (__builtin_popcount(value()) != 0 || count_reads != 3 || value_reads != 7) return 7;
    if (__builtin_stdc_rotate_left((unsigned short)0x8001, rotations) != 3) return 8;
    if (__builtin_popcountll(sequence++) != 48 || sequence != 0x0001000000000000ULL) return 9;
    if (__builtin_ctzll(sequence--) != 48 || sequence != 0x0000ffffffffffffULL) return 10;
    fields.u = 9;
    fields.s = -2;
    if (__builtin_stdc_count_zeros(fields.u) != 2 || __builtin_clrsbg(fields.s) != 3) return 11;
    if (__builtin_stdc_rotate_left(fields.u, rotations) != 3) return 12;
    if (__builtin_popcountll(negative_byte) != 64) return 13;
    if (__builtin_popcount((unsigned long)sequence) != 16) return 14;
    puts("PASS bit builtin contracts");
    return 0;
}
