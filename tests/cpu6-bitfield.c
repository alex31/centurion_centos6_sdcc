#include <testfwk.h>

void testBitfieldSizeof(void);
void testBitfieldsMultibitLiteral(void);
void testBitfieldsMultibit(void);
void testBitfields(void);
void testSignedBitfields(void);
void testCS(void);

struct nibbles { unsigned low : 4, high : 4; };
static volatile struct nibbles device = {7, 10};

static unsigned exchange_high(volatile struct nibbles *bits, unsigned value)
{
    unsigned previous = bits->high;
    bits->high = value;
    return previous;
}

static void test_nibbles(void)
{
    const volatile unsigned char *raw = (const volatile unsigned char *)&device;
    ASSERT(*raw == 0xa7);
    ASSERT(exchange_high(&device, 0x23) == 10);
    ASSERT(*raw == 0x37 && device.low == 7 && device.high == 3);
    device.low ^= 5;
    ASSERT(*raw == 0x32);
    ASSERT(device.high++ == 3 && *raw == 0x42);
    ASSERT(--device.high == 3 && *raw == 0x32);
    ASSERT((device.high = 17) == 1 && *raw == 0x12);
    device.high = 15;
    ASSERT(++device.high == 0 && *raw == 0x02);
    ASSERT(--device.high == 15 && *raw == 0xf2);
    ASSERT(device.low - 8 < 0);
}

struct wide_fields {
    unsigned value : 13;
    unsigned neighbor : 3;
    signed int delta : 9;
    unsigned another : 7;
    unsigned word : 16;
    signed int signed_word : 16;
};
static volatile struct wide_fields wide = {0x1234, 5, -256, 63, 0xbeef, -32767 - 1};

static void test_wide_fields(void)
{
    const volatile unsigned char *bytes = (const volatile unsigned char *)&wide;
    ASSERT(bytes[0] == 0x34 && bytes[1] == 0xb2);
    ASSERT(bytes[2] == 0 && bytes[3] == 0x7f);
    ASSERT(wide.value == 0x1234 && wide.neighbor == 5);
    ASSERT(wide.delta == -256 && wide.another == 63);
    ASSERT(wide.word == 0xbeef && wide.signed_word == -32767 - 1);
    wide.value = 0x157a;
    wide.delta = -19;
    wide.word = 0x9fd0;
    wide.signed_word = 32642;
    ASSERT(bytes[0] == 0x7a && bytes[1] == 0xb5);
    ASSERT(bytes[2] == 0xed && bytes[3] == 0x7f);
    ASSERT(wide.value == 0x157a && wide.neighbor == 5);
    ASSERT(wide.delta == -19 && wide.another == 63);
    ASSERT(wide.word == 0x9fd0 && wide.signed_word == 32642);
}

struct flags {
    int plain : 4;
    signed int negative : 3;
    _Bool flag : 1;
    unsigned : 0;
    unsigned unused : 1;
    unsigned : 3;
    unsigned tail : 4;
};
static volatile struct flags state = {.flag = 2, .tail = 9, .plain = -6, .negative = -4};
static volatile unsigned long flag_input = 0x10000UL;

static void write_flag(volatile struct flags *flags, unsigned long value)
{
    flags->flag = value;
}

static void test_boolean_and_padding(void)
{
    const volatile unsigned char *bytes = (const volatile unsigned char *)&state;
    ASSERT(state.flag == 1 && state.tail == 9);
    ASSERT(state.plain == -6 && state.negative == -4);
    ASSERT(bytes[0] == 0xca && bytes[1] == 0x90);
    write_flag(&state, 0);
    ASSERT(state.flag == 0 && bytes[0] == 0x4a);
    write_flag(&state, flag_input);
    ASSERT(state.flag == 1 && bytes[0] == 0xca);
    state.flag = 0;
    state.flag = 2;
    ASSERT(state.flag == 1 && bytes[0] == 0xca);
    ASSERT(state.flag++ == 1 && state.flag == 1);
    ASSERT(--state.flag == 0 && bytes[0] == 0x4a);
    ASSERT(++state.flag == 1 && bytes[0] == 0xca);
    state.tail = 3;
    ASSERT(bytes[0] == 0xca && bytes[1] == 0x30);
}

union bit_union { unsigned twelve : 12; unsigned char bytes[2]; };
static volatile union bit_union value = {.twelve = 0xabc};

static void test_union(void)
{
    const volatile unsigned char *bytes = (const volatile unsigned char *)&value;
    ASSERT(value.twelve == 0xabc);
    ASSERT(bytes[0] == 0xbc && bytes[1] == 0x0a);
    value.twelve = 0x596;
    ASSERT(bytes[0] == 0x96 && bytes[1] == 0x05);
}

struct record {
    unsigned char pad[160];
    struct nibbles bits;
    unsigned sequence : 12;
    unsigned tag : 4;
};

static unsigned update_record(struct record *records, unsigned index, unsigned input)
{
    records[index].bits.high = input;
    records[index].sequence = input * 17;
    return records[index].bits.low + records[index].sequence;
}

static void test_indexed_member(void)
{
    struct record records[2];
    records[0].bits.low = 11;
    records[0].bits.high = 4;
    records[1].bits.low = 6;
    records[1].tag = 9;
    ASSERT(update_record(records, 1, 71) == 1213);
    ASSERT(records[0].bits.low == 11 && records[0].bits.high == 4);
    ASSERT(records[1].bits.low == 6 && records[1].bits.high == 7);
    ASSERT(records[1].sequence == 1207 && records[1].tag == 9);
}

void test_cpu6_bitfields(void)
{
    test_nibbles();
    test_wide_fields();
    test_boolean_and_padding();
    test_union();
    test_indexed_member();
    testBitfieldSizeof();
    testBitfieldsMultibitLiteral();
    testBitfieldsMultibit();
    testBitfields();
    testSignedBitfields();
    testCS();
}
