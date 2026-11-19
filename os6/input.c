#include <stdio.h>

char *os6_readline_raw(void);

int os6_getline(char *buffer, unsigned int capacity)
{
    char *source;
    unsigned int i = 0, length = 0;
    if (!buffer || capacity == 0) return -1;
    source = os6_readline_raw();
    while (length < 75 && source[length]) length = length + 1;
    /* C75 input is space padded by OS6; do not expose the padding as text. */
    while (length && (source[length - 1] & 0x7f) == ' ') length = length - 1;
    while (i + 1 < capacity && i < length) {
        buffer[i] = source[i] & 0x7f;
        i = i + 1;
    }
    buffer[i] = 0;
    return i;
}

/* Historical API requested by the toolchain plan: callers must supply at
 * least 76 bytes. Prefer os6_getline when the buffer size is known. */
char *gets(char *buffer)
{
    if (os6_getline(buffer, 76) < 0) return (char *)0;
    return buffer;
}
