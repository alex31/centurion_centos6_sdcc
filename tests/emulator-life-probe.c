#include "../image/programs/life1d/life1d.h"

static uint16_t boards[2][LIFE_WORDS];

int main(void)
{
    uint16_t seed = 1;
    for (;;) {
        /* Vary each initial board so the workload keeps evolving. */
        for (unsigned word = 0; word < LIFE_WORDS; ++word) {
            seed = (seed >> 1) ^ ((seed & 1) ? 0xb400 : 0);
            boards[0][word] = seed;
        }
        boards[0][0] &= 0xfffc;
        boards[0][LIFE_WORDS - 1] &= 7;
        for (unsigned pair = 0; pair < 32; ++pair) {
            life_evolve(boards[1], boards[0]);
            life_evolve(boards[0], boards[1]);
        }
    }
}
