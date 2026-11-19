#include <stdint.h>
#include <stdio.h>

#ifndef TAIL_DEEP
#define TAIL_DEEP 16U
#endif
#ifndef TAIL_WALK
#define TAIL_WALK 32U
#endif

uint16_t read_stack(void);
void large_state(uint16_t n, uint16_t a, uint16_t b);
int many_state_test(void);
volatile uint16_t minimum_stack = 65535U;
volatile uint16_t seen_a, seen_b, seen_tag, ticks, after_calls;
volatile uint16_t escaped_seen, parameter_seen, variadic_calls, large_seen;
volatile uint32_t seen_sum;
static unsigned char data[TAIL_WALK];

/* The definition must still be recognized through an earlier prototype. */
void rotate_state(uint16_t n, uint16_t a, uint16_t b);

void observe_stack(void)
{
    uint16_t current = read_stack();
    if (current < minimum_stack) minimum_stack = current;
}

void rotate_state(uint16_t n, uint16_t a, uint16_t b)
{
    observe_stack();
    if (!n) { seen_a = a; seen_b = b; return; }
    rotate_state(n - 1, b, a);
}

void mixed_state(uint16_t n, unsigned char tag, uint32_t total)
{
    observe_stack();
    if (!n) { seen_tag = tag; seen_sum = total; return; }
    mixed_state(n - 1, tag + 37, total + n + tag);
}

void pointer_state(uint16_t n, const unsigned char *p, uint32_t total)
{
    observe_stack();
    if (!n) { seen_sum = total; return; }
    pointer_state(n - 1, p + 1, total + *p);
    return;
}

static uint16_t tick(void) { ticks++; return ticks; }
void evaluated_state(uint16_t n, uint16_t a, uint16_t b)
{
    if (!n) { seen_a = a; seen_b = b; return; }
    evaluated_state(n - 1, tick(), tick());
}

/* These frames must remain alive: the next call reads their local state. */
void escaping_local(uint16_t n, const uint16_t *previous)
{
    uint16_t local = n;
    if (!n) { escaped_seen = *previous; return; }
    escaping_local(n - 1, &local);
}
void escaping_parameter(uint16_t n, const uint16_t *previous)
{
    if (!n) { parameter_seen = *previous; return; }
    escaping_parameter(n - 1, &n);
}
void nonterminal(uint16_t n)
{
    if (n) nonterminal(n - 1);
    after_calls += n + 1;
}
void variadic_state(uint16_t n, ...)
{
    variadic_calls++;
    if (n) variadic_state(n - 1);
}

#define CHECK(condition) do { if (!(condition)) { puts("FAIL tail recursion"); return 1; } } while (0)
int main(void)
{
    uint16_t i, tag, baseline;
    uint32_t expected;
    baseline = read_stack();
    minimum_stack = 65535U;
    rotate_state(TAIL_DEEP, 0x1234, 0xabcd);
    CHECK(seen_a == ((TAIL_DEEP & 1) ? 0xabcd : 0x1234));
    CHECK(seen_b == ((TAIL_DEEP & 1) ? 0x1234 : 0xabcd));
    CHECK(read_stack() == baseline);
#ifdef EXPECT_TAIL
    CHECK(baseline - minimum_stack < 128U);
#endif
    expected = 0x12340000UL; tag = 251;
    for (i = 513; i; i--) { expected += i + tag; tag = (unsigned char)(tag + 37); }
    minimum_stack = 65535U;
#ifdef EXPECT_TAIL
    mixed_state(513, 251, 0x12340000UL);
    CHECK(seen_sum == expected && seen_tag == tag);
    CHECK(baseline - minimum_stack < 256U);
#else
    mixed_state(2, 251, 0x12340000UL);
    CHECK(seen_sum == 0x1234011eUL && seen_tag == 69);
#endif
    expected = 0;
    for (i = 0; i < TAIL_WALK; i++) { data[i] = (unsigned char)(i * 37U + 5U); expected += data[i]; }
    minimum_stack = 65535U;
    pointer_state(TAIL_WALK, data, 0);
    CHECK(seen_sum == expected && read_stack() == baseline);
#ifdef EXPECT_TAIL
    CHECK(baseline - minimum_stack < 256U);
#endif
    evaluated_state(16, 0, 0);
    CHECK(ticks == 32 && seen_a + seen_b == 63 && seen_a != seen_b);
    escaping_local(5, 0);
    escaping_parameter(5, 0);
    CHECK(escaped_seen == 1 && parameter_seen == 1);
    nonterminal(4);
    CHECK(after_calls == 15);
    variadic_state(3);
    CHECK(variadic_calls == 4);
    minimum_stack = 65535U;
#ifdef EXPECT_TAIL
    large_state(51, 13, 47);
    CHECK(large_seen == 154 && baseline - minimum_stack < 512U);
#else
    large_state(2, 13, 47);
    CHECK(large_seen == 86);
#endif
    CHECK(many_state_test());
    CHECK(read_stack() == baseline);
    puts("PASS tail recursion state, argument evaluation, frame lifetime and stack depth");
    return 0;
}
