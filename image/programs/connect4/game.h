#ifndef CONNECT4_GAME_H
#define CONNECT4_GAME_H

#define C4_ROWS 6
#define C4_COLUMNS 7
#define C4_EMPTY 0
#define C4_HUMAN 1
#define C4_COMPUTER 2

typedef struct {
    unsigned char cells[C4_ROWS][C4_COLUMNS];
    unsigned char heights[C4_COLUMNS];
    unsigned char moves;
} C4Board;

void c4_init(C4Board *board);
/* Return the occupied row, or -1 if the column is unavailable. */
int c4_play(C4Board *board, int column, int player);
int c4_won(const C4Board *board, int row, int column);
/* Choose a column without changing the board. */
int c4_choose(C4Board *board);

#endif
