#include <stdint.h>
#include <stdio.h>

static uint32_t values[] = {0UL, 1UL, 0x100UL, 0x10000UL, 0x80000000UL, 0xffffffffUL};

static _Bool boolean(uint32_t value)
{
    return (_Bool)value;
}

int main(void)
{
    unsigned int i;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        if (boolean(values[i]) != (i != 0)) {
            puts("FAIL: long to bool conversion");
            return 1;
        }
    }
    puts("PASS: long to bool conversion");
    return 0;
}
