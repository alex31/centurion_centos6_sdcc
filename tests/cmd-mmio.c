#include <stdio.h>
#include <system_mmio.h>

#define CMD_DATA CENTURION_MMIO8(CENTURION_CMD_BASE)
#define CMD_STATUS CENTURION_MMIO8(CENTURION_CMD_BASE + 1)
#define MMIO(address) CENTURION_MMIO8(address)

void finch_dma_arm(const void *buffer, uint16_t count);
uint16_t finch_dma_stop(void);

static uint8_t source[178], destination[178];

static int await_status(uint8_t mask, uint8_t expected)
{
    uint32_t limit = 300000;
    do {
        if ((CMD_STATUS & mask) == expected)
            return 0;
    } while (--limit);
    return -1;
}

static int result(void)
{
    return await_status(11, 1) ? -1 : CMD_DATA;
}

static int parameter(uint8_t byte)
{
    CMD_DATA = byte;
    return await_status(2, 0);
}

static int address_and_count(uint16_t address, uint16_t count)
{
    return parameter(address >> 8) || parameter(address) || parameter(count >> 8) || parameter(count);
}

static int transfer(uint8_t operation, uint16_t address, void *buffer, uint16_t count)
{
    int status;
    finch_dma_arm(buffer, (uint16_t)~count);
    CMD_DATA = operation;
    if (operation != 0x43 && address_and_count(address, count)) {
        finch_dma_stop();
        return -1;
    }
    status = result();
    if (finch_dma_stop() != 0xffffu)
        return -1;
    return status;
}

static int reject(uint8_t operation, uint16_t address, uint16_t length, int expected)
{
    CMD_DATA = operation;
    return address_and_count(address, length) || result() != expected;
}

static int check(void)
{
    unsigned i;
    if (MMIO(CENTURION_PRT_BASE + 1) != 31 || MMIO(0xf701u) != 0x2d || MMIO(0xf702u) != 0xb3)
        return 1;
    MMIO(0xf700u) = 0x53;
    if (MMIO(0xf700u) != 0x53 || MMIO(CENTURION_DIAG_SWITCHES_ADDR) != 0x0d)
        return 2;
    MMIO(CENTURION_DIAG_HEX_DISPLAY_ADDR) = 0x56;
    MMIO(CENTURION_DIAG_DECIMAL_SET_ADDR(2)) = 0;
    MMIO(CENTURION_DIAG_BLANK_ADDR) = 0;
    MMIO(CENTURION_PRT_BASE) = 'P';
    MMIO(CENTURION_PRT_BASE) = '\f';
    CMD_DATA = 0;
    if (result() != 0)
        return 3;
    CMD_DATA = 0xff;
    if (result() != 0x12)
        return 4;
    if (reject(0x46, 0x1000, 1, 0x26) || reject(0x47, 0, 0x1000, 0x29))
        return 5;
    for (i = 0; i < sizeof source; ++i)
        source[i] = (uint8_t)(i ^ 0xa5);
    if (transfer(0x43, 0, source, sizeof source) || transfer(0x47, 0x11, destination, sizeof destination))
        return 6;
    for (i = 0; i < sizeof source; ++i) {
        if (destination[i] != source[i])
            return 7;
        source[i] ^= 0x55;
    }
    if (transfer(0x46, 0xff0, source, sizeof source) || transfer(0x47, 0xff0, destination, sizeof destination))
        return 8;
    for (i = 0; i < sizeof source; ++i) {
        if (destination[i] != source[i])
            return 9;
    }
    CMD_DATA = 0x46;
    CMD_STATUS = 0;
    if (await_status(8, 0))
        return 10;
    return 0;
}

int main(void)
{
    const int status = check();
    if (status) {
        if (status >= 10)
            putchar('1');
        putchar('0' + status % 10);
        puts(" FAIL CMD");
    } else {
        puts("PASS CMD");
    }
    return status;
}
