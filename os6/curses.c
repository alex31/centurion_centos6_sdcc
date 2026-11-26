#include <os6_curses.h>
#include <stdio.h>

int os6_terminal_key(void);

static enum os6_terminal active_terminal = OS6_CURSES_VT100;

static int detect_terminal(void)
{
    int key;

    printf("\033[c\033\005");
    os6_printf_flush();
    for (;;) {
        int native;

        key = os6_terminal_key();
        if (key == EOF)
            return -1;
        native = key;
        key &= 127;
        if (key == 'v' || key == 'V' || key == '\r')
            return OS6_CURSES_VT100;
        if (key == 'a' || key == 'A')
            return OS6_CURSES_ADDS_REGENT_100;
        if (native == 0x82) {
            unsigned char status[10];
            unsigned int i;

            /* ADDS: STX, six status bytes, column, row, data, CR. */
            for (i = 0; i < 10; i++) {
                key = os6_terminal_key();
                if (key == EOF)
                    return -1;
                status[i] = key & 127;
                if (status[i] == '\r' && i < 9)
                    break;
            }
            if (i == 10 && status[6] >= 32 && status[6] < 112 &&
                status[7] >= 32 && status[7] < 56 && status[9] == '\r')
                return OS6_CURSES_ADDS_REGENT_100;
        }
        if (native == 5 || key == 27) {
            /* VT100 primary device attributes: CSI ? ... c. */
            if (os6_read_key() != '[' || os6_read_key() != '?')
                continue;
            key = os6_read_key();
            if (key < '0' || key > '9')
                continue;
            do { key = os6_read_key(); } while ((key >= '0' && key <= '9') || key == ';');
            if (key == 'c')
                return OS6_CURSES_VT100;
            if (key == EOF)
                return -1;
        }
    }
}

int os6_curses_init(enum os6_terminal terminal)
{
    switch (terminal) {
    case OS6_CURSES_VT100:
    case OS6_CURSES_ADDS_REGENT_100:
        active_terminal = terminal;
        return 0;
    case OS6_CURSES_AUTO: {
        int detected = detect_terminal();
        if (detected < 0)
            return -1;
        active_terminal = (enum os6_terminal)detected;
        return 0;
    }
    default:
        return -1;
    }
}

int os6_curses_select(void)
{
    char answer[8];

    for (;;) {
        int length;
        printf("Terminal [V]T100 / [A]DDS / A[U]TO (Enter for VT100): ");
        length = os6_getline_checked(answer, sizeof(answer));
        if (length == EOF)
            return -1;
        if (!length || (length == 1 && (answer[0] == 'v' || answer[0] == 'V')))
            return os6_curses_init(OS6_CURSES_VT100);
        if (length == 1 && (answer[0] == 'a' || answer[0] == 'A'))
            return os6_curses_init(OS6_CURSES_ADDS_REGENT_100);
        if (length == 1 && (answer[0] == 'u' || answer[0] == 'U'))
            return os6_curses_init(OS6_CURSES_AUTO);
        puts("Enter V, A, or U.");
    }
}

static int regent_column(unsigned int column)
{
    --column;
    return 16 * (column / 10) + column % 10;
}

int os6_curses_move(unsigned int row, unsigned int column)
{
    if (!row || row > 24 || !column || column > 80)
        return -1;
    if (active_terminal == OS6_CURSES_ADDS_REGENT_100)
        printf("\033Y%c%c", (int)(row + 31), (int)(column + 31));
    else
        printf("\033[%u;%uH", row, column);
    return 0;
}

int os6_curses_column(unsigned int column)
{
    if (!column || column > 80)
        return -1;
    if (active_terminal == OS6_CURSES_ADDS_REGENT_100)
        printf("\020%c", regent_column(column));
    else
        printf("\033[%uG", column);
    return 0;
}

void os6_curses_clear(void)
{
    printf("%s", active_terminal == OS6_CURSES_ADDS_REGENT_100 ? "\f" : "\033[2J\033[H");
}

void os6_curses_eol(void)
{
    printf("%s", active_terminal == OS6_CURSES_ADDS_REGENT_100 ? "\033K" : "\033[K");
}

void os6_curses_reverse(int enabled)
{
    if (active_terminal == OS6_CURSES_ADDS_REGENT_100)
        printf("\0330%c", enabled ? 'P' : '@');
    else
        printf("%s", enabled ? "\033[7m" : "\033[0m");
}

void os6_curses_keypad(int enabled)
{
    if (active_terminal == OS6_CURSES_VT100)
        printf("%s", enabled ? "\033[?1h" : "\033[?1l");
}

void os6_curses_end(void)
{
    os6_curses_keypad(0);
    os6_curses_reverse(0);
    os6_printf_flush();
}

static int ascii_key(int received)
{
    if (received == EOF)
        return EOF;
    /* CENTOS marks literal control characters with parity and returns
       editing keys such as Escape and Backspace as native key codes. */
    if (received == 5)
        return 27;
    if (received == 2 || (received & 127) == 127)
        return '\b';
    return received & 127;
}

int os6_read_key(void)
{
    os6_printf_flush();
    return ascii_key(os6_terminal_key());
}

int os6_curses_getch(void)
{
    int received, key;
    os6_printf_flush();
    received = os6_terminal_key();
    if (received == 4)
        return OS6_KEY_UP;
    key = ascii_key(received);
    if (active_terminal == OS6_CURSES_ADDS_REGENT_100) {
        /* CENTOS reports native Forward as function key 3. Native Back
           and Backspace both arrive as function key 2. */
        if (received == 3)
            return OS6_KEY_RIGHT;
        switch (key) {
        case 26: return OS6_KEY_UP;
        case 10: return OS6_KEY_DOWN;
        case 21: return OS6_KEY_LEFT;
        case 6: return OS6_KEY_RIGHT;
        }
    }
    if (key != 27)
        return key;
    key = os6_read_key();
    if (key != '[' && key != 'O')
        return key == EOF ? EOF : OS6_KEY_UNKNOWN;
    key = os6_read_key();
    if (key == '5' || key == '6') {
        int page = key;
        key = os6_read_key();
        if (key == '~')
            return page == '5' ? OS6_KEY_PAGE_UP : OS6_KEY_PAGE_DOWN;
    }
    while (key >= 32 && key < 64)
        key = os6_read_key();
    if (key >= 'A' && key <= 'D')
        return OS6_KEY_UP + key - 'A';
    return key == EOF ? EOF : OS6_KEY_UNKNOWN;
}
