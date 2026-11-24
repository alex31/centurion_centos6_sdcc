#include <stdio.h>
#include <string.h>

/* Report the first failing expression, including when running under CENTOS. */
#define CHECK(expr) do { if (!(expr)) { puts("FAIL " #expr); return 1; } } while (0)

static unsigned char buf[270];

static int memory(void)
{
    static const unsigned char pattern[] = {0, 0x80, 0xff, 1};
    for (size_t i = 0; i < sizeof buf; ++i) buf[i] = (unsigned char)i;
    CHECK(memset(buf + 1, 0x1aa, 0) == buf + 1 && buf[1] == 1);
    CHECK(memset(buf + 1, 0x1aa, 1) == buf + 1 && buf[1] == 0xaa && buf[2] == 2);
    CHECK(memset(buf + 1, 0x1aa, 257) == buf + 1);
    for (size_t i = 1; i <= 257; ++i) CHECK(buf[i] == 0xaa);
    CHECK(buf[0] == 0 && buf[258] == 2);
    CHECK(memcpy(buf, pattern, sizeof pattern) == buf);
    CHECK(memcpy(buf, pattern + 1, 0) == buf && buf[0] == 0);
    CHECK(memcmp(buf, pattern, sizeof pattern) == 0);
    CHECK(memcmp(buf, pattern + 1, 0) == 0);
    CHECK(memcmp(pattern + 2, pattern + 3, 1) > 0);
    CHECK(memcmp(pattern, pattern + 1, 1) < 0);
    CHECK(memchr(pattern, 0x180, 2) == pattern + 1);
    CHECK(memchr(pattern, 255, 2) == NULL);
    CHECK(memchr(pattern, 0, 0) == NULL);

    for (size_t i = 0; i < sizeof buf; ++i) buf[i] = (unsigned char)i;
    CHECK(memmove(buf + 3, buf, 257) == buf + 3);
    for (size_t i = 0; i < sizeof buf; ++i)
        CHECK(buf[i] == (unsigned char)(i >= 3 && i < 260 ? i - 3 : i));
    CHECK(memmove(buf, buf + 3, 257) == buf);
    for (size_t i = 0; i < 257; ++i) CHECK(buf[i] == (unsigned char)i);
    CHECK(memmove(buf, buf, 5) == buf);
    CHECK(memmove(buf + 6, buf, 0) == buf + 6 && buf[6] == 6);
    CHECK(memmove(buf + 6, buf, 3) == buf + 6 && buf[8] == 2 && buf[9] == 9);
    return 0;
}

static int strings(void)
{
    char text[12] = "hello";
    const char *abc = "abc";
    const char high[] = {(char)0x80, 0};
    CHECK(strlen(text) == 5 && strlen("") == 0);
    CHECK(strnlen(text, 0) == 0 && strnlen(text, 3) == 3);
    CHECK(strcpy(text, "a") == text && !strcmp(text, "a"));
    CHECK(strcat(text, "bc") == text && !strcmp(text, abc));
    CHECK(strncat(text, "deFG", 2) == text && !strcmp(text, "abcde"));
    CHECK(strncat(text, "abc", 0) == text && strlen(text) == 5);
    CHECK(strcmp(high, "a") > 0 && strncmp("a", high, 1) < 0);
    CHECK(strncmp(text, "abcz", 3) == 0 && strncmp("", "abc", 0) == 0);
    CHECK(strncpy(text, "q", 4) == text && text[0] == 'q' && text[1] == 0 &&
          text[2] == 0 && text[3] == 0 && text[4] == 'e');
    CHECK(strncpy(text, abc, 2) == text && text[0] == 'a' && text[1] == 'b');
    CHECK(strncpy(text, "z", 0) == text && text[0] == 'a');
    CHECK(strchr(abc, 'a') == abc && strchr(abc, 0) == abc + 3);
    CHECK(strchr(abc, 'z') == NULL && strrchr(abc, 'z') == NULL);
    CHECK(strrchr(abc, 0) == abc + 3 && strchr(high, 0x180) == high);
    strcpy(text, "ababa");
    CHECK(strrchr(text, 'a') == text + 4);
    CHECK(strstr(text, "aba") == text && strstr(text, "bab") == text + 1);
    CHECK(strstr(text, "abaX") == NULL && strstr(text, "") == text);
    CHECK(strspn(abc, "") == 0 && strspn(abc, "acb") == 3);
    CHECK(strcspn(abc, "") == 3 && strcspn(abc, "xb") == 1);
    CHECK(strpbrk(abc, "zbc") == abc + 1 && strpbrk("", "z") == NULL);
    CHECK(strlcpy(text, abc, 0) == 3 && !strcmp(text, "ababa"));
    CHECK(strlcpy(text, abc, 1) == 3 && text[0] == 0);
    CHECK(strlcpy(text, abc, 3) == 3 && !strcmp(text, "ab"));
    CHECK(strlcat(text, "cdef", 5) == 6 && !strcmp(text, "abcd"));
    CHECK(strlcat(text, "ef", 2) == 4 && !strcmp(text, "abcd"));
    return 0;
}

static int tokens(void)
{
    char text[] = ",;alpha,,beta:gamma;";
    char outer[] = "x:y,z";
    char inner[] = "1/2";
    char separators[] = ";;;";
    char *a, *b, *t;

    t = strtok(text, ",;");
    CHECK(t == text + 2 && !strcmp(t, "alpha") && text[7] == 0);
    CHECK(strtok_r(outer, ":", &a) == outer && !strcmp(outer, "x"));
    CHECK(strtok_r(inner, "/", &b) == inner && !strcmp(inner, "1"));
    t = strtok(NULL, ",:");
    CHECK(t == text + 9 && !strcmp(t, "beta"));
    CHECK(!strcmp(strtok_r(NULL, ",", &a), "y"));
    CHECK(!strcmp(strtok_r(NULL, "/", &b), "2"));
    CHECK(!strcmp(strtok(NULL, ";"), "gamma"));
    CHECK(strtok(NULL, ";") == NULL && strtok(NULL, "") == NULL);
    CHECK(!strcmp(strtok_r(NULL, "", &a), "z"));
    CHECK(strtok_r(NULL, ",", &a) == NULL && strtok_r(NULL, "/", &b) == NULL);
    CHECK(strtok(separators, ";") == NULL);
    CHECK(strtok(separators, "") == separators && !strcmp(separators, ";;;"));
    CHECK(strtok(NULL, "") == NULL);
    separators[0] = 0;
    CHECK(strtok(separators, ",") == NULL);
    return 0;
}

static int fields(void)
{
    char line[] = ",a,,b;";
    char *p = line;
    CHECK(strsep(&p, ",;") == line && !line[0] && p == line + 1);
    CHECK(strsep(&p, ",;") == line + 1 && !strcmp(line + 1, "a"));
    CHECK(strsep(&p, ",;") == line + 3 && !line[3]);
    CHECK(strsep(&p, ";") == line + 4 && !strcmp(line + 4, "b"));
    CHECK(strsep(&p, ";") == line + 6 && !line[6] && p == NULL);
    CHECK(strsep(&p, ";") == NULL);
    p = line + 1;
    CHECK(strsep(&p, "") == line + 1 && !strcmp(line + 1, "a") && p == NULL);
    p = line;
    CHECK(strsep(&p, ",") == line && !line[0] && p == NULL);
    return 0;
}

int main(void)
{
    if (memory() || strings() || tokens() || fields()) return 1;
    puts("PASS libc memory, strings and tokenizers");
    return 0;
}
