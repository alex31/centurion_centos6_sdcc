#include <os6.h>
#include <os6file.h>

int os6_assign_raw(char *name, OS6_RCB *rcb, int unit, int options);
int os6_file_open_raw(OS6_RCB *rcb, int mode);
int os6_file_read_raw(OS6_RCB *rcb, void *record);
int os6_file_write_raw(OS6_RCB *rcb, void *record);
int os6_file_close_raw(OS6_RCB *rcb);
int os6_jcl_submit(void);
void os6_jcl_clock(uint32_t *ticks);

#define JCL_DISK 1
#define JCL_SECTOR 400u
#define JCL_TIMEOUT_TICKS 300u
#define JCL_MAX_POLLS 2048u
#define TICKS_PER_DAY 864000UL

/* LIOCS service 5 transfers sectors of a native type A file. Input uses the
   length word as its argument; output starts at the capacity word. */
struct text_file {
    OS6_RCB rcb;
    uint16_t sector[2];
    struct {
        uint16_t capacity, length;
        unsigned char data[JCL_SECTOR], terminator;
    } block;
    unsigned used;
};

static int file_open(struct text_file *file, const char *name, int mode)
{
    char encoded[22];
    unsigned char *rcb = (unsigned char *)&file->rcb;
    unsigned i;
    int status;
    for (i = 0; i < sizeof(file->rcb); ++i) rcb[i] = 0;
    for (i = 0; name[i]; ++i) encoded[i] = name[i] | 128;
    encoded[i] = 0;
    file->rcb.control = 0x2101;
    file->rcb.buffer_size = JCL_SECTOR;
    file->rcb.buffer = (uint8_t *)&file->block.length;
    file->rcb.record_flags = 5;
    file->rcb.class_code = 1;
    file->rcb.file_type = 255;
    file->rcb.reserved2 = (uint16_t)file->sector;
    file->sector[0] = file->sector[1] = file->used = 0;
    file->block.capacity = file->block.length = JCL_SECTOR;
    status = os6_assign_raw(encoded, &file->rcb, JCL_DISK, 0);
    if (status) return status;
    status = os6_file_open_raw(&file->rcb, mode);
    if (status) os6_assign_raw(0, &file->rcb, JCL_DISK, 0);
    return status;
}

static int file_close(struct text_file *file)
{
    int status = os6_file_close_raw(&file->rcb);
    int unassign = os6_assign_raw(0, &file->rcb, JCL_DISK, 0);
    return status ? status : unassign;
}

static int flush_sector(struct text_file *file)
{
    int status;
    while (file->used < JCL_SECTOR) file->block.data[file->used++] = 0;
    file->block.capacity = file->block.length = JCL_SECTOR;
    status = os6_file_write_raw(&file->rcb, &file->block);
    if (!status) ++file->sector[1];
    file->used = 0;
    return status;
}

/* Write complete records with the extra CR that terminates a full sector. */
static int put_record(struct text_file *file, const char *text, unsigned length)
{
    if (!length) {
        text = " ";
        length = 1;
    }
    if (length + 2 > JCL_SECTOR - file->used) {
        file->block.data[file->used++] = 0x8d;
        if (flush_sector(file)) return -1;
    }
    while (length--) file->block.data[file->used++] = (unsigned char)*text++ | 128;
    file->block.data[file->used++] = 0x8d;
    return 0;
}

static int put_stream(struct text_file *file, const char *text)
{
    while (*text) {
        const char *end = text;
        while (*end && *end != '\r' && *end != '\n') ++end;
        if (put_record(file, text, end - text)) return -1;
        if (*end == '\r') ++end;
        if (*end == '\n') ++end;
        text = end;
    }
    return 0;
}

static int valid_stream(const char *job)
{
    unsigned length = 0, lines = 0;
    if (!job) return 0;
    while (*job) {
        unsigned char c = *job++;
        if (c == '\r' || c == '\n') length = 0;
        else {
            if ((c < 32 && c != '\t') || c > 126 || ++length > JCL_SECTOR - 2) return 0;
            lines = 1;
        }
    }
    return lines;
}

static int prepare_file(struct text_file *file, const char *name, const char *job, int tag)
{
    static const char eof[] = {4};
    int status = 0, closed;
    if (file_open(file, name, OS6_WRITE)) return -1;
    if (job) {
        status = put_stream(file, ".USE JCLSDK.LOG ON 1 FOR SYSLOG\n.NOLOG\n.NOTIME\n/ SDKJCL-BEGIN /\n") ||
                 put_stream(file, job) ||
                 put_stream(file, ".USE CRT1 FOR SYSLOG\n") ||
                 put_stream(file, tag ? ".NAM JCLSDK.TAG1 ON 1 TAG0\n" : ".NAM JCLSDK.TAG0 ON 1 TAG1\n") ||
                 put_stream(file, ".USE CRT1 FOR SYSRDR\n");
    }
    if (!status) status = put_record(file, eof, 1) || flush_sector(file);
    closed = file_close(file);
    return status || closed;
}

static int tag_exists(struct text_file *file, const char *name)
{
    if (file_open(file, name, OS6_READ)) return 0;
    return file_close(file) ? -1 : 1;
}

/* The dispatcher publishes the completion tag after closing SYSLOG. At
   that point the entire log is available and its native EOF is mandatory. */
static int collect_log(struct text_file *file, char *output, uint16_t capacity,
                       uint16_t *copied)
{
    static const char start[] = "SDKJCL-BEGIN";
    unsigned match = 0;
    uint16_t count = 0;
    int active = 0, truncated = 0, result = OS6_JCL_IO;
    if (file_open(file, "JCLSDK.LOG", OS6_READ)) return OS6_JCL_IO;
    for (;;) {
        unsigned i, used = 0;
        int status;
        file->block.capacity = file->block.length = JCL_SECTOR;
        status = os6_file_read_raw(&file->rcb, &file->block.length);
        if (status) break;
        ++file->sector[1];
        for (i = 0; i < JCL_SECTOR; ++i) {
            unsigned char c = file->block.data[i] & 127u;
            if (c == 4) {
                if (active) result = truncated ? OS6_JCL_TRUNCATED : OS6_JCL_OK;
                goto close_file;
            }
            if (!c) continue;
            if (c == '\r' && !used) break;
            if (active) {
                if (capacity && count < capacity - 1) output[count++] = c == '\r' ? '\n' : c;
                else truncated = 1;
            } else if (c == '\r') {
                active = match == sizeof(start) - 1;
                match = 0;
            } else if (match < sizeof(start) - 1) {
                match = c == (unsigned char)start[match] ? match + 1 : (c == 'S' ? 1u : 0u);
            }
            used = c != '\r';
        }
        os6_checkpoint();
    }
close_file:
    if (file_close(file)) return OS6_JCL_IO;
    if (result >= 0) {
        *copied = count;
        if (capacity) output[count] = 0;
    }
    return result;
}

int system_jcl(const char *job, char *output, uint16_t *size)
{
    struct text_file file;
    uint16_t capacity;
    uint32_t started, now;
    unsigned polls = JCL_MAX_POLLS;
    int result, tag0, tag1;
    if (!size) return OS6_JCL_ARGUMENT;
    capacity = *size;
    *size = 0;
    if (capacity && !output) return OS6_JCL_ARGUMENT;
    if (!valid_stream(job)) {
        if (capacity) output[0] = 0;
        return OS6_JCL_ARGUMENT;
    }
    tag0 = tag_exists(&file, "JCLSDK.TAG0");
    tag1 = tag_exists(&file, "JCLSDK.TAG1");
    result = tag0 < 0 || tag1 < 0 || tag0 == tag1 ||
             prepare_file(&file, "JCLSDK.LOG", 0, 0) ||
             prepare_file(&file, "JCLSDK.REQ", job, tag1) || os6_jcl_submit();
    if (capacity) output[0] = 0;
    if (result) return OS6_JCL_IO;
    os6_jcl_clock(&started);
    do {
        result = tag_exists(&file, tag1 ? "JCLSDK.TAG0" : "JCLSDK.TAG1");
        if (result) {
            result = result > 0 ? collect_log(&file, output, capacity, size) : OS6_JCL_IO;
            if (result < 0 && capacity) output[0] = 0;
            return result;
        }
        os6_checkpoint();
        os6_jcl_clock(&now);
        if (now < started) now += TICKS_PER_DAY;
    } while (--polls && now - started < JCL_TIMEOUT_TICKS);
    if (capacity) output[0] = 0;
    return OS6_JCL_TIMEOUT;
}
