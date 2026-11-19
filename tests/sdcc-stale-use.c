/* A folded inline branch removes an indirect store. Its address use
   must leave the use set before constant propagation narrows the net. */
enum tokens { TOK_LEN = -96, TOK_ASC = 1, TOK_CHR = 2, TOK_STR = -29,
    TOK_VAL = -27, TOK_LEFT = 4, TOK_RIGHT = 3, TOK_MID = 5, TOK_NOT = -99 };
enum types { TYPE_BAD, TYPE_INT, TYPE_REAL, TYPE_STRING };
struct parser { signed char token; unsigned error; };
void emit(struct parser *, unsigned);
void advance(struct parser *);
unsigned expression_at(struct parser *, unsigned);
unsigned read_string_function(struct parser *, signed char);
unsigned real_function(signed char);

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
