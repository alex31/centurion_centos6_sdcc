#ifndef CPU6_OS6_STDIO_H
#define CPU6_OS6_STDIO_H
#define EOF (-1)
#define OS6_LINE_TOO_LONG (-2)
#define OS6_LINE_INVALID (-3)

int getchar(void);
/* Discard the remainder of the line started by getchar(). */
void os6_discard_line(void);
/* Read a complete text line, excluding the newline, into a bounded buffer.
   A rejected line is consumed and leaves an empty buffer. */
int os6_getline_checked(char *buffer, unsigned int capacity);
int putchar(int c);
int puts(const char *s);
int printf(const char *format, ...);
/* Optional printf-only batching. Disable or flush before other console I/O. */
void os6_printf_buffered(int enabled);
void os6_printf_flush(void);
int os6_getline(char *buffer, unsigned int capacity);
char *gets(char *buffer);
#endif
