#include <stdio.h>

static volatile unsigned long numerator = 1000000UL;
static volatile unsigned long divisor = 300UL;
static volatile unsigned int a = 23;
static volatile unsigned int b = 17;

int main(void)
{
    if (a * b != 391 || numerator / divisor != 3333UL ||
        numerator % divisor != 100UL)
        return 1;
    puts("PASS");
    return 0;
}
