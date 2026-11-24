/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <string.h>

int memcmp(const void *left, const void *right, size_t n)
{
    const unsigned char *a = left, *b = right;
    while (n--) {
        int difference = *a++ - *b++;
        if (difference) return difference;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        ++p;
    }
    return NULL;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++) ++len;
    return len;
}

size_t strnlen(const char *s, size_t n)
{
    size_t len = 0;
    while (len < n && s[len]) ++len;
    return len;
}

char *strcpy(char *dest, const char *src)
{
    char *out = dest;
    while ((*out++ = *src++) != '\0') {}
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    char *out = dest;
    while (n--) {
        char c = *src;
        *out++ = c;
        if (c) ++src;
    }
    return dest;
}

char *strcat(char *dest, const char *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *out = dest + strlen(dest);
    while (n-- && *src) *out++ = *src++;
    *out = '\0';
    return dest;
}

size_t strlcpy(char *dest, const char *src, size_t capacity)
{
    size_t len = strlen(src);
    if (capacity) {
        size_t copy = len < capacity ? len : capacity - 1;
        memcpy(dest, src, copy);
        dest[copy] = '\0';
    }
    return len;
}

size_t strlcat(char *dest, const char *src, size_t capacity)
{
    size_t used = strnlen(dest, capacity);
    if (used == capacity) return used + strlen(src);
    return used + strlcpy(dest + used, src, capacity - used);
}

int strcmp(const char *left, const char *right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return (unsigned char)*left - (unsigned char)*right;
}

int strncmp(const char *left, const char *right, size_t n)
{
    while (n--) {
        unsigned char a = (unsigned char)*left++, b = (unsigned char)*right++;
        if (a != b || !a) return (int)a - b;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    for (;;) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return NULL;
        ++s;
    }
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char *)last;
}

char *strstr(const char *s, const char *needle)
{
    if (!*needle) return (char *)s;
    while ((s = strchr(s, *needle)) != NULL) {
        const char *a = s + 1, *b = needle + 1;
        while (*b && *a == *b) {
            ++a;
            ++b;
        }
        if (!*b) return (char *)s;
        ++s;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;
    while (s[n] && strchr(accept, s[n])) ++n;
    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n])) ++n;
    return n;
}

char *strpbrk(const char *s, const char *accept)
{
    s += strcspn(s, accept);
    return *s ? (char *)s : NULL;
}

char *strtok_r(char *s, const char *delim, char **saveptr)
{
    char *end;
    if (!s) s = *saveptr;
    if (!s) return NULL;
    s += strspn(s, delim);
    if (!*s) {
        *saveptr = NULL;
        return NULL;
    }
    end = s + strcspn(s, delim);
    if (*end) *end++ = '\0';
    *saveptr = end;
    return s;
}

char *strtok(char *s, const char *delim)
{
    static char *next;
    return strtok_r(s, delim, &next);
}

char *strsep(char **stringp, const char *delim)
{
    char *field = *stringp;
    if (field) {
        char *next = strpbrk(field, delim);
        if (next) *next++ = '\0';
        *stringp = next;
    }
    return field;
}
