#ifndef CPU6_OS6_CURSES_H
#define CPU6_OS6_CURSES_H

enum os6_terminal {
    OS6_CURSES_VT100,
    OS6_CURSES_ADDS_REGENT_100,
    OS6_CURSES_AUTO
};

enum os6_curses_key {
    OS6_KEY_UP = 256,
    OS6_KEY_DOWN,
    OS6_KEY_RIGHT,
    OS6_KEY_LEFT,
    OS6_KEY_PAGE_UP,
    OS6_KEY_PAGE_DOWN,
    OS6_KEY_UNKNOWN
};

/* AUTO queries the terminal and offers a manual choice if it stays silent. */
int os6_curses_init(enum os6_terminal terminal);
int os6_curses_select(void);

/* Screen coordinates are one-based, from (1, 1) to (24, 80). */
int os6_curses_move(unsigned int row, unsigned int column);
int os6_curses_column(unsigned int column);
void os6_curses_clear(void);
void os6_curses_eol(void);
void os6_curses_reverse(int enabled);
void os6_curses_keypad(int enabled);
void os6_curses_end(void);
int os6_curses_getch(void);

/* Read one ASCII key through the CENTOS console, including its native echo. */
int os6_read_key(void);

#endif
