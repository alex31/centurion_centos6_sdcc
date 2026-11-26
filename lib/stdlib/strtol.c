#include <errno.h>
#include <stdlib.h>

int errno;

static unsigned int digit_value(unsigned char character)
{
    if (character >= '0' && character <= '9')
        return character - '0';
    if (character >= 'A' && character <= 'Z')
        return character - 'A' + 10;
    if (character >= 'a' && character <= 'z')
        return character - 'a' + 10;
    return 36;
}

long strtol(const char *text, char **end, int base)
{
    const char *cursor = text;
    const char *first_digit;
    unsigned long value = 0, limit, cutoff;
    unsigned int digit, last;
    int negative, overflow = 0;

    if (end)
        *end = (char *)text;
    if (base && (base < 2 || base > 36)) {
        errno = EINVAL;
        return 0;
    }

    while (*cursor == ' ' || (*cursor >= '\t' && *cursor <= '\r'))
        ++cursor;
    negative = *cursor == '-';
    if (negative || *cursor == '+')
        ++cursor;
    if ((base == 0 || base == 16) && cursor[0] == '0' &&
        (cursor[1] == 'x' || cursor[1] == 'X') && digit_value(cursor[2]) < 16) {
        base = 16;
        cursor += 2;
    }
    if (!base)
        base = *cursor == '0' ? 8 : 10;

    limit = negative ? 0x80000000UL : 0x7fffffffUL;
    cutoff = limit / (unsigned int)base;
    last = limit % (unsigned int)base;
    first_digit = cursor;
    while ((digit = digit_value(*cursor)) < (unsigned int)base) {
        if (value > cutoff || (value == cutoff && digit > last))
            overflow = 1;
        if (!overflow)
            value = value * (unsigned int)base + digit;
        ++cursor;
    }
    if (cursor == first_digit)
        return 0;
    if (end)
        *end = (char *)cursor;
    if (overflow) {
        errno = ERANGE;
        return negative ? -2147483647L - 1 : 2147483647L;
    }
    if (negative && value == 0x80000000UL)
        return -2147483647L - 1;
    return negative ? -(long)value : (long)value;
}
