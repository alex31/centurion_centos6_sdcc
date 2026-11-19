#include "life1d.h"

/* For an eight-cell window, evolve its middle four cells (bits 2..5).
   Each output byte uses the result of two overlapping windows. The second
   table places its result in the upper nibble without a runtime shift. */
#define WINDOW_NIBBLES(E) \
    E(0) E(0) E(0) E(1) E(0) E(0) E(2) E(3) E(0) E(1) E(1) E(1) E(4) E(5) E(7) E(6) \
    E(0) E(1) E(3) E(3) E(2) E(3) E(3) E(2) E(9) E(9) E(11) E(10) E(15) E(14) E(12) E(13) \
    E(0) E(0) E(2) E(3) E(6) E(6) E(6) E(7) E(4) E(5) E(7) E(7) E(6) E(7) E(5) E(4) \
    E(2) E(3) E(3) E(3) E(6) E(7) E(5) E(4) E(15) E(15) E(13) E(12) E(9) E(8) E(10) E(11) \
    E(0) E(0) E(0) E(1) E(4) E(4) E(6) E(7) E(12) E(13) E(13) E(13) E(12) E(13) E(15) E(14) \
    E(8) E(9) E(11) E(11) E(14) E(15) E(15) E(14) E(13) E(13) E(15) E(14) E(11) E(10) E(8) E(9) \
    E(4) E(4) E(6) E(7) E(6) E(6) E(6) E(7) E(12) E(13) E(15) E(15) E(10) E(11) E(9) E(8) \
    E(14) E(15) E(15) E(15) E(10) E(11) E(9) E(8) E(3) E(3) E(1) E(0) E(5) E(4) E(6) E(7) \
    E(0) E(0) E(0) E(1) E(0) E(0) E(2) E(3) E(8) E(9) E(9) E(9) E(12) E(13) E(15) E(14) \
    E(8) E(9) E(11) E(11) E(10) E(11) E(11) E(10) E(9) E(9) E(11) E(10) E(15) E(14) E(12) E(13) \
    E(0) E(0) E(2) E(3) E(6) E(6) E(6) E(7) E(12) E(13) E(15) E(15) E(14) E(15) E(13) E(12) \
    E(10) E(11) E(11) E(11) E(14) E(15) E(13) E(12) E(7) E(7) E(5) E(4) E(1) E(0) E(2) E(3) \
    E(8) E(8) E(8) E(9) E(12) E(12) E(14) E(15) E(12) E(13) E(13) E(13) E(12) E(13) E(15) E(14) \
    E(8) E(9) E(11) E(11) E(14) E(15) E(15) E(14) E(5) E(5) E(7) E(6) E(3) E(2) E(0) E(1) \
    E(12) E(12) E(14) E(15) E(14) E(14) E(14) E(15) E(4) E(5) E(7) E(7) E(2) E(3) E(1) E(0) \
    E(6) E(7) E(7) E(7) E(2) E(3) E(1) E(0) E(11) E(11) E(9) E(8) E(13) E(12) E(14) E(15)

#define LOW_NIBBLE(n)  (n),
#define HIGH_NIBBLE(n) (16u * (n)),
static const uint8_t next_low[256]  = { WINDOW_NIBBLES(LOW_NIBBLE) };
static const uint8_t next_high[256] = { WINDOW_NIBBLES(HIGH_NIBBLE) };
#undef HIGH_NIBBLE
#undef LOW_NIBBLE
#undef WINDOW_NIBBLES

static inline unsigned high_byte_offset(void)
{
#ifdef __SDCC_cpu6
    return 0;
#else
    const uint16_t one = 1;
    return *(const unsigned char *)&one;
#endif
}

static inline void finish_word(unsigned char *dst, unsigned offset, unsigned low_mask,
                               uint8_t high, uint8_t low, uint16_t word, uint16_t right)
{
    uint16_t windows = (word >> 2) | (right << 14);
    uint8_t value = high | next_high[windows >> 8];
    dst[offset + high_byte_offset()] = value;
    value = (low | next_high[windows & 0xffu]) & low_mask;
    dst[offset + (1u - high_byte_offset())] = value;
}

void life_evolve(uint16_t *next, const uint16_t *current)
{
    unsigned char *dst = (unsigned char *)next;
    /* The left-shifted word supplies windows for nibbles 0 and 2. Do those
       lookups before fetching the next word for nibbles 1 and 3. */
    uint16_t w0 = current[0];
    uint16_t windows = w0 << 2;
    uint8_t high = next_low[windows >> 8];
    uint8_t low = next_low[windows & 0xffu];
    uint16_t w1 = current[1];
    finish_word(dst, 0, 0xfcu, high, low, w0, w1);

    windows = (w1 << 2) | (w0 >> 14);
    high = next_low[windows >> 8];
    low = next_low[windows & 0xffu];
    uint16_t w2 = current[2];
    finish_word(dst, 2, 0xffu, high, low, w1, w2);

    windows = (w2 << 2) | (w1 >> 14);
    high = next_low[windows >> 8];
    low = next_low[windows & 0xffu];
    uint16_t w3 = current[3];
    finish_word(dst, 4, 0xffu, high, low, w2, w3);

    windows = (w3 << 2) | (w2 >> 14);
    high = next_low[windows >> 8];
    low = next_low[windows & 0xffu];
    uint16_t w4 = current[4];
    finish_word(dst, 6, 0xffu, high, low, w3, w4);

    /* Only cells 64..66 are interior in the final word. */
    dst[8 + high_byte_offset()] = 0;
    uint8_t tail = next_low[((w4 << 2) | (w3 >> 14)) & 0xffu] & 7u;
    dst[8 + (1u - high_byte_offset())] = tail;
}
