#include <stdio.h>
#include <testfwk.h>
#include "sdcc-stale-use.h"

/* Minimal runtime for the selected upstream SDCC tests. */
int __numTests;
static unsigned failures;

void __fail(const char *message, const char *condition, const char *file, int line)
{
    (void)line;
    puts(message);
    puts(file);
    puts(condition);
    ++failures;
}

void testTortureExecute(void);
void testWrites(void);
void testMemberTypes(void);

static unsigned operand_type;
static unsigned emitted, first_opcode, last_opcode;

void emit(struct parser *parser, unsigned opcode)
{
    (void)parser;
    if (emitted == 0)
        first_opcode = opcode;
    last_opcode = opcode;
    ++emitted;
}

void advance(struct parser *parser)
{
    (void)parser;
}

unsigned expression_at(struct parser *parser, unsigned precedence)
{
    (void)parser;
    ASSERT(precedence == 3);
    return operand_type;
}

unsigned read_string_function(struct parser *parser, signed char token)
{
    (void)parser;
    (void)token;
    return TYPE_STRING;
}

unsigned real_function(signed char token)
{
    (void)token;
    return TYPE_BAD;
}

static void test_parser_stores_error(void)
{
    struct parser parser = {TOK_NOT, 0};
    unsigned *error = &parser.error;
    operand_type = TYPE_STRING;
    emitted = 0;
    ASSERT(primary(&parser) == TYPE_BAD);
    ASSERT(*error == 3);
    ASSERT(parser.token == TOK_NOT && emitted == 0);
}

static void test_parser_coerces_real(void)
{
    struct parser parser = {TOK_NOT, 0};
    operand_type = TYPE_REAL;
    emitted = 0;
    ASSERT(primary(&parser) == TYPE_INT);
    ASSERT(parser.error == 0);
    ASSERT(emitted == 2 && first_opcode == 31 && last_opcode == 11);
}

static void test_parser_preserves_error(void)
{
    struct parser parser = {'?', 9};
    emitted = 0;
    ASSERT(primary(&parser) == TYPE_BAD);
    ASSERT(parser.error == 9 && emitted == 0);
}

static unsigned store_and_read(unsigned *destination, const unsigned *observer,
                               unsigned value)
{
    *destination = value;
    return *observer;
}

/* Keep the alias relationship unknown when the compiler generates the callee. */
static unsigned (*volatile word_store)(unsigned *, const unsigned *, unsigned) = store_and_read;

static void test_word_alias(void)
{
    unsigned value = 0x1234, other = 0x5678;
    ASSERT(word_store(&value, &value, 0xabcd) == 0xabcd);
    ASSERT(word_store(&value, &other, 0x9abc) == 0x5678);
    ASSERT(value == 0x9abc);
}

static unsigned store_byte_and_read(unsigned char *destination, const unsigned *observer,
                                    unsigned char value)
{
    *destination = value;
    return *observer;
}

static unsigned (*volatile byte_store)(unsigned char *, const unsigned *, unsigned char) =
    store_byte_and_read;

static void test_character_alias(void)
{
    unsigned value = 0x1234;
    unsigned char *bytes = (unsigned char *)&value;
    ASSERT(byte_store(bytes + 1, &value, 0xab) == 0x12ab);
    ASSERT(byte_store(bytes, &value, 0xcd) == 0xcdab);
}

struct packet { unsigned char tag, payload[5], end; };

static void copy_packet(struct packet *destination, const struct packet *source)
{
    *destination = *source;
}

static void (*volatile packet_copy)(struct packet *, const struct packet *) = copy_packet;

static void test_odd_struct_copy(void)
{
    static const struct packet source = {6, {17, 28, 39, 50, 61}, 72};
    struct guarded_packet { unsigned char before; struct packet packet; unsigned char after; };
    struct guarded_packet target = {0xa5, {0, {0, 0, 0, 0, 0}, 0}, 0x5a};
    unsigned index;
    packet_copy(&target.packet, &source);
    packet_copy(&target.packet, &target.packet);
    ASSERT(target.before == 0xa5 && target.after == 0x5a);
    ASSERT(target.packet.tag == 6 && target.packet.end == 72);
    for (index = 0; index < sizeof source.payload; ++index)
        ASSERT(target.packet.payload[index] == 17 + 11 * index);
}

int main(void)
{
    test_parser_stores_error();
    test_parser_coerces_real();
    test_parser_preserves_error();
    test_word_alias();
    test_character_alias();
    test_odd_struct_copy();
    testTortureExecute();
    testWrites();
    testMemberTypes();
    if (failures) {
        puts("FAIL C language regression");
        return 1;
    }
    puts("PASS C language regression");
    return 0;
}
