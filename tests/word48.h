#ifndef SIEVE_WORD48_H
#define SIEVE_WORD48_H
#include "../lib/int48/int48.h"

/* Three big-endian limbs; the benchmark uses 47 positive bits, as CPL does. */
typedef uint48_t Word48;
void word48_copy(Word48 *out, const Word48 *in);
void word48_double(Word48 *value);
void word48_subtract(Word48 *value, const Word48 *other);
void word48_divide(Word48 *quotient, const Word48 *value, const Word48 *divisor);
unsigned int word48_popbit(Word48 *value);
#endif
