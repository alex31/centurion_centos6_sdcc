#ifndef SDCC_STALE_USE_H
#define SDCC_STALE_USE_H

enum tokens { TOK_LEN = -96, TOK_ASC = 1, TOK_CHR = 2, TOK_STR = -29,
    TOK_VAL = -27, TOK_LEFT = 4, TOK_RIGHT = 3, TOK_MID = 5, TOK_NOT = -99 };
enum types { TYPE_BAD, TYPE_INT, TYPE_REAL, TYPE_STRING };
struct parser { signed char token; unsigned error; };

void emit(struct parser *, unsigned);
void advance(struct parser *);
unsigned expression_at(struct parser *, unsigned);
unsigned read_string_function(struct parser *, signed char);
unsigned real_function(signed char);
unsigned primary(struct parser *);

#endif
