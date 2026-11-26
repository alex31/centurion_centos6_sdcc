#include <stdio.h>
#include <os6_curses.h>
#include "game.h"

static C4Board board;
static char input[8];

static void draw_board(void)
{
    static const char border[] = " +---+---+---+---+---+---+---+";
    int row;
    os6_curses_clear();
    os6_curses_move(1, 1); printf("CONNECT FOUR");
    os6_curses_move(2, 1); printf("You: X     Computer: O");
    os6_curses_move(4, 1); printf("   1   2   3   4   5   6   7");
    os6_curses_move(5, 1); printf("%s", border);
    for (row = 0; row < C4_ROWS; row++) {
        os6_curses_move(6 + row, 1);
        printf(" |   |   |   |   |   |   |   |");
    }
    os6_curses_move(12, 1); printf("%s", border);
}

static void draw_token(int row, int column, char token)
{
    os6_curses_move(6 + row, 4 + 4 * column);
    printf("%c", token);
}

static void message(const char *s)
{
    os6_curses_move(14, 1);
    os6_curses_eol();
    printf("%s", s);
}

static int ask(const char *question)
{
    os6_curses_move(15, 1);
    os6_curses_eol();
    printf("%s", question);
    return os6_getline(input, sizeof(input));
}

int main(void)
{
    int row, column, finished, playing = 1;
    if (os6_curses_select() < 0)
        return 1;
    while (playing) {
        c4_init(&board);
        draw_board();
        message("Your turn.");
        finished = 0;
        while (!finished) {
            if (ask("Column (1-7), or Q to quit: ") < 0) return 1;
            if ((input[0] == 'q' || input[0] == 'Q') && !input[1]) {
                playing = 0;
                break;
            }
            if (input[0] < '1' || input[0] > '7' || input[1]) {
                message("Enter a digit from 1 to 7, or Q.");
                continue;
            }
            column = input[0] - '1';
            row = c4_play(&board, column, C4_HUMAN);
            if (row < 0) {
                message("That column is full. Choose another one.");
                continue;
            }
            draw_token(row, column, 'X');
            if (c4_won(&board, row, column)) {
                message("You win!");
                finished = 1;
                continue;
            }
            if (board.moves == C4_ROWS * C4_COLUMNS) {
                message("Draw.");
                finished = 1;
                continue;
            }
            column = c4_choose(&board);
            row = c4_play(&board, column, C4_COMPUTER);
            if (row < 0) return 2; /* Inconsistent board state. */
            draw_token(row, column, 'O');
            if (c4_won(&board, row, column)) {
                message("Computer wins.");
                finished = 1;
                continue;
            }
            if (board.moves == C4_ROWS * C4_COLUMNS) {
                message("Draw.");
                finished = 1;
                continue;
            }
            os6_curses_move(14, 1);
            os6_curses_eol();
            printf("Computer: column %d. Your turn.", column + 1);
        }
        if (playing) {
            if (ask("Play again? (Y/N): ") < 0) return 1;
            playing = (input[0] == 'y' || input[0] == 'Y') && !input[1];
        }
    }
    os6_curses_clear();
    os6_curses_end();
    puts("End of Connect Four.");
    return 0;
}
