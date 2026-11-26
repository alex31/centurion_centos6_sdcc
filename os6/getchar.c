#include <stdio.h>

int os6_terminal_record(unsigned char *record, unsigned int limit);

#define RECORD_LIMIT 75

/* A native record starts with its two-byte length and ends with a NUL. */
static unsigned char record[2 + RECORD_LIMIT + 1];
static unsigned int next, length;
static unsigned char line_pending;

int getchar(void)
{
    if (next == length) {
        int count;

        /* A full record is returned as soon as it fills. Only a shorter
           record, including an empty one, consumes the physical Enter. */
        if (line_pending && length < RECORD_LIMIT) {
            line_pending = 0;
            return '\n';
        }
        count = os6_terminal_record(record, RECORD_LIMIT);
        if (count <= 0 || count > RECORD_LIMIT) {
            line_pending = 0;
            return count == 0 ? '\n' : EOF;
        }
        next = 0;
        length = count;
        line_pending = 1;
    }
    return record[2 + next++] & 0x7f;
}

void os6_discard_line(void)
{
    while (line_pending) {
        next = length;
        if (getchar() == EOF)
            break;
    }
}

int os6_getline_checked(char *buffer, unsigned int capacity)
{
    unsigned int limit, used = 0;
    int character;

    if (!buffer || !capacity)
        return EOF;
    /* The length returned by this interface is a signed 16-bit int. */
    limit = capacity > 32768u ? 32767u : capacity - 1;
    while ((character = getchar()) != '\n') {
        int status;

        if (character == EOF) {
            buffer[0] = '\0';
            return EOF;
        }
        if ((character < ' ' && character != '\t') || character == 127)
            status = OS6_LINE_INVALID;
        else if (used == limit)
            status = OS6_LINE_TOO_LONG;
        else {
            buffer[used++] = (char)character;
            continue;
        }
        os6_discard_line();
        buffer[0] = '\0';
        return status;
    }
    buffer[used] = '\0';
    return (int)used;
}
