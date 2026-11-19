#include "play.h"

static ChessMove legal[256];
/* Exact position records, not hashes: no collision can declare a false draw.
   At most 100 reversible plies precede the application's fifty-move draw. */
static uint8_t history[101][35];
static unsigned history_count, quiet_plies, move_count, status;

static unsigned insufficient(void)
{
    unsigned square, piece, minors=0, knights=0, colors=0;
    for (square=0;square<128;square++) {
        if (square&8) continue;
        piece=chess_piece((uint8_t)square)&7;
        if (!piece || piece==MCUMAX_KING) continue;
        if (piece==MCUMAX_KNIGHT) knights++;
        else if (piece==MCUMAX_BISHOP) colors |= 1U<<(((square>>4)+(square&7))&1);
        else return 0;
        minors++;
    }
    return minors<=1 || (!knights && colors!=3);
}

static void record(void)
{
    uint8_t key[35], square, i, piece, ep=MCUMAX_SQUARE_INVALID;
    unsigned n, equal, matches=0;
    for (i=0;i<32;i++) {
        square=(uint8_t)((i/4)*16+(i%4)*2);
        piece=chess_piece(square);
        key[i]=(uint8_t)((piece<<4)|chess_piece(square+1));
    }
    /* An e.p. square affects repetition only when a legal e.p. capture exists. */
    for (n=0;n<move_count;n++) {
        piece=chess_piece(legal[n].from)&7;
        if ((piece==MCUMAX_PAWN_UPSTREAM || piece==MCUMAX_PAWN_DOWNSTREAM) &&
            ((legal[n].from^legal[n].to)&7) && !chess_piece(legal[n].to)) ep=mcumax_ep();
    }
    key[32]=chess_side();key[33]=mcumax_rights();key[34]=ep;
    for (n=0;n<history_count;n++) {
        equal=1;
        for (i=0;i<35;i++) if (history[n][i]!=key[i]) {equal=0;break;}
        matches+=equal;
    }
    if (matches>=2 && status==CHESS_PLAYING) status=CHESS_REPETITION;
    if (history_count<101) {
        for (i=0;i<35;i++) history[history_count][i]=key[i];
        history_count++;
    }
}

static void refresh(void)
{
    uint32_t count=chess_moves(legal,256);
    move_count=(unsigned)count;
    status=CHESS_PLAYING;
    if (count>256) {status=CHESS_ERROR;move_count=0;return;}
    if (!count) status=mcumax_in_check()?CHESS_MATE:CHESS_STALEMATE;
    else if (quiet_plies>=100) status=CHESS_FIFTY;
    else if (insufficient()) status=CHESS_MATERIAL;
    record();
}
void game_start(void)
{ chess_start();history_count=quiet_plies=0;refresh(); }
void game_position(const char *fen, unsigned quiet)
{ chess_fen(fen);history_count=0;quiet_plies=quiet;refresh(); }
unsigned game_status(void) {return status;}
unsigned game_checked(void) {return mcumax_in_check();}
unsigned game_legal(void) {return move_count;}

unsigned game_apply(uint8_t from, uint8_t to)
{
    unsigned i,pawn,capture;
    if (status!=CHESS_PLAYING) return 0;
    for (i=0;i<move_count;i++) if (legal[i].from==from && legal[i].to==to) break;
    if (i==move_count) return 0;
    pawn=chess_piece(from)&7;
    pawn=pawn==MCUMAX_PAWN_UPSTREAM || pawn==MCUMAX_PAWN_DOWNSTREAM;
    capture=chess_piece(to)!=0;
    if (!chess_play(from,to)) {status=CHESS_ERROR;return 0;}
    if (pawn || capture) {quiet_plies=0;history_count=0;}
    else quiet_plies++;
    refresh();return 1;
}
unsigned game_move(const char *text)
{
    unsigned i;uint8_t from,to;char chars[5];
    for (i=0;i<4;i++) {
        chars[i]=text[i];if (!chars[i]) return 0;
        if (chars[i]>='A' && chars[i]<='H') chars[i]+='a'-'A';
    }
    if (chars[0]<'a'||chars[0]>'h'||chars[2]<'a'||chars[2]>'h'||
        chars[1]<'1'||chars[1]>'8'||chars[3]<'1'||chars[3]>'8') return 0;
    from=(uint8_t)(('8'-chars[1])*16+chars[0]-'a');
    to=(uint8_t)(('8'-chars[3])*16+chars[2]-'a');
    if (text[4]) {
        unsigned piece=chess_piece(from)&7;
        if ((text[4]!='q'&&text[4]!='Q')||text[5]||
            (piece!=MCUMAX_PAWN_UPSTREAM&&piece!=MCUMAX_PAWN_DOWNSTREAM)||
            ((to&0x70)!=0&&(to&0x70)!=0x70)) return 0;
    }
    return game_apply(from,to);
}
void game_choice(ChessMove *move)
{
    unsigned i;
    chess_best(32,1,move);
    for (i=0;i<move_count;i++) if (legal[i].from==move->from&&legal[i].to==move->to) return;
    if (move_count) {move->from=legal[0].from;move->to=legal[0].to;}
    else {move->from=MCUMAX_SQUARE_INVALID;move->to=MCUMAX_SQUARE_INVALID;}
}
