#include "game.h"
#include <stdint.h>
#ifdef C4_PROFILE
#include <stdio.h>
#endif

#ifdef __CPU6_OS6__
#include <os6.h>
#endif

/* Prefer the center when two moves have the same score. */
static const unsigned char column_order[C4_COLUMNS] = {3, 2, 4, 1, 5, 0, 6};
static const signed char row_delta[4] = {0, 1, 1, 1};
static const signed char column_delta[4] = {1, 0, 1, -1};

void c4_init(C4Board *board)
{
    int row, column;
    for (row = 0; row < C4_ROWS; row++)
        for (column = 0; column < C4_COLUMNS; column++)
            board->cells[row][column] = C4_EMPTY;
    for (column = 0; column < C4_COLUMNS; column++)
        board->heights[column] = 0;
    board->moves = 0;
}

int c4_play(C4Board *board, int column, int player)
{
    int row;
    if (column < 0 || column >= C4_COLUMNS ||
        (player != C4_HUMAN && player != C4_COMPUTER) ||
        board->heights[column] >= C4_ROWS)
        return -1;
    row = C4_ROWS - 1 - board->heights[column];
    board->cells[row][column] = player;
    board->heights[column]++;
    board->moves++;
    return row;
}

int c4_won(const C4Board *board, int row, int column)
{
    int direction, sign, r, c, count, player;
    if (row < 0 || row >= C4_ROWS || column < 0 || column >= C4_COLUMNS)
        return 0;
    player = board->cells[row][column];
    if (player == C4_EMPTY) return 0;
    for (direction = 0; direction < 4; direction++) {
#ifdef __CPU6_OS6__
        os6_checkpoint();
#endif
        count = 1;
        for (sign = -1; sign <= 1; sign += 2) {
            r = row + sign * row_delta[direction];
            c = column + sign * column_delta[direction];
            while (r >= 0 && r < C4_ROWS && c >= 0 && c < C4_COLUMNS &&
                   board->cells[r][c] == player) {
                count++;
                r += sign * row_delta[direction];
                c += sign * column_delta[direction];
            }
        }
        if (count >= 4) return 1;
    }
    return 0;
}

/* Search on seven compact columns. Bit zero is the bottom row. */
typedef struct {
    unsigned char human[C4_COLUMNS];
    unsigned char computer[C4_COLUMNS];
    unsigned char height[C4_COLUMNS];
    unsigned char moves;
} SearchPosition;
static SearchPosition search_position;
static const unsigned char height_bit[6] = {1,2,4,8,16,32};
static const unsigned char pop6[64] = {
    0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
    2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6
};
#define C4_WIN_SCORE 1000
#define C4_INFINITY 2000
#define C4_MAX_DEPTH 7
#define C4_TIME_LIMIT_TENTHS 30UL
#define C4_DAY_TENTHS 864000UL
#ifndef C4_NODE_LIMIT
#define C4_NODE_LIMIT 144
#endif
static unsigned int search_nodes;
static int search_aborted;
#ifdef __CPU6_OS6__
static unsigned long search_start;
static unsigned long search_now;
void c4_clock_read(unsigned long *value);
#endif
#ifdef C4_PROFILE
static int completed_depth;
#endif

/* Each byte holds one column. Reversing these lanes pairs the same four
   columns in the opposite order for the second diagonal. */
static inline uint16_t reverse_columns(uint16_t pair)
{
#ifdef __SDCC_cpu6
    return __builtin_bswap16(pair);
#else
    return (uint16_t)((pair << 8) | (pair >> 8));
#endif
}

static int position_win(int player, int column)
{
    int first, last, c;
    unsigned int a;
    unsigned char *p = player == C4_HUMAN ? search_position.human : search_position.computer;
    a = p[column];
    a &= a >> 1;
    if (a & (a >> 2)) return 1;
    first = column - 3;
    if (first < 0) first = 0;
    last = column;
    if (last > 3) last = 3;
    for (c = first; c <= last; c++) {
        /* Six occupied-row bits leave two padding bits in each byte. The
           shift by one clears bit 5 of the low lane, so a bit from the
           high lane cannot produce a match when the outer pair shifts by 3. */
        const uint16_t outer = ((uint16_t)p[c] << 8) | p[c + 3];
        const uint16_t inner = ((uint16_t)p[c + 1] << 8) | p[c + 2];
        const uint16_t horizontal = outer & inner;
        const uint16_t matching = outer & (inner >> 1);
        if ((horizontal & (horizontal >> 8)) ||
            (matching && (matching & (reverse_columns(inner) >> 2) &
                                     (reverse_columns(outer) >> 3))))
            return 1;
    }
    return 0;
}

static void position_play(int player, int column)
{
    unsigned int h = search_position.height[column];
    unsigned char *p = player == C4_HUMAN ? search_position.human : search_position.computer;
    /* Each row bit is unique; addition also avoids a byte-OR assembler bug. */
    p[column] = p[column] + height_bit[h];
    search_position.height[column] = h + 1;
    search_position.moves++;
}

static void position_undo(int player, int column)
{
    unsigned int h = --search_position.height[column];
    unsigned char *p = player == C4_HUMAN ? search_position.human : search_position.computer;
    p[column] = p[column] - height_bit[h];
    search_position.moves--;
}

static int count_wins(int player)
{
    int c, wins = 0;
    for (c = 0; c < C4_COLUMNS; c++) {
        if (search_position.height[c] >= C4_ROWS) continue;
        position_play(player, c);
        if (position_win(player, c)) wins++;
        position_undo(player, c);
        if (wins == 2) break;
    }
    return wins;
}

static int evaluate(int player)
{
    int opponent = 3 - player;
    int c, center, score = 0;
    unsigned int a, b, x, y;
    unsigned char *me = player == C4_HUMAN ? search_position.human : search_position.computer;
    unsigned char *them = opponent == C4_HUMAN ? search_position.human : search_position.computer;
    int wins = count_wins(player);
    if (wins) return C4_WIN_SCORE;
    wins = count_wins(opponent);
    if (wins > 1) return -C4_WIN_SCORE;
    if (wins == 1) score -= 25;
    center = (int)pop6[me[3]] - (int)pop6[them[3]];
    score += center + center + center;
    for (c = 0; c < 6; c++) {
        a = me[c]; b = me[c + 1]; x = them[c]; y = them[c + 1];
        score += (int)pop6[a & b] - (int)pop6[x & y];
        score += (int)pop6[a & (b >> 1)] - (int)pop6[x & (y >> 1)];
        score += (int)pop6[(a >> 1) & b] - (int)pop6[(x >> 1) & y];
    }
    return score;
}

static int search(int player, int depth, int alpha, int beta)
{
    int i, column, score, best = -C4_INFINITY;
    /* The node cap remains a backstop if the system clock is unavailable. */
    if (search_nodes >= C4_NODE_LIMIT) { search_aborted = 1; return 0; }
    search_nodes++;
#ifdef __CPU6_OS6__
    if ((search_nodes & 7) == 0) {
        os6_checkpoint();
        c4_clock_read(&search_now);
        if (search_now < search_start) search_now += C4_DAY_TENTHS;
        if (search_now - search_start >= C4_TIME_LIMIT_TENTHS) {
            search_aborted = 1;
            return 0;
        }
    }
#endif
    if (search_position.moves == 42) return 0;
    if (depth == 0) return evaluate(player);
    for (i = 0; i < C4_COLUMNS; i++) {
        column = column_order[i];
        if (search_position.height[column] >= C4_ROWS) continue;
        position_play(player, column);
        if (position_win(player, column)) score = C4_WIN_SCORE + depth;
        else score = -search(3 - player, depth - 1, -beta, -alpha);
        position_undo(player, column);
        if (search_aborted) return 0;
        if (score > best) best = score;
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }
    return best;
}

int c4_choose(C4Board *board)
{
    int c, h, player, i, depth, col, score, best, best_score, candidate;
#ifdef __CPU6_OS6__
    c4_clock_read(&search_start);
#endif
    for (c = 0; c < 7; c++) {
        search_position.human[c] = 0;
        search_position.computer[c] = 0;
    }
    for (c = 0; c < 7; c++) {
        search_position.height[c] = board->heights[c];
        for (h = 0; h < board->heights[c]; h++) {
            player = board->cells[5 - h][c];
            if (player == C4_HUMAN) search_position.human[c] += height_bit[h];
            if (player == C4_COMPUTER) search_position.computer[c] += height_bit[h];
        }
    }
    search_position.moves = board->moves;
    best = -1;
    for (i = 0; i < 7; i++) {
        c = column_order[i];
        if (search_position.height[c] < 6) { best = c; break; }
    }
    if (best < 0) return -1;
    for (player = C4_COMPUTER; player >= C4_HUMAN; player--) {
        for (i = 0; i < 7; i++) {
            c = column_order[i];
            if (search_position.height[c] >= 6) continue;
            position_play(player,c);
            score = position_win(player,c);
            position_undo(player,c);
            if (score) return c;
        }
    }
    search_nodes = 0;
    search_aborted = 0;
#ifdef C4_PROFILE
    completed_depth = 0;
#endif
    /* Keep the best move from the last fully searched depth if time runs out. */
    for (depth = 1; depth <= C4_MAX_DEPTH; depth++) {
        best_score = -C4_INFINITY;
        candidate = best;
        for (i = 0; i < 7; i++) {
            col = column_order[i];
            if (search_position.height[col] >= 6) continue;
            position_play(C4_COMPUTER,col);
            score = -search(C4_HUMAN,depth - 1,-C4_INFINITY,-best_score);
            position_undo(C4_COMPUTER,col);
            if (search_aborted) break;
            if (score > best_score) { best_score = score; candidate = col; }
        }
        if (search_aborted) break;
        best = candidate;
#ifdef C4_PROFILE
        completed_depth = depth;
#endif
        if (best_score >= C4_WIN_SCORE) break;
    }
#ifdef C4_PROFILE
    printf("DEPTH %d NODES %u\n", completed_depth, search_nodes);
#endif
    return best;
}
