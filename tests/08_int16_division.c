int main(void)
{
    int a = -12345;
    int b = 67;
    int c = -67;
    unsigned int ua = 65000U;
    unsigned int ub = 123U;

    if (a / b != -184 || a % b != -17)
        return 1;
    if (a / c != 184 || a % c != -17)
        return 2;
    if (ua / ub != 528U || ua % ub != 56U)
        return 3;
    return 7;
}
