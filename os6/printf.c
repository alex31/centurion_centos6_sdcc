#include <stdarg.h>
#include <stdio.h>

/* Like the existing CPU6 arithmetic runtime, this formatter is not reentrant.
   Default printf returns flush output; listings may opt into batch mode. */
static char output_buffer[64];
static unsigned int output_length;
extern void (*os6_exit_flush)(void);
typedef const char *string_arg;
void os6_write_encoded(const char *data, unsigned int length);

static void flush_output(void)
{
    unsigned int length = output_length;
    /* CENTOS trims trailing blanks from a formatted multi-character field.
       Its single-character path preserves them, as required for C prompts. */
    while (length && (unsigned char)output_buffer[length - 1] == 0xa0U) length--;
    if (length) os6_write_encoded(output_buffer, length);
    while (length < output_length) {
        putchar(' ');
        length++;
    }
    output_length = 0;
}

/* Explicit batch mode for listings. Ordinary printf calls remain immediately
   visible. Flush before mixing a batch with putchar/puts or reading input. */
void os6_printf_flush(void)
{
    flush_output();
}

void os6_printf_buffered(int enabled)
{
    if (!enabled) flush_output();
    os6_exit_flush = enabled ? os6_printf_flush : (void (*)(void))0;
}

static void emit_character(int value)
{
    if (value == '\n') {
        flush_output();
        putchar('\n');
    } else {
        output_buffer[output_length++] = (char)(value | 0x80);
        if (output_length == sizeof(output_buffer)) flush_output();
    }
}

static int print_word(unsigned int value, unsigned int base)
{
    static const unsigned int decimal[5] = {10000U,1000U,100U,10U,1U};
    unsigned int position, digit, leading = 1, limit = base == 16 ? 4 : 5;
    int count = 0;
    for (position = 0; position < limit; position++) {
        if (base == 16) {
            digit = (value >> 12) & 15U;
            value = (unsigned int)(value << 4);
        } else {
            digit = 0;
            while (value >= decimal[position]) {
                value -= decimal[position];
                digit++;
            }
        }
        if (digit || !leading || position + 1 == limit) {
            emit_character(digit < 10 ? digit + '0' : digit - 10 + 'a');
            leading = 0;
            count++;
        }
    }
    return count;
}

static int print_long(unsigned long value, unsigned int base)
{
    static const unsigned long decimal[10] = {
        1000000000UL,100000000UL,10000000UL,1000000UL,100000UL,
        10000UL,1000UL,100UL,10UL,1UL
    };
    unsigned int position, digit, leading = 1, limit = base == 16 ? 8 : 10;
    int count = 0;
    for (position = 0; position < limit; position++) {
        if (base == 16) {
            digit = (unsigned int)(value >> 28) & 15U;
            value <<= 4;
        } else {
            digit = 0;
            while (value >= decimal[position]) {
                value -= decimal[position];
                digit++;
            }
        }
        if (digit || !leading || position + 1 == limit) {
            emit_character(digit < 10 ? digit + '0' : digit - 10 + 'a');
            leading = 0;
            count++;
        }
    }
    return count;
}

int printf(const char *format, ...)
{
    va_list arguments;
    int count = 0;
    va_start(arguments, format);
    while (*format) {
        int wide = 0;
        char c = *format++;
        if (c != '%') {
            emit_character(c);
            count++;
            continue;
        }
        c = *format++;
        if (c == 'l') {
            wide = 1;
            c = *format++;
        }
        /* A trailing incomplete conversion must not read past the string. */
        if (!c) {
            emit_character('%'); count++;
            if (wide) { emit_character('l'); count++; }
            break;
        }
        if (c == 's') {
            const char *s = va_arg(arguments, string_arg);
            while (*s) { emit_character(*s++); count++; }
        } else if (c == 'c') {
            emit_character(va_arg(arguments, int));
            count++;
        } else if (c == 'd') {
            if (wide) {
                long value = va_arg(arguments, long);
                unsigned long magnitude = (unsigned long)value;
                if (value < 0) {
                    emit_character('-'); count++;
                    magnitude = 0UL - magnitude;
                }
                count += print_long(magnitude, 10);
            } else {
                int value = va_arg(arguments, int);
                unsigned int magnitude = (unsigned int)value;
                if (value < 0) {
                    emit_character('-'); count++;
                    magnitude = 0U - magnitude;
                }
                count += print_word(magnitude, 10);
            }
        } else if (c == 'u' || c == 'x') {
            unsigned int base = c == 'x' ? 16 : 10;
            if (wide) count += print_long(va_arg(arguments, unsigned long), base);
            else count += print_word(va_arg(arguments, unsigned int), base);
        } else if (c == '%') {
            emit_character('%'); count++;
        } else {
            emit_character('%'); emit_character(c); count += 2;
        }
    }
    va_end(arguments);
    if (!os6_exit_flush) flush_output();
    return count;
}
