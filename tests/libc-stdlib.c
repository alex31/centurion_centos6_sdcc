#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __CPU6_OS6__
#include <os6_curses.h>
#include <time.h>
#endif

#if RAND_MAX != 32767
#error RAND_MAX must match the CPU6 rand() range
#endif

struct item {
    signed char key;
    unsigned char check, tag;
};

static struct item list[] = {
    {3, 7, 1}, {-8, 9, 2}, {0, 11, 3}, {7, 13, 4}, {3, 15, 5},
    {-4, 17, 6}, {1, 19, 7}, {-8, 21, 8}, {2, 23, 9}, {6, 25, 10}
};

static int compare(const void *a, const void *b)
{
    int left = ((const struct item *)a)->key;
    int right = ((const struct item *)b)->key;
    return (left > right) - (left < right);
}

static int sorted(void)
{
    static const signed char keys[] = {-8, -8, -4, 0, 1, 2, 3, 3, 6, 7};
    unsigned int seen = 0;
    size_t i;

    qsort(list, 0, sizeof(list[0]), compare);
    qsort(list, 1, sizeof(list[0]), compare);
    if (list[0].tag != 1)
        return 0;
    qsort(list, sizeof(list) / sizeof(list[0]), sizeof(list[0]), compare);
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        unsigned int tag = list[i].tag;
        if (list[i].key != keys[i] || tag < 1 || tag > 10 || list[i].check != 2 * tag + 5)
            return 0;
        seen |= 1u << (tag - 1);
    }
    return seen == (1u << 10) - 1;
}

static int random_sequence(void)
{
    static const int expected[] = {16838, 5758, 10113, 17515};
    size_t i;

    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
        if (rand() != expected[i])
            return 0;
    srand(1);
    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
        if (rand() != expected[i])
            return 0;
    srand(0);
    return rand() == 0;
}

static int integer_parsing(void)
{
    static const struct {
        const char *text;
        int base, offset, error;
        long value;
    } tests[] = {
        {" \t-42xyz", 10, 5, 0, -42},
        {"0X7f", 0, 4, 0, 127},
        {"077", 0, 3, 0, 63},
        {"zZ", 36, 2, 0, 1295},
        {"2147483647", 10, 10, 0, 2147483647L},
        {"-2147483648", 10, 11, 0, -2147483647L - 1},
        {"2147483648000!", 10, 13, ERANGE, 2147483647L},
        {"-2147483649!", 10, 11, ERANGE, -2147483647L - 1},
        {" +", 10, 0, 0, 0},
        {"", 10, 0, 0, 0},
        {"xyz", 10, 0, 0, 0},
        {"0x", 16, 1, 0, 0},
        {"7", 1, 0, EINVAL, 0},
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        char *end;
        long value;
        errno = 0;
        value = strtol(tests[i].text, &end, tests[i].base);
        if (value != tests[i].value || errno != tests[i].error || end != tests[i].text + tests[i].offset)
            return 0;
    }
    return strtol("1", NULL, 10) == 1;
}

#ifdef __CPU6_OS6__
static int repeated_line(int character, unsigned int length)
{
    while (length--)
        if (getchar() != character)
            return 0;
    return getchar() == '\n';
}

static int console_input(void)
{
    int character;

    puts("LIBC INPUT");
    if (getchar() != 'A' || getchar() != ' ' || getchar() != 'B' || getchar() != '\n' ||
        getchar() != '\n' || getchar() != 'Z' || getchar() != '\n')
        return 0;
    puts("LIBC LONG INPUT");
    if (!repeated_line('X', 74) || !repeated_line('Y', 75) || !repeated_line('Z', 151))
        return 0;
    puts("LIBC BYTES");
    if (getchar() != 'A' || getchar() != 0 || getchar() != '\t' || getchar() != 'B' ||
        getchar() != ' ' || getchar() != ' ' || getchar() != '\n')
        return 0;
    puts("LIBC EDITED LINE");
    if (!repeated_line('B', 1) || !repeated_line('B', 1))
        return 0;
    puts("LIBC PRINTABLE");
    for (character = ' '; character <= '~'; ++character) {
        /* On the native CRT, '_' erases the preceding character. */
        if (character == '^' || character == '_')
            continue;
        if (getchar() != character)
            return 0;
    }
    if (getchar() != '\n')
        return 0;
    os6_discard_line();
    puts("LIBC DISCARD");
    if (getchar() != 'A')
        return 0;
    os6_discard_line();
    os6_discard_line();
    puts("LIBC NEXT LINE");
    return repeated_line('Z', 1);
}

static int checked_input(void)
{
    char line[16], *end;
    char small[] = "a!";

    puts("LIBC CHECKED INPUT");
    if (os6_getline_checked(NULL, 0) != EOF ||
        os6_getline_checked(line, sizeof(line)) != OS6_LINE_INVALID || line[0] ||
        os6_getline_checked(line, sizeof(line)) != OS6_LINE_TOO_LONG || line[0])
        return 0;
    if (os6_getline_checked(line, sizeof(line)) != 10)
        return 0;
    errno = 0;
    if (strtol(line, &end, 10) != 2147483647L || errno != ERANGE || *end)
        return 0;
    errno = 0;
    if (os6_getline_checked(line, sizeof(line)) != 3 || strtol(line, &end, 10) != 23 ||
        errno || *end != 'x')
        return 0;
    if (os6_getline_checked(line, sizeof(line)) != 2 || strtol(line, &end, 10) != 42 ||
        errno || *end)
        return 0;
    return os6_getline_checked(small, 1) == 0 && !small[0] && small[1] == '!';
}

static int console_keys(void)
{
    puts("LIBC SINGLE KEY");
    if (os6_read_key() != 'x')
        return 0;
    if (os6_curses_init(OS6_CURSES_VT100) != 0)
        return 0;
    os6_curses_keypad(1);
    puts("LIBC CURSOR KEYS");
    if (os6_curses_getch() != OS6_KEY_UP || os6_curses_getch() != OS6_KEY_DOWN ||
        os6_curses_getch() != OS6_KEY_RIGHT || os6_curses_getch() != OS6_KEY_LEFT ||
        os6_curses_getch() != OS6_KEY_PAGE_UP || os6_curses_getch() != OS6_KEY_PAGE_DOWN)
        return 0;
    os6_curses_end();
    if (os6_curses_init(OS6_CURSES_ADDS_REGENT_100) != 0)
        return 0;
    puts("LIBC ADDS KEYS");
    if (os6_curses_getch() != OS6_KEY_UP || os6_curses_getch() != OS6_KEY_DOWN ||
        os6_curses_getch() != OS6_KEY_RIGHT || os6_curses_getch() != '\b' ||
        os6_curses_getch() != '\b')
        return 0;
    os6_curses_end();
    return 1;
}
#endif

int main(void)
{
    if (!sorted() || !random_sequence() || !integer_parsing()) {
        puts("FAIL stdlib");
        return EXIT_FAILURE;
    }
#ifdef __CPU6_OS6__
    {
        time_t saved;
        time_t value = time(&saved);
        time_t later = time(NULL);
        printf("TIME %lu %lu %lu\n", value, saved, later);
    }
    if (!console_input()) {
        puts("FAIL getchar");
        return EXIT_FAILURE;
    }
    if (!checked_input()) {
        puts("FAIL checked input");
        return EXIT_FAILURE;
    }
    if (!console_keys()) {
        puts("FAIL console keys");
        return EXIT_FAILURE;
    }
    os6_printf_buffered(1);
    printf("PASS stdlib");
    exit(23);
#elif defined(TEST_BARE_CONSOLE)
    if (getchar() != 'A' || getchar() != 255) {
        puts("FAIL getchar");
        return EXIT_FAILURE;
    }
    puts("PASS stdlib");
    exit(EXIT_SUCCESS);
#else
    puts("PASS stdlib");
    return EXIT_SUCCESS;
#endif
    puts("FAIL exit returned");
    return EXIT_FAILURE;
}
