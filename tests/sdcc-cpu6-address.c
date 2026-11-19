#include <stdint.h>
#include <stdio.h>

static uint16_t words[8];
static uint8_t bytes[8];
static volatile unsigned count = 8;

static unsigned check(unsigned n, unsigned bias)
{
    uint16_t local[8];
    uint8_t *p = bytes + 2;
    uint16_t *q = words + 3;
    unsigned sum = 0;
    for (unsigned i = 0; i < n; ++i) {
        local[i] = bias + i;
        words[i] = 3 * i + bias;
        bytes[i] = (uint8_t)(i + 1);
    }
    for (unsigned i = 0; i < n; ++i) {
        /* A conditional definition must still follow the chosen object. */
        uint16_t *selected = (i & 1) ? local : words;
        sum += selected[i] + p[1] + q[-1];
    }
    if ((uint8_t)(uint16_t)(p + 1) != (uint8_t)(uint16_t)&bytes[3] ||
        (uint8_t *)q - (uint8_t *)words != 6)
        return 0;
    return sum;
}

int main(void)
{
    /* Even indices contribute 3*i, odd indices contribute i. Each adds
       two copies of bias, bytes[3] (4), and the offset in words[2] (6). */
    if (check(count, 5) != 212 || check(count, 21) != 468) {
        puts("FAIL address rematerialization");
        return 1;
    }
    puts("PASS address rematerialization");
    return 0;
}
