#include "engine.h"

void chess_start(void) { mcumax_init(); }
void chess_fen(const char *fen) { mcumax_set_fen_position(fen); }
uint8_t chess_piece(uint8_t square)
{
    uint8_t piece = mcumax_get_piece(square);
    return (piece & 7) ? piece : 0;
}
uint8_t chess_side(void) { return mcumax_get_current_side() == 8 ? 0 : 1; }
uint32_t chess_moves(ChessMove *moves, uint32_t capacity)
{ return mcumax_search_valid_moves(moves, capacity); }
void chess_best(uint32_t nodes, uint32_t depth, ChessMove *move)
{ mcumax_search_best_move(nodes, depth, move); }
uint8_t chess_play(uint8_t from, uint8_t to) { return mcumax_play_move(from, to); }
uint32_t chess_nodes(void) { return mcumax_get_nodes(); }
void chess_callback(mcumax_callback callback, void *data) { mcumax_set_callback(callback, data); }
void chess_stop(void) { mcumax_stop_search(); }
