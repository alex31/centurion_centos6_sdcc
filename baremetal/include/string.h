#ifndef CPU6_STRING_H
#define CPU6_STRING_H

#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned int size_t;
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif

void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int c, size_t n);
int memcmp(const void *left, const void *right, size_t n);
void *memchr(const void *s, int c, size_t n);

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
size_t strlcpy(char *dest, const char *src, size_t capacity);
size_t strlcat(char *dest, const char *src, size_t capacity);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *s, const char *needle);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *s, const char *accept);
char *strtok(char *s, const char *delim);
char *strtok_r(char *s, const char *delim, char **saveptr);
char *strsep(char **stringp, const char *delim);

#endif
