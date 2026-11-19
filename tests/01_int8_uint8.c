int main(void)
{
    signed char a = -10;
    unsigned char b = 250;
    int promoted = a;

    if (promoted != -10 || a >= 5 || b <= 5)
        return 1;
    if ((unsigned char)(b + 10) != 4 || (unsigned char)(b - 5) != 245)
        return 2;
    if ((b & 15) != 10 || (b | 3) != 251 || (b ^ 15) != 245)
        return 3;
    if ((unsigned char)~b != 5 || (b >> 2) != 62 || (b << 1) != 500)
        return 4;
    return 7;
}
