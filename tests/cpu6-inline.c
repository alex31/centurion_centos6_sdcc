#include <stdio.h>

/* Every case has the same result with heuristic inlining disabled. */
static unsigned value = 7;
enum { token = 13 };
static volatile unsigned observed;
static unsigned very_long_private_name_left = 17;
static unsigned very_long_private_name_right = 29;
struct record { unsigned value; };
struct entry { unsigned char tag; unsigned value; };
static struct entry entries[2] = {{1, 19}, {2, 43}};
static unsigned *entry_value = &entries[1].value;
static unsigned sequence[4] = {5, 11, 17, 23};
static unsigned *sequence_end = sequence + 4;
static unsigned char *second_byte = (unsigned char *)sequence + 1;

static inline unsigned get_value(void) { return value; }
static inline void set_value(unsigned n) { value = n; }
static inline unsigned get_token(void) { return token; }
static inline unsigned init_value(unsigned n) {
    unsigned local = value;
    return local + n;
}
unsigned shadow(unsigned value, unsigned token) {
    return get_value() + get_token() + init_value(value) + token;
}
static inline unsigned nested(unsigned n) {
    unsigned local = n;
    { unsigned n = 3; local += n; }
    return local;
}
static inline unsigned member(struct record *p) { return p->value; }
static inline unsigned target(void) { return 7; }
static unsigned other(void) { return 9; }
unsigned indirect(void) {
    unsigned (*target)(void) = other;
    { return target(); }
}
static inline unsigned tick(void) {
    static unsigned counter;
    return ++counter;
}
unsigned third(void) { return tick(); }
unsigned auto_leaf(unsigned x, unsigned y) {
    unsigned result = x + y;
    if (x & 1) result ^= 0x55U;
    else result += 3;
    return result;
}
unsigned invoke_leaf(unsigned n) { return auto_leaf(n, 5); }
static unsigned auto_global(unsigned n) { return value + n; }
unsigned auto_shadow(unsigned value) { return auto_global(value); }
static unsigned auto_tick(void) { static unsigned counter = 20; return ++counter; }
static unsigned static_objects(void) {
    static unsigned table[3] = {2, 5, 9};
    static unsigned *pointer = table + 1;
    static struct record item = {37};
    return *pointer + table[2] + item.value;
}
static inline unsigned pair(unsigned x, unsigned y) { return 10*x + y; }
unsigned swap(unsigned x, unsigned y) { return pair(y, x); }
unsigned recursive(unsigned n) { return n ? n + recursive(n - 1) : 0; }
static unsigned changing(void) { return observed++; }
static unsigned loop(unsigned n) {
    unsigned sum = 0, i;
    for (i = 0; i < n; ++i) sum += i;
    return sum;
}
static inline unsigned forced_large(unsigned n) {
    n = (n ^ 1) + 3; n = (n ^ 2) + 3; n = (n ^ 3) + 3;
    n = (n ^ 4) + 3; n = (n ^ 5) + 3; n = (n ^ 6) + 3;
    n = (n ^ 7) + 3; n = (n ^ 8) + 3; n = (n ^ 9) + 3;
    n = (n ^ 10) + 3; n = (n ^ 11) + 3; n = (n ^ 12) + 3;
    n = (n ^ 13) + 3; n = (n ^ 14) + 3; n = (n ^ 15) + 3;
    n = (n ^ 16) + 3; n = (n ^ 17) + 3; n = (n ^ 18) + 3;
    return n;
}
unsigned invoke_forced(unsigned n) { return forced_large(n); }

int main(void) {
    unsigned stage = 1, a, b, c, i, expected;
    struct record item;
    if (shadow(99, 2) != 128 || get_token() != 13) goto fail;
    stage = 2;
    set_value(11);
    if (value != 11 || shadow(99, 2) != 136 || nested(8) != 11) goto fail;
    stage = 3;
    item.value = 123;
    if (member(&item) != 123 || indirect() != 9) goto fail;
    stage = 4;
    a = tick(); b = tick(); c = third();
    if (a != 1 || b != 2 || c != 3) goto fail;
    stage = 5;
    if (very_long_private_name_left != 17 || very_long_private_name_right != 29) goto fail;
    if (auto_shadow(99) != 110 || auto_tick() != 21 || auto_tick() != 22) goto fail;
    if (static_objects() != 51) goto fail;
    if (*entry_value != 43 || sequence_end[-1] != 23 || second_byte != (unsigned char *)sequence + 1) goto fail;
    if (swap(4, 8) != 84 || recursive(10) != 55 || loop(10) != 45) goto fail;
    stage = 6;
    observed = 3;
    a = auto_leaf(changing(), 7);
    if (observed != 4 || a != (10 ^ 0x55U)) goto fail;
    a = 2; b = 5;
    c = pair(a++, b++);
    if (a != 3 || b != 6 || c != 25) goto fail;
    stage = 7;
    for (i = 0; i < 16; ++i) {
        expected = i;
        for (a = 1; a <= 18; ++a) expected = (expected ^ a) + 3;
        if (invoke_forced(i) != expected || invoke_leaf(i) != ((i & 1) ? ((i+5)^0x55U) : i+8)) goto fail;
    }
    puts("PASS inline scopes, identity, side effects and multiline bodies");
    return 0;
fail:
    puts("FAIL inline"); putchar('0'+stage); return 1;
}
