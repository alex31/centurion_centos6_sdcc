#ifndef CPU6_INT48_H
#define CPU6_INT48_H
#include <stdint.h>

/* Both forms occupy six big-endian bytes and use the same pointer API.
   CPU6_NATIVE_INT48 requires SDCC --cpu6-bitint48. */
#ifdef CPU6_NATIVE_INT48
typedef unsigned _BitInt(48) uint48_t;
typedef _BitInt(48) int48_t;
#else
typedef struct { uint16_t hi, mid, lo; } uint48_t;
typedef struct { uint16_t hi, mid, lo; } int48_t;
#endif

#define INT48_DECIMAL_SIZE 17
#define INT48_OK 0
#define INT48_DIVZERO 1
#define INT48_OVERFLOW 2
#define INT48_ALIAS 3

void u48_from_u32(uint48_t *out, uint32_t value);
void i48_from_i32(int48_t *out, int32_t value);
void u48_copy(uint48_t *out, const uint48_t *value);
void i48_copy(int48_t *out, const int48_t *value);
int u48_cmp(const uint48_t *a, const uint48_t *b);
int i48_cmp(const int48_t *a, const int48_t *b);
/* Addition, subtraction, negation and multiplication wrap modulo 2^48. */
void u48_add(uint48_t *out, const uint48_t *a, const uint48_t *b);
void i48_add(int48_t *out, const int48_t *a, const int48_t *b);
void u48_sub(uint48_t *out, const uint48_t *a, const uint48_t *b);
void i48_sub(int48_t *out, const int48_t *a, const int48_t *b);
void u48_neg(uint48_t *out, const uint48_t *value);
void i48_neg(int48_t *out, const int48_t *value);
void u48_mul(uint48_t *out, const uint48_t *a, const uint48_t *b);
void i48_mul(int48_t *out, const int48_t *a, const int48_t *b);
/* q/r may be null, and may alias inputs, but may not overlap each other.
   Error leaves outputs unchanged. Signed quotient truncates toward zero;
   remainder has the dividend's sign. INT48_MIN / -1 reports overflow. */
int u48_divmod(uint48_t *q, uint48_t *r, const uint48_t *n, const uint48_t *d);
int i48_divmod(int48_t *q, int48_t *r, const int48_t *n, const int48_t *d);
void u48_shl(uint48_t *out, const uint48_t *value, uint16_t count);
void u48_shr(uint48_t *out, const uint48_t *value, uint16_t count);
void i48_shl(int48_t *out, const int48_t *value, uint16_t count);
void i48_shr(int48_t *out, const int48_t *value, uint16_t count);
/* Specialized in-place arithmetic primitives used by the sieve. */
void u48_double(uint48_t *value);
uint16_t u48_div2(uint48_t *value);
/* out must provide INT48_DECIMAL_SIZE bytes, including the final NUL. */
unsigned int u48_format_dec(char *out, const uint48_t *value);
unsigned int i48_format_dec(char *out, const int48_t *value);
#endif
