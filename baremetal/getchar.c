#include <stdio.h>
#include <system_mmio.h>

int getchar(void)
{
    while (!(CENTURION_MMIO8(CENTURION_MUX_STATUS_ADDR(0, 0)) & CENTURION_MUX_STATUS_RX_READY))
        ;
    return CENTURION_MMIO8(CENTURION_MUX_DATA_ADDR(0, 0));
}
