#include <stdio.h>
#include <stdint.h>

/* Exercise every index, including holes and fall-through, with a live input
   on both sides of the indirect jump. */
unsigned dispatch(uint8_t key, unsigned value)
{
    switch (key) {
    case 0: value += 3; break;
    case 1: value += 7; break;
    case 2: value += 2; /* fall through */
    case 3: value += 9; break;
    case 4: case 5: value += 13; break;
    case 7: value += 17; break;
    case 8: value += 19; break;
    case 9: value += 23; break;
    case 10: value += 29; break;
    case 11: value += 31; break;
    case 12: value += 37; break;
    case 13: value += 41; break;
    case 14: value += 43; break;
    case 15: value += 47; break;
    default: value += 53;
    }
    return value;
}

int signed_cases(int key)
{
    switch (key) {
    case -4: return 1;
    case -3: return 2;
    case -2: return 3;
    case 0: return 4;
    case 1: return 5;
    case 2: return 6;
    }
    return 0;
}

int wide_cases(int32_t key)
{
    switch (key) {
    case 2147483643L: return 1;
    case 2147483644L: return 2;
    case 2147483645L: return 3;
    case 2147483646L: return 4;
    case 2147483647L: return 5;
    default: return 0;
    }
}

int main(void)
{
    static volatile int keys[] = {-4, -1, 2, 252};
    static volatile int32_t wide[] = {2147483643L, 2147483647L, 65535L, -1L};
    unsigned total = 0;
    for (unsigned i = 0; i < 1024; ++i)
        total = dispatch((uint8_t)(i & 15), total);
    if (total != 64u * (3+7+11+9+13+13+53+17+19+23+29+31+37+41+43+47) ||
        dispatch(255, 1) != 54 || signed_cases(keys[0]) != 1 ||
        signed_cases(keys[1]) || signed_cases(keys[2]) != 6 || signed_cases(keys[3]) ||
        wide_cases(wide[0]) != 1 || wide_cases(wide[1]) != 5 ||
        wide_cases(wide[2]) || wide_cases(wide[3])) {
        puts("FAIL switch dispatch");
        return 1;
    }
    puts("PASS switch dispatch");
    return 0;
}
