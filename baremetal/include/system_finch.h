#ifndef CENTURION_SYSTEM_FINCH_H
#define CENTURION_SYSTEM_FINCH_H

#include <system_mmio.h>

/* CPU-facing registers of the Finch/Floppy Controller (FFC).
   https://github.com/Nakazoto/CenturionComputer/wiki/FFC-Board#cpu-interface */
typedef struct {
    uint8_t command_data;
    const uint8_t status;
} FfcRegisters;

#define FFC ((volatile FfcRegisters *)CENTURION_FFC_BASE)

/* Writing to the status address resets the card. Use this alias for reset. */
#define FFC_RESET_REG CENTURION_MMIO8(CENTURION_FFC_STATUS_ADDR)

#define FFC_STATUS_FOUT 0x01u /* A byte is available for the CPU. */
#define FFC_STATUS_FIN  0x02u /* A CPU byte is pending at the controller. */
#define FFC_STATUS_BUSY 0x08u /* The controller is executing a command. */

/* Commands written to command_data. */
#define FFC_CMD_DMA_COMMAND_BUFFER      0x43u
#define FFC_CMD_EXECUTE_COMMAND_BUFFER  0x45u
#define FFC_CMD_DMA_TO_FFC              0x46u
#define FFC_CMD_DMA_FROM_FFC            0x47u
#define FFC_CMD_READ_DRIVE_STATUS_1     0x50u
#define FFC_CMD_READ_DRIVE_STATUS_3     0x51u
#define FFC_CMD_READ_DRIVE_READY        0x52u

/* The low four bits of the drive-ready reply correspond to units 0 through 3. */
#define FFC_DRIVE_READY_MASK 0x0Fu

#define FINCH_SECTOR_SIZE 400u
#define FINCH_SECTORS_PER_TRACK 29u
#define FINCH_CYLINDERS 605u
#define FINCH_TRACK_SIZE (FINCH_SECTOR_SIZE * FINCH_SECTORS_PER_TRACK)

enum {
    FINCH_OK = 0,
    FINCH_INVALID_ARGUMENT = -1,
    FINCH_TIMEOUT = -2,
    FINCH_INCOMPLETE_DMA = -3
};

/* Bare-metal driver for the standard 400-byte Finch sector format. Unit is
   1 through 3. Cylinder, head and sector are zero-based. Positive returns
   are the controller's status bytes. Calls require exclusive use of the
   controller and CPU DMA, with buffers in identity-mapped system RAM. */

/* Reset the controller and seek the selected drive to cylinder zero.
   Call before the first transfer and after a timeout. */
int finch_init(uint8_t unit);
int finch_read_sector(uint8_t unit, uint16_t cylinder, uint8_t head,
                      uint8_t sector, void *data);
int finch_write_sector(uint8_t unit, uint16_t cylinder, uint8_t head,
                       uint8_t sector, const void *data);

/* Overwrite one physical track with FINCH_TRACK_SIZE bytes of data in
   sector order. Filesystem initialization is a separate operation. */
int finch_format_track(uint8_t unit, uint16_t cylinder, uint8_t head,
                       const void *data);

/* Format every cylinder using the installed drive's head count and the
   same FINCH_TRACK_SIZE-byte image on each track. Stops on the first error. */
int finch_format_disk(uint8_t unit, uint8_t heads, const void *track_data);

#endif
