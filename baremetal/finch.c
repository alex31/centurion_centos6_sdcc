#include <system_finch.h>

#define FFC_POLL_LIMIT 150000UL
#define FINCH_RAM_END 0xf000u

/* Chain elements used by the CPU6 diagnostic and the original Finch utility. */
enum {
    SELECT_UNIT = 0x81,
    RESTORE = 0x82,
    SEEK = 0x83,
    SELECT_HEAD = 0x84,
    FORMAT_TRACK = 0x89,
    READ_SECTORS = 0x8a,
    WRITE_SECTORS = 0x8b,
    END_CHAIN = 0xff
};

void finch_dma_arm(const void *address, uint16_t count);
uint16_t finch_dma_stop(void);

static int wait_status(uint8_t mask, uint8_t expected)
{
    uint32_t polls = FFC_POLL_LIMIT;
    do {
        if ((FFC->status & mask) == expected)
            return FINCH_OK;
    } while (--polls);
    return FINCH_TIMEOUT;
}

static int command(uint8_t code, const void *data, uint16_t length)
{
    int result = wait_status(FFC_STATUS_BUSY | FFC_STATUS_FIN, 0);
    if (result)
        return result;
    if (length)
        finch_dma_arm(data, (uint16_t)~length);

    FFC->command_data = code;
    result = wait_status(FFC_STATUS_BUSY | FFC_STATUS_FIN | FFC_STATUS_FOUT,
                         FFC_STATUS_FOUT);

    if (!result)
        result = FFC->command_data;
    if (length) {
        uint16_t remaining = finch_dma_stop();
        if (!result && remaining != 0xffffu)
            return FINCH_INCOMPLETE_DMA;
    }
    return result;
}

static int execute(const uint8_t *chain, uint16_t chain_size,
                   const void *data, uint16_t length)
{
    int result = command(FFC_CMD_DMA_COMMAND_BUFFER, chain, chain_size);
    if (result)
        return result;
    return command(FFC_CMD_EXECUTE_COMMAND_BUFFER, data, length);
}

static int valid_transfer(uint8_t unit, uint16_t cylinder, uint8_t head,
                          const void *data, uint16_t length)
{
    return unit >= 1 && unit <= 3 && cylinder < FINCH_CYLINDERS && head < 8 &&
           data && (uint16_t)data <= FINCH_RAM_END - length;
}

static void sector_chain(uint8_t *chain, uint8_t unit, uint16_t cylinder,
                         uint8_t head, uint8_t operation)
{
    chain[0] = SELECT_UNIT;
    chain[1] = (uint8_t)(1u << unit);
    chain[2] = SELECT_HEAD;
    chain[3] = head;
    chain[4] = SEEK;
    chain[5] = (uint8_t)(cylinder >> 8);
    chain[6] = (uint8_t)cylinder;
    chain[7] = operation;
}

static void sector_record(uint8_t *record, uint8_t sector)
{
    record[0] = sector;
    record[1] = (uint8_t)(FINCH_SECTOR_SIZE >> 8);
    record[2] = (uint8_t)FINCH_SECTOR_SIZE;
}

int finch_init(uint8_t unit)
{
    uint8_t chain[] = {SELECT_UNIT, 0, SELECT_HEAD, 0, RESTORE, END_CHAIN};
    if (unit < 1 || unit > 3)
        return FINCH_INVALID_ARGUMENT;
    chain[1] = (uint8_t)(1u << unit);
    FFC_RESET_REG = 0;
    return execute(chain, sizeof chain, 0, 0);
}

static int transfer_sector(uint8_t unit, uint16_t cylinder, uint8_t head,
                           uint8_t sector, const void *data, uint8_t operation)
{
    uint8_t chain[12];
    if (sector >= FINCH_SECTORS_PER_TRACK ||
        !valid_transfer(unit, cylinder, head, data, FINCH_SECTOR_SIZE))
        return FINCH_INVALID_ARGUMENT;
    sector_chain(chain, unit, cylinder, head, operation);
    sector_record(chain + 8, sector);
    chain[11] = END_CHAIN;
    return execute(chain, sizeof chain, data, FINCH_SECTOR_SIZE);
}

int finch_read_sector(uint8_t unit, uint16_t cylinder, uint8_t head,
                      uint8_t sector, void *data)
{
    return transfer_sector(unit, cylinder, head, sector, data, READ_SECTORS);
}

int finch_write_sector(uint8_t unit, uint16_t cylinder, uint8_t head,
                       uint8_t sector, const void *data)
{
    return transfer_sector(unit, cylinder, head, sector, data, WRITE_SECTORS);
}

int finch_format_track(uint8_t unit, uint16_t cylinder, uint8_t head,
                       const void *data)
{
    uint8_t chain[8 + 3 * FINCH_SECTORS_PER_TRACK + 1];
    uint8_t sector;
    if (!valid_transfer(unit, cylinder, head, data, FINCH_TRACK_SIZE))
        return FINCH_INVALID_ARGUMENT;

    sector_chain(chain, unit, cylinder, head, FORMAT_TRACK);
    for (sector = 0; sector < FINCH_SECTORS_PER_TRACK; ++sector)
        sector_record(chain + 8 + 3 * sector, sector);
    chain[sizeof chain - 1] = END_CHAIN;
    return execute(chain, sizeof chain, data, FINCH_TRACK_SIZE);
}

int finch_format_disk(uint8_t unit, uint8_t heads, const void *track_data)
{
    uint16_t cylinder;
    if (!heads || heads > 8 || !valid_transfer(unit, 0, 0, track_data, FINCH_TRACK_SIZE))
        return FINCH_INVALID_ARGUMENT;

    for (cylinder = 0; cylinder < FINCH_CYLINDERS; ++cylinder) {
        uint8_t head;
        for (head = 0; head < heads; ++head) {
            int result = finch_format_track(unit, cylinder, head, track_data);
            if (result)
                return result;
        }
    }
    return FINCH_OK;
}
