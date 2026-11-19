#ifndef CPU6_CHESS_PLAY_H
#define CPU6_CHESS_PLAY_H
#include "engine.h"
#define CHESS_PLAYING 0
#define CHESS_MATE 1
#define CHESS_STALEMATE 2
#define CHESS_REPETITION 3
#define CHESS_FIFTY 4
#define CHESS_MATERIAL 5
#define CHESS_ERROR 6
void game_start(void);
void game_position(const char *fen, unsigned quiet);
unsigned game_status(void);
unsigned game_move(const char *text);
unsigned game_apply(uint8_t from, uint8_t to);
void game_choice(ChessMove *move);
unsigned game_checked(void);
unsigned game_legal(void);
#endif
