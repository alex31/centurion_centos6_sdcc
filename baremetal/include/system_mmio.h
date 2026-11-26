#ifndef CENTURION_SYSTEM_MMIO_H
#define CENTURION_SYSTEM_MMIO_H

#include <stdint.h>

/* CPU6 logical addresses from the reverse-engineered Centurion memory map.
   Peripheral availability depends on the cards installed in the machine.
   https://github.com/Nakazoto/CenturionComputer/wiki/Memory-Mapping */
#define CENTURION_MMIO8(address) (*(volatile uint8_t *)(address))

/* Printer window as mapped by the emulator. */
#define CENTURION_PRT_BASE 0xF0E0u
#define CENTURION_PRT_END  0xF0EFu

/* Optional diagnostic card. 0xF110 reads the switches and writes the display.
   Decimal points are numbered 1 through 4. */
#define CENTURION_DIAG_UNBLANK_ADDR      0xF106u
#define CENTURION_DIAG_BLANK_ADDR        0xF107u
#define CENTURION_DIAG_SWITCHES_ADDR     0xF110u
#define CENTURION_DIAG_HEX_DISPLAY_ADDR  0xF110u
#define CENTURION_DIAG_DECIMAL_SET_ADDR(digit) (0xF108u + 2u * ((digit) - 1u))
#define CENTURION_DIAG_DECIMAL_CLEAR_ADDR(digit) (CENTURION_DIAG_DECIMAL_SET_ADDR(digit) + 1u)

/* Hawk DSK-II. The sector address and disk status each occupy two bytes. */
#define CENTURION_DSK2_BASE               0xF140u
#define CENTURION_DSK2_UNIT_SELECT_ADDR   0xF140u
#define CENTURION_DSK2_SECTOR_ADDR        0xF141u
#define CENTURION_DSK2_WRITE_MASK_ADDR    0xF143u
#define CENTURION_DSK2_DISK_STATUS_ADDR   0xF144u
#define CENTURION_DSK2_COMMAND_ADDR       0xF148u
#define CENTURION_DSK2_DIAG_CONTROL_ADDR  0xF149u
#define CENTURION_DSK2_DIAG_CLOCK_ADDR    0xF14Au
#define CENTURION_DSK2_RESET_ADDR         0xF14Bu
#define CENTURION_DSK2_ASSERT_IRQ_ADDR    0xF14Cu
#define CENTURION_DSK2_DISABLE_IRQ_ADDR   0xF14Du
#define CENTURION_DSK2_ENABLE_IRQ_ADDR    0xF14Eu
#define CENTURION_DSK2_CLEAR_IRQ_ADDR     0xF14Fu

/* MUX card numbers are 0 through 15. Port numbers are 0 through 3. */
#define CENTURION_MUX_BASE 0xF200u
#define CENTURION_MUX_CARD_ADDR(card) (CENTURION_MUX_BASE + 0x10u * (card))
#define CENTURION_MUX_STATUS_ADDR(card, port) (CENTURION_MUX_CARD_ADDR(card) + 2u * (port))
#define CENTURION_MUX_DATA_ADDR(card, port) (CENTURION_MUX_STATUS_ADDR(card, port) + 1u)
#define CENTURION_MUX_RTS_ADDR(card) (CENTURION_MUX_CARD_ADDR(card) + 8u)
#define CENTURION_MUX_IRQ_LEVEL_ADDR(card) (CENTURION_MUX_CARD_ADDR(card) + 0xAu)
#define CENTURION_MUX_BAUD_ADDR(card) (CENTURION_MUX_CARD_ADDR(card) + 0xBu)
#define CENTURION_MUX_ASSERT_TX_IRQ_ADDR(card) (CENTURION_MUX_CARD_ADDR(card) + 0xCu)
#define CENTURION_MUX_DISABLE_IRQ_ADDR(card) (CENTURION_MUX_CARD_ADDR(card) + 0xDu)
#define CENTURION_MUX_ENABLE_IRQ_ADDR(card) (CENTURION_MUX_CARD_ADDR(card) + 0xEu)
#define CENTURION_MUX_RESET_ADDR(card) (CENTURION_MUX_CARD_ADDR(card) + 0xFu)
/* A read at card 0's reset address returns interrupt information. */
#define CENTURION_MUX_IRQ_INFO_ADDR CENTURION_MUX_RESET_ADDR(0)
#define CENTURION_MUX_STATUS_RX_READY 0x01u
#define CENTURION_MUX_STATUS_TX_EMPTY 0x02u

/* The PROM on the FFC card selects its base address. */
#ifndef CENTURION_FFC_BASE
#define CENTURION_FFC_BASE 0xF800u
#endif
#define CENTURION_FFC_COMMAND_DATA_ADDR CENTURION_FFC_BASE
#define CENTURION_FFC_STATUS_ADDR       (CENTURION_FFC_BASE + 1u)

/* CMD disk controller address window. */
#define CENTURION_CMD_BASE 0xF808u
#define CENTURION_CMD_END  0xF809u

#endif
