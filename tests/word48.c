/* Compatibility names used by both sieve sources. Arithmetic lives in the
   common int48 library, with the same API and implementation choices for
   FCC and SDCC. */
#include "word48.h"

void word48_copy(Word48 *out, const Word48 *in) { u48_copy(out, in); }
void word48_double(Word48 *value) { u48_double(value); }
void word48_subtract(Word48 *value, const Word48 *other) { u48_sub(value, value, other); }
void word48_divide(Word48 *quotient, const Word48 *value, const Word48 *divisor)
{
    /* The sieve always supplies a nonzero divisor. */
    u48_divmod(quotient, 0, value, divisor);
}
unsigned int word48_popbit(Word48 *value) { return u48_div2(value); }
