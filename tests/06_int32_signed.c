int main(void)
{
    long a = -123456L;
    long b = 97L;
    long c = -97L;

    if (a / b != -1272L || a % b != -72L)
        return 1;
    if (a / c != 1272L || a % c != -72L)
        return 2;
    if (b / c != -1L || b % c != 0L)
        return 3;
    if ((a >> 4) != -7716L)
        return 4;
    if (!(a < b) || !(b > c) || !(a <= a) || !(b >= b))
        return 5;
    return 7;
}
