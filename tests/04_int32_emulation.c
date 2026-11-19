int main(void)
{
    unsigned long a = 0x1234ffffUL;
    unsigned long b = 0x00020003UL;

    if (a + b != 0x12370002UL)
        return 1;
    if (a - b != 0x1232fffcUL)
        return 2;
    if (b * 7UL != 0x000e0015UL)
        return 3;
    if (a / 13UL != 0x016689d8UL)
        return 4;
    if ((a << 4) != 0x234ffff0UL || (a >> 4) != 0x01234fffUL)
        return 5;
    return 7;
}
