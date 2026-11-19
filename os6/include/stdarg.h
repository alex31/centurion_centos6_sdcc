#ifndef CPU6_OS6_STDARG_H
#define CPU6_OS6_STDARG_H
/* CPU6 Fuzix arguments are stacked right-to-left in whole words. */
typedef char *va_list;
#define __va_size(t) ((sizeof(t) + 1) & ~1)
#define va_start(ap, last) ((ap) = (char *)&(last) + __va_size(last))
#define va_arg(ap, type) (*(type *)(void *)((ap += __va_size(type)) - __va_size(type)))
#define va_end(ap)
#endif
