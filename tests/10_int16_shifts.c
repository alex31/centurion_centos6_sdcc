int main(void)
{
    unsigned int x = 0xf123U;
    int neg = -256;
    int count = 8;

    if ((x >> 8) != 0xf1U || (x >> 9) != 0x78U || (x >> 15) != 1U)
        return 1;
    if ((x >> count) != 0xf1U || (x << count) != 0x2300U)
        return 2;
    count = 3;
    if ((neg >> count) != -32 || (neg >> 3) != -32)
        return 3;
    if ((x & 0U) != 0U || (x >> 3) != 0x1e24U)
        return 4;
    return 7;
}
