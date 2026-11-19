/* The quotient/remainder oracle uses only increment, comparison and reset.
   It does not repeat the compiler's division or sign-normalization algorithm. */
#include <stdint.h>
#include <stdio.h>

#ifndef CPU6_NATIVE_SWEEP_MAX
#define CPU6_NATIVE_SWEEP_MAX 32768U
#endif

static int quotient(int a, int b) { return a / b; }
static int remainder(int a, int b) { return a % b; }
static int word_condition(unsigned int a, unsigned int b) { return a * b ? 7 : 3; }
static int byte_condition(unsigned int a, unsigned int b) { return (unsigned char)(a * b) ? 7 : 3; }
static unsigned long wide_product(unsigned long a, unsigned long b) { return a * b; }

static int check(int n, int d, uint16_t q, uint16_t r)
{
    return (uint16_t)quotient(n, d) == q && (uint16_t)remainder(n, d) == r;
}

int main(void)
{
    static const uint16_t divisors[] = {1, 3, 47, 32767, 32768U};
    /* Read the operands at runtime so the optimized variants exercise MUL. */
    static const volatile unsigned int words[] = {256, 65535U, 16, 15, 17};
    static const volatile unsigned long wide[] = {65535UL, 0xffffffffUL, 0x10000UL};
    uint16_t i, magnitude, q, r, d, nq, nr;
    if (word_condition(words[0], words[0]) != 3 || word_condition(words[1], words[1]) != 7 ||
        byte_condition(words[2], words[2]) != 3 || byte_condition(words[3], words[4]) != 7 ||
        wide_product(wide[0], wide[0]) != 0xfffe0001UL ||
        wide_product(wide[1], wide[1]) != 1UL ||
        wide_product(wide[2], wide[2]) != 0UL) {
        puts("FAIL native multiply, truncated flags or wide product"); return 1;
    }
    for (i = 0; i < sizeof(divisors) / sizeof(divisors[0]); i++) {
        d = divisors[i]; q = r = 0;
        for (magnitude = 0; magnitude <= CPU6_NATIVE_SWEEP_MAX; magnitude++) {
            nq = 0U - q; nr = 0U - r;
            if (d < 32768U) {
                if (magnitude < 32768U && !check((int)magnitude, (int)d, q, r)) goto fail;
                if (!check((int)(0U - magnitude), (int)d, nq, nr)) goto fail;
            }
            if (magnitude < 32768U && !check((int)magnitude, (int)(0U - d), nq, r)) goto fail;
            /* INT_MIN / -1 has undefined behavior in 16-bit C. */
            if (!(magnitude == 32768U && d == 1) &&
                !check((int)(0U - magnitude), (int)(0U - d), q, nr)) goto fail;
            r++;
            if (r == d) { r = 0; q++; }
        }
    }
    puts("PASS native multiplication and complete signed dividend sweeps");
    return 0;
fail:
    puts("FAIL native signed dividend sweep");
    return 1;
}
