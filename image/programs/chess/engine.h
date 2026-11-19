#ifndef CPU6_CHESS_ENGINE_H
#define CPU6_CHESS_ENGINE_H
#include "mcu-max.h"

/* Portable engine boundary shared by the benchmark and a future CENTOS UI.
   Squares are 0xRF: rank 0 is rank 8, file 0 is file a. Invalid = 0x80.
   Pieces: low 3 bits use MCUMAX_*, bit 3 means black; empty is normalized to 0.
   Only queen promotion is supported by the upstream engine. */
typedef mcumax_move ChessMove;
void chess_start(void);
void chess_fen(const char *fen);
uint8_t chess_piece(uint8_t square);
uint8_t chess_side(void); /* 0 = white, 1 = black */
uint32_t chess_moves(ChessMove *moves, uint32_t capacity);
void chess_best(uint32_t nodes, uint32_t depth, ChessMove *move);
uint8_t chess_play(uint8_t from, uint8_t to);
uint32_t chess_nodes(void);
void chess_callback(mcumax_callback callback, void *data);
void chess_stop(void);
#endif
