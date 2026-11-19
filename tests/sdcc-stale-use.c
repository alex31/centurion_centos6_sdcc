/* A folded inline branch removes an indirect store. Its address use
   must leave the use set before constant propagation narrows the net. */
#include "sdcc-stale-use.h"

static void coerce(struct parser *p, unsigned from, unsigned to)
{
    if (from == to || p->error)
        return;
    if (from == TYPE_STRING || to == TYPE_STRING) {
        p->error = 3;
        return;
    }
    emit(p, to == TYPE_INT ? 31 : 29);
}

unsigned primary(struct parser *p)
{
    unsigned type;
    signed char token = p->token;
    switch (token) {
    case TOK_LEN: case TOK_ASC: case TOK_CHR: case TOK_STR:
    case TOK_VAL: case TOK_LEFT: case TOK_RIGHT: case TOK_MID:
        return read_string_function(p, token);
    default: break;
    }
    real_function(token);
    if (token == '+' || token == '-' || token == TOK_NOT) {
        advance(p);
        type = expression_at(p, token == TOK_NOT ? 3 : 6);
        if (type == TYPE_STRING) {
            p->error = 3;
            return TYPE_BAD;
        }
        if (token == '-')
            emit(p, type == TYPE_INT ? 10 : 24);
        if (token == TOK_NOT) {
            coerce(p, type, TYPE_INT);
            emit(p, 11);
            type = TYPE_INT;
        }
        return type;
    }
    if (!p->error)
        p->error = 1;
    return TYPE_BAD;
}
