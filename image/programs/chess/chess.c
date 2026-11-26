#include <stdio.h>
#include <os6.h>
#include <os6_curses.h>
#include "play.h"

static char input[76];
static unsigned callback_count, searching;
static unsigned long began, now;
static unsigned flipped, last_computer;
static char last_move[5];
static char displayed[8][8];
void c4_clock_read(unsigned long *value);

static void cooperate(void *unused)
{
    (void)unused;
    ++callback_count;
    os6_checkpoint();
    if (searching) {
        c4_clock_read(&now);
        if (now<began) now+=864000UL;
        if (now-began>=30UL || callback_count>=8192) chess_stop();
    }
}
static int command(const char *word)
{
    unsigned i;char c;
    for (i=0;word[i];i++) {
        c=input[i];if(c>='a'&&c<='z')c-='a'-'A';
        if(c!=word[i])return 0;
    }
    return !input[i];
}
static void message(unsigned row,const char *text)
{
    os6_curses_move(row, 1);
    os6_curses_eol();
    printf("%s", text);
}
static int ask(unsigned row,const char *text)
{message(row,text);return os6_getline(input,sizeof(input));}
static char square_token(unsigned rank, unsigned file)
{
    unsigned r = flipped ? 7 - rank : rank;
    unsigned f = flipped ? 7 - file : file;
    unsigned piece = chess_piece((uint8_t)(r * 16 + f));
    char token = " PPNKBRQ"[piece & 7];
    return piece & 8 ? token + 'a' - 'A' : token;
}

static void paint_square(unsigned rank, unsigned file, char token)
{
    int reverse = ((rank + file) & 1) == 0;
    if (reverse)
        os6_curses_reverse(1);
    printf(" %c ", token);
    if (reverse)
        os6_curses_reverse(0);
    displayed[rank][file] = token;
}

static void draw_board(void)
{
    unsigned rank,file,r;
    static const char *legend[]={"P/p  Pawn","N/n  Knight","B/b  Bishop","R/r  Rook","Q/q  Queen","K/k  King"};
    os6_curses_reverse(0);
    os6_curses_clear();
    printf("CHESS - Centurion CPU6");
    message(2,"White: uppercase   Black: lowercase");
    os6_curses_move(3,6);
    for(file=0;file<8;file++)printf("%c   ",flipped?'h'-file:'a'+file);
    for(rank=0;rank<8;rank++){
        r=flipped?7-rank:rank;
        os6_curses_move(4+2*rank,1);printf("   +---+---+---+---+---+---+---+---+");
        os6_curses_move(5+2*rank,1);printf(" %u |",8-r);
        for(file=0;file<8;file++){
            paint_square(rank,file,square_token(rank,file));
            putchar('|');
        }
        printf(" %u",8-r);
    }
    os6_curses_move(20,1);printf("   +---+---+---+---+---+---+---+---+");
    os6_curses_move(21,6);for(file=0;file<8;file++)printf("%c   ",flipped?'h'-file:'a'+file);
    for(rank=0;rank<6;rank++){os6_curses_move(5+2*rank,44);printf("%s",legend[rank]);}
    os6_curses_move(18,44);printf("Move format: e2e4");
    os6_curses_move(19,44);printf("HELP  Commands");
    os6_curses_move(20,44);printf("NEW   New game   RESIGN");
    os6_curses_move(21,44);printf("Q     Exit");
}

static void update_board(void)
{
    unsigned rank, file;
    for (rank = 0; rank < 8; rank++) {
        for (file = 0; file < 8; file++) {
            char token = square_token(rank, file);
            if (token != displayed[rank][file]) {
                os6_curses_move(5 + 2 * rank, 5 + 4 * file);
                paint_square(rank, file, token);
            }
        }
    }
    os6_curses_move(24, 1);
    os6_curses_eol();
    printf("%s %s", last_computer ? "Computer plays" : "Last move:", last_move);
}
static void remember(uint8_t from,uint8_t to,unsigned computer)
{
    last_move[0]=(char)('a'+(from&7));last_move[1]=(char)('8'-(from>>4));
    last_move[2]=(char)('a'+(to&7));last_move[3]=(char)('8'-(to>>4));last_move[4]=0;
    last_computer=computer;
}
static void ending(void)
{
    switch(game_status()){
    case CHESS_MATE:message(22,chess_side()?"Checkmate. White wins.":"Checkmate. Black wins.");break;
    case CHESS_STALEMATE:message(22,"Draw: stalemate.");break;
    case CHESS_REPETITION:message(22,"Draw: threefold repetition.");break;
    case CHESS_FIFTY:message(22,"Draw: fifty-move rule.");break;
    case CHESS_MATERIAL:message(22,"Draw: insufficient material.");break;
    default:message(22,"Engine error.");
    }
}
int main(void)
{
    unsigned human=0, two=0, restart=1, result;
    ChessMove move;
    if (os6_curses_init(OS6_CURSES_AUTO) < 0)
        return 1;
    chess_callback(cooperate,0);
    while(restart){
        os6_curses_reverse(0);
        os6_curses_clear();
        printf("CHESS - mcu-max 1.0.6 (MIT)");
        message(3,"Moves: e2e4, e1g1 for castling; promotion is always a queen.");
        if(ask(5,"Play White, Black, or 2 players? (W/B/2/Q): ")<0)return 1;
        if(command("Q"))break;
        if(!command("W")&&!command("B")&&!command("2"))continue;
        two=command("2");human=command("B");flipped=human&&!two;last_move[0]=0;game_start();
        draw_board();
        while(1){
            if(last_move[0])update_board();
            if(game_status()!=CHESS_PLAYING){ending();break;}
            os6_curses_move(22,1);
            os6_curses_eol();
            printf("%s to move%s.",chess_side()?"Black":"White",game_checked()?" - CHECK":"");
            if(!two && chess_side()!=human){
                message(23,"Computer thinking...");callback_count=0;c4_clock_read(&began);searching=1;
                game_choice(&move);searching=0;
                remember(move.from,move.to,1);
                if(!game_apply(move.from,move.to)){message(22,"Engine error.");return 2;}
                continue;
            }
            do {
                /* Leave room for the complete 75-character CENTOS input. */
                if(ask(23,"> ")<0)return 1;
                if(command("Q")){restart=0;break;}
                if(command("NEW"))break;
                if(command("RESIGN")){message(22,chess_side()?"Black resigns. White wins.":"White resigns. Black wins.");break;}
                if(command("HELP")){
                    message(24,"e2e4 | Castling: e1g1/e1c1 | Promotion: a7a8q | NEW/Q");
                    result=0;continue;
                }
                result=game_move(input);
                if(!result)message(24,"Illegal move or invalid notation.");
                else{
                    unsigned i;for(i=0;i<4;i++)last_move[i]=(input[i]>='A'&&input[i]<='H')?input[i]+'a'-'A':input[i];
                    last_move[4]=0;last_computer=0;
                }
            }while(!result);
            if(!restart)break;
            if(command("NEW")||command("RESIGN"))break;
        }
        if(!restart)break;
        do {
            if(ask(23,"New game? (Y/N): ")<0)return 1;
        }while(!command("Y")&&!command("N"));
        restart=command("Y");
    }
    os6_curses_end();
    message(24,"End of Chess.");putchar(10);return 0;
}
