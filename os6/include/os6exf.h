#ifndef CPU6_OS6EXF_H
#define CPU6_OS6EXF_H

#include <stdint.h>

#define OS6_EXF_SECTOR_SIZE 400

typedef struct {
    uint16_t length;
    uint8_t data[OS6_EXF_SECTOR_SIZE];
} OS6_EXF_BLOCK;

/* The logical unit must already be assigned to an existing type E file.
 * One writer is supported; begin resets its position to sector zero. */
int os6_exf_begin(uint8_t logical_unit);
int os6_exf_write(OS6_EXF_BLOCK *block);

#endif
