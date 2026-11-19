/* A 17-bit intermediate used to become an unimplemented _BitInt(24). */
#include <stdint.h>
#include <stdio.h>

static int borrow(uint16_t left, uint16_t right, uint16_t carry)
{
    uint32_t subtrahend = (uint32_t)right + carry;
    uint16_t low = left - (uint16_t)subtrahend;
    return low == left && left < subtrahend;
}
static uint32_t add_limb(uint16_t left, uint16_t right, uint16_t carry)
{
    uint32_t result = (uint32_t)left + right + carry;
    return result;
}
int main(void)
{
    if (!borrow(0, 65535U, 1) || !borrow(65535U, 65535U, 1) || borrow(0, 0, 0) ||
        add_limb(65535U, 65535U, 1) != 131071UL || add_limb(65535U, 0, 1) != 65536UL) {
        puts("FAIL intermediate width and carry"); return 1;
    }
    puts("PASS intermediate width and carry"); return 0;
}
