/* Host tests of the actual CPU6 peephole dataflow implementation. */
#include "common.h"
#include "cpu6/peep.h"
#include <assert.h>

static lineNode *lines(const char *const *text, unsigned count)
{
    lineNode *nodes = calloc(count, sizeof(*nodes));
    assert(nodes);
    for (unsigned i = 0; i < count; i++) {
        nodes[i].line = (char *)text[i];
        nodes[i].prev = i ? nodes + i - 1 : NULL;
        nodes[i].next = i + 1 < count ? nodes + i + 1 : NULL;
        nodes[i].isLabel = text[i][strlen(text[i]) - 1] == ':';
        nodes[i].isComment = text[i][0] == ';';
    }
    return nodes;
}

#define PROGRAM(...) const char *text[] = {__VA_ARGS__}; \
    lineNode *p = lines(text, sizeof(text) / sizeof(text[0]))

int main(void)
{
    {
        PROGRAM("nop", "ldb 1", "rsr");
        assert(cpu6notUsed("b", p, p));
        assert(!cpu6notUsed("x", p, p));
        assert(cpu6notUsed("of", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "xfr b,a", "ldb 1", "rsr");
        assert(!cpu6notUsed("b", p, p));
        assert(cpu6notUsed("a", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "clrb bl", "stb (_out)", "rsr");
        assert(!cpu6notUsed("b", p, p));
        assert(cpu6notUsed("bl", p, p));
        assert(!cpu6notUsed("bh", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "bz Lused", "ldb 7", "bra Ldone", "Lused:",
                "stb (_out)", "Ldone:", "ldb 0", "rsr");
        assert(!cpu6notUsed("b", p, p));
        assert(!cpu6notUsed("zf", p, p));
        assert(cpu6notUsed("a", p, p));
        assert(!cpu6notUsedFrom("b", "Lused", p));
        assert(cpu6notUsedFrom("b", "Ldone", p));
        assert(!cpu6notUsedFrom("a", "Lmissing", p));
        free(p);
    }
    {
        PROGRAM("nop", "bz Lleft", "ldb 1", "bra Ldone", "Lleft:",
                "ldb 2", "Ldone:", "rsr");
        assert(cpu6notUsed("b", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "Lloop:", "inr x", "bnz Lloop", "ldb 1", "rsr");
        assert(cpu6notUsed("b", p, p));
        assert(!cpu6notUsed("x", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "jsr _callee", "stx (_save)", "rsr");
        sym_link signature = {.xclass = DECLARATOR, .select.d.dcl_type = FUNCTION};
        symbol callee = {.type = &signature};
        operand target = {.type = SYMBOL, .svt.symOperand = &callee};
        iCode call = {.op = CALL, .left = &target};

        assert(!cpu6notUsed("a", p, p));
        assert(!cpu6notUsed("b", p, p));
        assert(!cpu6notUsed("x", p, p));
        assert(!cpu6notUsed("y", p, p));

        p[1].ic = &call;
        assert(cpu6notUsed("a", p, p));
        assert(cpu6notUsed("b", p, p));

        signature.funcAttrs.sdcccall = 1;
        assert(!cpu6notUsed("a", p, p));
        assert(!cpu6notUsed("b", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "jsr __mul", "ldb 0", "rsr");
        assert(!cpu6notUsed("b", p, p));
        assert(cpu6notUsed("cf", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "jsr __cclt", "ldb 0", "rsr");
        assert(!cpu6notUsed("of", p, p));
        assert(!cpu6notUsed("zf", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "lda 0", "bl Lcarry", "Lcarry:", "rsr");
        assert(!cpu6notUsed("cf", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "cla", "bl Lcarry", "Lcarry:", "rsr");
        assert(cpu6notUsed("cf", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "jmp _callee");
        assert(cpu6notUsed("nf", p, p));
        assert(!cpu6notUsed("a", p, p));
        assert(!cpu6notUsed("b", p, p));
        assert(!cpu6notUsed("x", p, p));
        assert(!cpu6notUsed("s", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "jmp Lmissing");
        assert(!cpu6notUsed("nf", p, p));
        free(p);
    }
    {
        PROGRAM("nop", "unknown a", "ldb 1", "rsr");
        assert(!cpu6notUsed("b", p, p));
        assert(cpu6instructionSize(p + 1) > 127);
        free(p);
    }
    {
        PROGRAM("nop", "ldb 1", "rsr");
        p[1].isInline = 1;
        assert(!cpu6notUsed("b", p, p));
        free(p);
    }
    assert(cpu6canAssign("a", "x", NULL));
    {
        PROGRAM("nop", "mul 7,a", "stb (_out)", "rsr");
        assert(cpu6notUsed("b", p, p));
        assert(!cpu6notUsed("a", p, p));
        assert(cpu6instructionSize(p + 1) == 4);
        free(p);
    }
    {
        PROGRAM("nop", "div b,a", "rsr");
        assert(!cpu6notUsed("b", p, p));
        assert(!cpu6notUsed("cf", p, p));
        assert(cpu6notUsed("of", p, p));
        assert(cpu6instructionSize(p + 1) == 2);
        free(p);
    }
    {
        PROGRAM("nop", "div b,47,a", "rsr");
        assert(!cpu6notUsed("a", p, p));
        assert(cpu6instructionSize(p + 1) == 4);
        free(p);
    }
    assert(!cpu6canAssign("s", "a", NULL));
    assert(cpu6canAssign("b", "8(s)", "stable-memory"));
    assert(cpu6canAssign("a", "(_value+2)", "stable-memory"));
    assert(!cpu6canAssign("b", "(s+)", "stable-memory"));
    assert(!cpu6canAssign("b", "(-s)", "stable-load"));
    assert(!cpu6canAssign("b", "(0xf200)", "stable-memory"));
    assert(!cpu6canAssign("b", "(b)", "independent-address"));
    assert(cpu6canAssign("b", "(s)", "independent-address"));
    assert(cpu6canAssign("ldab", "x", "autoindex-byte"));
    assert(cpu6canAssign("ldbb", "a", "autoindex-byte"));
    assert(!cpu6canAssign("ldab", "a", "autoindex-byte"));
    assert(!cpu6canAssign("stbb", "b", "autoindex-byte"));
    assert(!cpu6canAssign("lda", "x", "autoindex-byte"));
    assert(cpu6canAssign("stb", "y", "autoindex-word"));
    assert(!cpu6canAssign("ldx", "x", "autoindex-word"));
    assert(!cpu6canAssign("lda", "s", "autoindex-word"));
    assert(!cpu6canAssign("jsr", "x", "autoindex-any"));
    assert(cpu6canAssign("x", "-128", "autoindex-offset"));
    assert(cpu6canAssign("x", "127", "autoindex-offset"));
    assert(!cpu6canAssign("x", "128", "autoindex-offset"));
    assert(!cpu6canAssign("x", "-129", "autoindex-offset"));
    assert(!cpu6canAssign("x", "_symbol", "autoindex-offset"));
    assert(cpu6canAssign("x", "0(s)", "autoindex-spill"));
    assert(!cpu6canAssign("x", "(s+)", "autoindex-spill"));
    assert(!cpu6canAssign("x", "0(x)", "autoindex-spill"));
    assert(cpu6canAssign("ldbb", "stbb", "autoindex-spill-op"));
    assert(!cpu6canAssign("ldbb", "stb", "autoindex-spill-op"));
    {
        PROGRAM("ldbb (x+)", "lda 0(x+)", "stb -128(-y)");
        assert(cpu6instructionSize(p) == 2);
        assert(cpu6instructionSize(p + 1) == 3);
        assert(cpu6instructionSize(p + 2) == 3);
        free(p);
    }
    {
        PROGRAM("nop", "ldbb 12(x+)", "rsr");
        assert(!cpu6notUsed("x", p, p));
        assert(!cpu6notUsed("bh", p, p));
        assert(cpu6notUsed("bl", p, p));
        assert(cpu6notUsed("cf", p, p)); /* Return does not observe flags. */
        free(p);
    }
    {
        PROGRAM("nop", "addb al,bl", "bf Ldone", "Ldone:", "rsr");
        assert(!cpu6notUsed("bh", p, p));
        assert(!cpu6notUsed("bl", p, p));
        assert(cpu6notUsed("of", p, p));
        assert(cpu6instructionSize(p + 1) == 1);
        free(p);
    }
    {
        PROGRAM("nop", "andb al,bl", "bf Ldone", "Ldone:", "rsr");
        assert(!cpu6notUsed("of", p, p));
        assert(cpu6instructionSize(p + 1) == 1);
        free(p);
    }
    /* Lot 1-3 additions: bgz/ble, ldw/stw, push/pop, ivr 2-op, jmp @(b), 2-digit stack offsets */
    {
        PROGRAM("nop", "bgz Lpos", "ble Lpos", "Lpos:", "rsr");
        assert(!cpu6notUsed("nf", p, p));
        assert(!cpu6notUsed("zf", p, p));
        assert(cpu6notUsed("cf", p, p));
        assert(cpu6instructionSize(p + 1) == 2);
        assert(cpu6instructionSize(p + 2) == 2);
        free(p);
    }
    {
        PROGRAM("nop", "ldw y,18(s)", "stw y,20(s)", "rsr");
        assert(cpu6notUsed("y", p, p));
        assert(!cpu6notUsed("s", p, p));
        assert(cpu6instructionSize(p + 1) == 4);
        assert(cpu6instructionSize(p + 2) == 4);
        free(p);
    }
    {
        PROGRAM("nop", "push xh,zl", "pop xh,zl", "rsr");
        assert(!cpu6notUsed("x", p, p));
        assert(!cpu6notUsed("y", p, p));
        assert(!cpu6notUsed("z", p, p));
        assert(cpu6notUsed("a", p, p));
        assert(cpu6instructionSize(p + 1) == 2);
        assert(cpu6instructionSize(p + 2) == 2);
        free(p);
    }
    {
        PROGRAM("nop", "ivr b,2", "rsr");
        assert(!cpu6notUsed("b", p, p));
        assert(cpu6notUsed("a", p, p));
        assert(cpu6instructionSize(p + 1) == 2);
        free(p);
    }
    {
        PROGRAM("nop", "jmp @(b)");
        assert(!cpu6notUsed("b", p, p));
        assert(cpu6instructionSize(p + 1) == 2);
        free(p);
    }
    assert(cpu6canAssign("b", "16(s)", "stable-load"));
    assert(cpu6canAssign("y", "18(s)", "extended-load"));
    assert(!cpu6canAssign("b", "(0xf200)", "stable-load"));
    assert(!cpu6canAssign("y", "(0xf200)", "extended-load"));
    puts("PASS: CPU6 peephole register aliases, flags, branches, loops and calls");
    return 0;
}
