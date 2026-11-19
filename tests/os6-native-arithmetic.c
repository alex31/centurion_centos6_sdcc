#include <stdint.h>
#include <stdio.h>

static volatile uint16_t u = 65535U, v = 32768U;
static volatile int16_t n = -32768, d = -32767;
static volatile uint32_t wide = 0xffffffffUL;

int main(void)
{
    /* Exercise a full product with F set, a truncated zero product, quotient
       and remainder, signed endpoints, and the three-product long expansion. */
    if ((uint32_t)u * (uint32_t)u != 0xfffe0001UL ||
        (uint16_t)(v * 2U) != 0 || u / v != 1 || u % v != 32767U ||
        n / d != 1 || n % d != -1 || wide * wide != 1UL) {
        puts("OS6 NATIVE ARITHMETIC FAIL");
        return 1;
    }
    puts("OS6 NATIVE ARITHMETIC PASS");
    return 0;
}
