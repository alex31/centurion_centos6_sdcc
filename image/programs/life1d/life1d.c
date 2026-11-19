#include <stdio.h>
#include "life1d.h"

#ifdef __CPU6_OS6__
#include <os6file.h>
void life_write_line(const char *encoded, unsigned int length);
void life_clock_read(uint32_t *ticks);
#define CRT_BIT 0x80u
#else
#define CRT_BIT 0u
#endif

#if defined(LIFE_SEED) && (LIFE_SEED < 0 || LIFE_SEED > 255)
#error LIFE_SEED must be between 0 and 255
#endif

static uint8_t initial_seed(void)
{
#if defined(LIFE_SEED)
    return LIFE_SEED;
#elif defined(__CPU6_OS6__)
    static FILE_OS6 file;
    static uint8_t record[OS6_RECORD_SIZE];
    static const char name[] = "LIFESEED";
    uint32_t ticks;
    uint8_t seed;
    int status;

    life_clock_read(&ticks);
    seed = (uint8_t)(ticks + (ticks >> 8));
    if (os6_fopen(&file, name, 1, OS6_READ))
        goto unavailable;
    status = os6_fread_sector(&file, record);
    if (os6_fclose(&file))
        goto unavailable;
    if (status == 0) {
        if (record[0] != 'L' || record[1] != '1' ||
            (record[2] ^ record[3]) != 255u)
            goto unavailable;
        /* An 8-bit counter visits every starting seed before wrapping. */
        seed = (uint8_t)(record[2] + 1u);
    } else if (status != 1) {
        goto unavailable; /* Status 1 is EOF on a newly allocated file. */
    }

    if (os6_fopen(&file, name, 1, OS6_WRITE))
        goto unavailable;
    record[0] = 'L';
    record[1] = '1';
    record[2] = seed;
    record[3] = seed ^ 255u;
    status = os6_fwrite_sector(&file, record);
    if (os6_fclose(&file) || status)
        goto unavailable;
    return seed;

unavailable:
    puts("LIFESEED NOT UPDATED");
    return seed;
#else
    return 1;
#endif
}

/* Each word has the native representation of two consecutive characters.
   The low bit denotes the first cell on both CPU6 and the host. */
#define SPACE (' ' | CRT_BIT)
#define STAR  ('*' | CRT_BIT)
#define BAR   ('|' | CRT_BIT)
static const union { unsigned char text[2]; uint16_t word; } glyph_pairs[4] = {
    {{SPACE, SPACE}}, {{STAR, SPACE}}, {{SPACE, STAR}}, {{STAR, STAR}}
};

static inline void format_byte(uint16_t *out, uint8_t cells)
{
    out[0] = glyph_pairs[cells & 3u].word;
    out[1] = glyph_pairs[(cells >> 2) & 3u].word;
    out[2] = glyph_pairs[(cells >> 4) & 3u].word;
    out[3] = glyph_pairs[cells >> 6].word;
}

static void print_board(const uint16_t *board)
{
    /* A leading pad keeps the word stores aligned. The final cell is a
       fixed dead border cell, immediately followed by the right delimiter. */
    static struct {
        unsigned char prefix[2];
        uint16_t pairs[LIFE_CELLS / 2];
        unsigned char suffix[3];
    } line = { .prefix = {0, BAR}, .suffix = {SPACE, BAR, 0} };
    uint16_t *out = line.pairs;
    for (unsigned i = 0; i < LIFE_WORDS - 1; ++i) {
        uint16_t bits = board[i];
        format_byte(out, (uint8_t)bits);
        format_byte(out + 4, (uint8_t)(bits >> 8));
        out += 8;
    }
    uint16_t tail = board[LIFE_WORDS - 1];
    out[0] = glyph_pairs[tail & 3u].word;
    out[1] = glyph_pairs[(tail >> 2) & 1u].word;

#ifdef __CPU6_OS6__
    life_write_line((const char *)&line + 1, LIFE_CELLS + 2);
#else
    puts((const char *)&line + 1);
#endif
}

int main(void)
{
    uint16_t first[LIFE_WORDS];
    uint16_t second[LIFE_WORDS];
    uint16_t *current = first;
    uint16_t *next = second;
    const uint8_t seed = initial_seed();
    uint8_t random = seed;
    unsigned int cell;
    unsigned int generation;
    uint16_t *seed_word = first;
    uint16_t bits = 0, mask = 4u;

    for (cell = 2; cell < LIFE_CELLS - 2; ++cell) {
        random = (uint8_t)(17u * random + 43u);
        if (random < 128u)
            bits |= mask;
        mask <<= 1;
        if (!mask) {
            *seed_word++ = bits;
            bits = 0;
            mask = 1u;
        }
    }
    *seed_word = bits;

    printf("1D LIFE - B23/S24  SEED %u\n", (unsigned int)seed);
    print_board(current);
    for (generation = 0; generation < 26; ++generation) {
        uint16_t *old = current;
        life_evolve(next, current);
        current = next;
        next = old;
        print_board(current);
    }
    puts("DONE");
    return 0;
}
