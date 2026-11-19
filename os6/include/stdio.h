#ifndef CPU6_OS6_STDIO_H
#define CPU6_OS6_STDIO_H
int putchar(int c);
int puts(const char *s);
int printf(const char *format, ...);
/* Optional printf-only batching. Disable or flush before other console I/O. */
void os6_printf_buffered(int enabled);
void os6_printf_flush(void);
int os6_getline(char *buffer, unsigned int capacity);
char *gets(char *buffer);
#endif
