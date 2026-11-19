#include <stdint.h>
#include <stdio.h>

struct record { uint8_t tag; uint32_t value; };
static struct record records[4];
static volatile uint32_t observed;
static uint8_t bytes[256];

static uint32_t update(struct record *p, uint32_t value)
{
    p[-1].value = value ^ 0x80000000UL;
    p[1].value = value & 0x7fffffffUL;
    observed = p[-1].value;
    return p[1].value;
}

static unsigned mix(uint8_t i)
{
    /* The addition wraps as a byte before it becomes an address. */
    return (bytes[i] >> 1) + (bytes[(uint8_t)(i + 1)] >> 2);
}

int main(void)
{
    static volatile uint32_t input = 0x81234567UL;
    static volatile uint8_t index = 255;
    records[0].tag = 19;
    records[2].tag = 41;
    bytes[255] = 251;
    bytes[0] = 168;
    if (update(records + 1, input) != 0x01234567UL || observed != 0x01234567UL ||
        records[0].tag != 19 || records[2].tag != 41 || mix(index) != 167) {
        puts("FAIL indexed accesses");
        return 1;
    }
    input = 0x01234567UL;
    if (update(records + 1, input) != input || observed != 0x81234567UL) {
        puts("FAIL signed-bit masks");
        return 1;
    }
    puts("PASS indexed accesses and promoted bytes");
    return 0;
}
