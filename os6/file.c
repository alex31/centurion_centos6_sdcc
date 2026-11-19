#include <os6file.h>

int os6_assign_raw(char *name, OS6_RCB *rcb, int unit, int options);
int os6_file_open_raw(OS6_RCB *rcb, int mode);
int os6_file_read_raw(OS6_RCB *rcb, void *record);
int os6_file_write_raw(OS6_RCB *rcb, void *record);
int os6_file_end_raw(OS6_RCB *rcb);
int os6_file_close_raw(OS6_RCB *rcb);

struct os6_record {
    uint16_t capacity, length;
    uint8_t data[OS6_RECORD_SIZE];
    uint8_t terminator;
};
static struct os6_record record;
static FILE_OS6 *active_file;

int os6_fopen(FILE_OS6 *file, const char *name, uint8_t unit, uint8_t mode)
{
    int i, status;
    uint8_t *raw;
    if (!file || !name || active_file || (mode != OS6_READ && mode != OS6_WRITE))
        return -1;
    for (i = 0; i < 10 && name[i]; i++) {
        unsigned char ch = name[i];
        if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        file->name[i] = ch | 0x80;
    }
    if (i == 0 || name[i]) return -1;
    file->name[i] = 0;
    raw = (uint8_t *)&file->rcb;
    for (i = 0; i < sizeof(OS6_RCB); i++) raw[i] = 0;
    file->unit = unit;
    file->mode = mode;
    file->sector_cur = 0;
    file->rcb.buffer_size = 400;
    file->rcb.buffer = file->io_buffer;
    file->rcb.record_flags = 4;
    file->rcb.class_code = 2;
    file->rcb.file_type = 1;
    status = os6_assign_raw(file->name, &file->rcb, unit, 0);
    if (status) return status;
    status = os6_file_open_raw(&file->rcb, mode);
    if (status) {
        os6_assign_raw((char *)0, &file->rcb, unit, 0);
        return status;
    }
    active_file = file;
    return 0;
}

int os6_fread_sector(FILE_OS6 *file, void *buffer256)
{
    unsigned int i;
    uint8_t *out = (uint8_t *)buffer256;
    int status;
    if (!file || active_file != file || file->mode != OS6_READ || !buffer256) return -1;
    record.capacity = OS6_RECORD_SIZE;
    record.length = OS6_RECORD_SIZE;
    status = os6_file_read_raw(&file->rcb, &record);
    if (status) return status;
    if (record.length != OS6_RECORD_SIZE) return -1;
    for (i = 0; i < OS6_RECORD_SIZE; i++) out[i] = record.data[i];
    file->sector_cur = file->sector_cur + 1;
    return 0;
}

int os6_fwrite_sector(FILE_OS6 *file, const void *buffer256)
{
    unsigned int i;
    const uint8_t *in = (const uint8_t *)buffer256;
    int status;
    if (!file || active_file != file || file->mode != OS6_WRITE || !buffer256) return -1;
    record.capacity = OS6_RECORD_SIZE;
    record.length = OS6_RECORD_SIZE;
    for (i = 0; i < OS6_RECORD_SIZE; i++) record.data[i] = in[i];
    status = os6_file_write_raw(&file->rcb, &record);
    if (!status) file->sector_cur = file->sector_cur + 1;
    return status;
}

int os6_fclose(FILE_OS6 *file)
{
    int status = 0, next;
    if (!file || active_file != file) return -1;
    if (file->mode == OS6_WRITE) status = os6_file_end_raw(&file->rcb);
    next = os6_file_close_raw(&file->rcb);
    if (!status) status = next;
    next = os6_assign_raw((char *)0, &file->rcb, file->unit, 0);
    if (!status) status = next;
    active_file = (FILE_OS6 *)0;
    return status;
}
