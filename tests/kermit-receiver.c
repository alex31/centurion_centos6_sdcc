/* Exercise E-Kermit receiver I/O callbacks with valid short packets. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cdefs.h"
#include "kermit.h"

static struct k_data state;
static struct k_response result;
static UCHAR buffer[4], contents[16];
static unsigned written, closes;
static int fail_write, fail_close;
static char reply_type, disposition;

static int reply(struct k_data *k, UCHAR *bytes, int size)
{
    assert(k == &state && size >= 6);
    reply_type = bytes[3];
    return X_OK;
}

static int open_file(struct k_data *k, UCHAR *name, int mode)
{
    assert(k == &state && mode == 2 && !strcmp((char *)name, "FILE"));
    return X_OK;
}

static int write_file(struct k_data *k, UCHAR *bytes, int size)
{
    assert(k == &state && size > 0 && size <= (int)sizeof(buffer));
    if (fail_write) return X_ERROR;
    assert(written + size <= sizeof(contents));
    memcpy(contents + written, bytes, size);
    written += size;
    return X_OK;
}

static int close_file(struct k_data *k, UCHAR action, int mode)
{
    assert(k == &state && mode == 2 && closes++ == 0);
    disposition = action;
    return fail_close ? X_ERROR : X_OK;
}

static int packet(char type, const char *data)
{
    short slot;
    UCHAR *p = getrslot(&state, &slot);
    unsigned length = strlen(data), check = 0;
    assert(p && length < 90);
    p[0] = 32 + length + 3;
    p[1] = 32 + state.r_seq;
    p[2] = type;
    memcpy(p + 3, data, length);
    for (unsigned i = 0; i < length + 3; ++i) check += p[i];
    p[length + 3] = 32 + ((check + ((check & 0300) >> 6)) & 077);
    reply_type = 0;
    return kermit(K_RUN, &state, slot, length + 4, "", &result);
}

static void start_file(void)
{
    memset(&state, 0, sizeof(state));
    state.openf = open_file;
    state.writef = write_file;
    state.closef = close_file;
    state.txd = reply;
    state.obuf = buffer;
    state.obuflen = sizeof(buffer);
    state.bct = 1;
    written = closes = 0;
    fail_write = fail_close = disposition = 0;
    assert(kermit(K_INIT, &state, 0, 0, "", &result) == X_OK);
    assert(packet('S', "") == X_OK && reply_type == 'Y');
    assert(packet('F', "FILE") == X_OK && reply_type == 'Y');
}

int main(void)
{
    start_file();
    assert(packet('Z', "") == X_OK && reply_type == 'Y');
    assert(!written && closes == 1 && result.status == R_FILE);
    assert(packet('B', "") == X_DONE && reply_type == 'Y');

    start_file();
    assert(packet('D', "abcdef") == X_OK && reply_type == 'Y');
    assert(written == 4 && result.sofar == 4);
    assert(packet('Z', "") == X_OK && reply_type == 'Y');
    assert(closes == 1 && written == 6 && result.sofar == 6);
    assert(!memcmp(contents, "abcdef", 6));
    assert(packet('B', "") == X_DONE);

    start_file();
    assert(packet('D', "abc") == X_OK && reply_type == 'Y');
    fail_write = 1;
    assert(packet('Z', "") == X_ERROR && reply_type == 'E');
    assert(!written && !result.sofar && closes == 1 && disposition == 'D');

    start_file();
    fail_write = 1;
    assert(packet('D', "abcdef") == X_ERROR && reply_type == 'E');
    assert(!written && !result.sofar && closes == 1 && disposition == 'D');

    start_file();
    fail_close = 1;
    assert(packet('Z', "") == X_ERROR && reply_type == 'E');
    assert(closes == 1 && result.status == R_ERROR);
    puts("PASS Kermit receiver: empty file, buffered data, write and close errors");
    return 0;
}
