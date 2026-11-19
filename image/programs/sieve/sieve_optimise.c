/* Fast CPU6 sieve for the 10,000-prime case.
   Default: one byte per odd candidate and pointer-based composite marking.
   SIEVE_PACKED_BITMAP: one bit per odd candidate, with eight-entry mask tables.
   Yield after initialization and each prime's complete marking pass.
   Timing excludes prime enumeration and output. */
#include <stdint.h>
#include <stdio.h>
#ifdef SIEVE_BARE
int printf(const char *format, ...);
void os6_printf_buffered(int enabled);
#endif
#if !defined(SIEVE_HOST) && !defined(SIEVE_BARE)
#include <os6.h>
void sieve_clock_read(uint32_t *value);
#define CHECKPOINT() os6_checkpoint()
#else
#define CHECKPOINT() do { } while (0)
#endif

#ifndef MAX_PRIME
#define MAX_PRIME 10000U
#endif
#ifndef SQRT_PRIME
#define SQRT_PRIME 100U
#endif
#define ODD_COUNT ((MAX_PRIME + 1U) / 2U)

#ifdef SIEVE_PACKED_BITMAP
#define CELL_COUNT ((ODD_COUNT + 7U) / 8U)
#define INITIAL_CELL 255U
static const uint8_t masks[8] = {1,2,4,8,16,32,64,128};
static const uint8_t clear_masks[8] = {254,253,251,247,239,223,191,127};
/* Call with a side-effect-free odd candidate. */
#define IS_PRIME(value) (sieve[(value) >> 4] & masks[((value) >> 1) & 7U])
#else
#define CELL_COUNT ODD_COUNT
#define INITIAL_CELL 1U
#define IS_PRIME(value) (sieve[(value) >> 1] != 0)
#endif
static uint8_t sieve[CELL_COUNT];

#if !defined(SIEVE_HOST) && !defined(SIEVE_BARE)
static uint32_t start_time, end_time;
#endif

int main(void)
{
    unsigned int i, factor, index, count;
    uint8_t *position;
#ifndef SIEVE_PACKED_BITMAP
    unsigned int remaining;
#endif
#if !defined(SIEVE_HOST) && !defined(SIEVE_BARE)
    uint32_t elapsed;
    sieve_clock_read(&start_time);
#endif
    position = sieve;
    while (position != sieve + CELL_COUNT) *position++ = INITIAL_CELL;
    CHECKPOINT();
#ifdef SIEVE_PACKED_BITMAP
    sieve[0] &= 254U; /* One is not prime. */
#else
    sieve[0] = 0;
#endif

    for (factor = 3; factor <= SQRT_PRIME; factor += 2) {
        if (IS_PRIME(factor)) {
            index = (factor * factor) >> 1;
#ifdef SIEVE_PACKED_BITMAP
            while (index < ODD_COUNT) {
                sieve[index >> 3] &= clear_masks[index & 7U];
                index += factor;
            }
#else
            position = sieve + index;
            remaining = (ODD_COUNT - 1U - index) / factor;
            /* Each pointer advance lands inside the array. The final write
               is separate to avoid forming a pointer beyond one-past-end. */
            while (remaining) {
                *position = 0;
                position += factor;
                remaining--;
            }
            *position = 0;
#endif
            CHECKPOINT();
        }
    }
#if !defined(SIEVE_HOST) && !defined(SIEVE_BARE)
    sieve_clock_read(&end_time);
    elapsed = end_time - start_time;
#endif
#ifndef SIEVE_HOST
    os6_printf_buffered(1);
#endif
    printf("      2");
    count = 1;
    for (i = 3; i <= MAX_PRIME; i += 2) {
        if (IS_PRIME(i)) {
            printf(",%u", i);
            count++;
        }
    }
#if !defined(SIEVE_HOST) && !defined(SIEVE_BARE)
    printf("\nCount:%u\nC_SIEVE;%lu.%u;1\n", count,
           (unsigned long)(elapsed / 10UL), (unsigned int)(elapsed % 10UL));
#else
    printf("\nCount:%u\nC_SIEVE;0.0;1\n", count);
#endif
#ifndef SIEVE_HOST
    os6_printf_buffered(0);
#endif
#if MAX_PRIME == 10000
    if (count != 1229) { puts("SIEVE FAIL"); return 1; }
#endif
#ifdef SIEVE_BARE
    puts("PASS sieve bitmap");
#endif
    return 0;
}
