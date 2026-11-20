#include <system_finch.h>

static uint8_t track[FINCH_TRACK_SIZE];
static uint8_t sector[FINCH_SECTOR_SIZE];

static int matches(const uint8_t *expected)
{
    uint16_t i;
    for (i = 0; i < FINCH_SECTOR_SIZE; ++i) {
        if (sector[i] != expected[i])
            return 0;
    }
    return 1;
}

int main(void)
{
    uint16_t i;
    int status;

    if (finch_init(0) != FINCH_INVALID_ARGUMENT ||
        finch_read_sector(1, FINCH_CYLINDERS, 0, 0, sector) != FINCH_INVALID_ARGUMENT ||
        finch_write_sector(1, 0, 0, FINCH_SECTORS_PER_TRACK, sector) != FINCH_INVALID_ARGUMENT ||
        finch_format_track(1, 0, 0, 0) != FINCH_INVALID_ARGUMENT ||
        finch_format_disk(1, 0, track) != FINCH_INVALID_ARGUMENT)
        return 1;

    status = finch_init(1);
    if (status)
        return 0x100 | (uint8_t)status;

    for (i = 0; i < FINCH_TRACK_SIZE; ++i)
        track[i] = (uint8_t)(i ^ (i >> 8) ^ 0xa6);
    status = finch_format_track(1, 1, 1, track);
    if (status)
        return 0x200 | (uint8_t)status;

    status = finch_read_sector(1, 1, 1, 17, sector);
    if (status)
        return 0x300 | (uint8_t)status;
    if (!matches(track + 17 * FINCH_SECTOR_SIZE))
        return 0x400;

    status = finch_write_sector(1, 1, 1, 17, track);
    if (status)
        return 0x500 | (uint8_t)status;
    status = finch_read_sector(1, 1, 1, 17, sector);
    if (status)
        return 0x600 | (uint8_t)status;
    if (!matches(track))
        return 0x700;

    status = finch_read_sector(1, 1, 1, 18, sector);
    if (status)
        return 0x800 | (uint8_t)status;
    if (!matches(track + 18 * FINCH_SECTOR_SIZE))
        return 0x900;
    return FINCH_OK;
}
