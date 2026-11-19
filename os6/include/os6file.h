#ifndef CPU6_OS6FILE_H
#define CPU6_OS6FILE_H
#include <stdint.h>

#define OS6_READ 1
#define OS6_WRITE 2
#define OS6_RECORD_SIZE 256

/* CPU6's native sequential-file Record Control Block (30 bytes). */
#pragma pack(1)
typedef struct {
    uint8_t status;
    uint8_t logical_unit;
    uint16_t control;
    uint16_t buffer_size;
    uint8_t *buffer;
    uint16_t reserved0;
    uint16_t record_flags;
    uint8_t class_code;
    uint8_t file_type;
    uint8_t reserved1[12];
    uint16_t reserved2, reserved3;
} OS6_RCB;

typedef struct {
    uint8_t unit;
    char name[11];
    uint16_t sector_cur;
    uint8_t mode;
    OS6_RCB rcb;
    uint8_t io_buffer[403];
} FILE_OS6;
#pragma pack()

/* Open an existing root file. mode is OS6_READ or OS6_WRITE. Returns 0 on
 * success and nonzero on error. One FILE_OS6 may be open at a time. */
int os6_fopen(FILE_OS6 *file, const char *name, uint8_t unit, uint8_t mode);
/* Transfer one 256-byte binary record; the drive's physical sector is 400 B. */
int os6_fread_sector(FILE_OS6 *file, void *buffer256);
int os6_fwrite_sector(FILE_OS6 *file, const void *buffer256);
int os6_fclose(FILE_OS6 *file);
#endif
