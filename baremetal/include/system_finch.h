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

#endif
