#ifndef LIFE1D_H
#define LIFE1D_H

#include <stdint.h>

enum { LIFE_CELLS = 69, LIFE_WORDS = (LIFE_CELLS + 15) / 16 };

/* Separate buffers keep the update synchronous. Bit 0 is the leftmost cell
   in each word. The outer two cells at each end stay dead. */
void life_evolve(uint16_t *next, const uint16_t *current);

#endif
