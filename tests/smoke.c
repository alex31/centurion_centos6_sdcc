#include <stdio.h>

static volatile unsigned long numerator = 1000000UL;
static volatile unsigned long divisor = 300UL;
static volatile unsigned int a = 23;
static volatile unsigned int b = 17;
static volatile int signed_dividend = -99;
static volatile int signed_divisor = 7;

int main(void)
{
    if (a * b != 391 || numerator / divisor != 3333UL ||
        numerator % divisor != 100UL ||
        signed_dividend / signed_divisor != -14 ||
        signed_dividend % signed_divisor != -1)
        return 1;
    puts("PASS");
    return 0;
}
