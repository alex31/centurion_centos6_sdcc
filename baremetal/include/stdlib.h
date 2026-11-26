#ifndef CPU6_STDLIB_H
#define CPU6_STDLIB_H

#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned int size_t;
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif

#define RAND_MAX 32767
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

int rand(void);
void srand(unsigned int seed);
long strtol(const char *text, char **end, int base);
void qsort(void *base, size_t count, size_t size,
           int (*compare)(const void *, const void *));
void exit(int status);

#endif
