#include <system_mmio.h>

/* Polled output on Centurion MUX channel 0. */
int putchar(int c)
{
    while (!(CENTURION_MMIO8(CENTURION_MUX_STATUS_ADDR(0, 0)) & CENTURION_MUX_STATUS_TX_EMPTY))
        ;
    CENTURION_MMIO8(CENTURION_MUX_DATA_ADDR(0, 0)) = (uint8_t)c;
    return c;
}

int puts(const char *s)
{
    while (*s)
        putchar(*s++);
    putchar('\n');
    return 0;
}
