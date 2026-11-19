static unsigned char bytes[4];
static unsigned int words[2];
static int calls;

static unsigned int divisor(void)
{
    calls++;
    return 256U;
}

static unsigned char byte_div(unsigned char *p, unsigned int n)
{
    return *p /= n;
}

static unsigned int word_div(unsigned int *p, long n)
{
    return *p /= n;
}

static unsigned char byte_shift(unsigned char *p, unsigned int n)
{
    return *p >>= n;
}

int main(void)
{
    unsigned char c = 200;
    unsigned int w = 32768U;
    int i = 1;
    unsigned int value;
    volatile unsigned char v = 200;
    signed char negative = -100;
    if (byte_div(&c, 256U) != 0 || c != 0)
        return 1;
    if (word_div(&w, -2L) != 49152U || w != 49152U)
        return 2;
    bytes[0] = 0xa5;
    bytes[1] = 200;
    bytes[2] = 0x5a;
    value = (bytes[i++] %= divisor());
    if (value != 200 || bytes[1] != 200 || i != 2 || calls != 1)
        return 3;
    c = 255;
    if (byte_shift(&c, 8) != 0 || c != 0)
        return 4;
    c = 1;
    c <<= 8;
    v /= 256U;
    if (c != 0 || v != 0 || bytes[0] != 0xa5 || bytes[2] != 0x5a)
        return 5;
    words[0] = 32769U;
    i = 0;
    value = (words[i++] %= -3L);
    if (value != 0 || words[0] != 0 || i != 1)
        return 6;
    negative >>= 8;
    if (negative != -1)
        return 8;
    return 7;
}
