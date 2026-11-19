int main(void)
{
    long lo = -2147483647L - 1L;
    long hi = 2147483647L;
    unsigned long upper = 0x80000000UL;
    unsigned long lower = 0x7fffffffUL;
    int ilo = -32767;
    int ihi = 32767;
    ilo--;

    if (!(lo < hi) || !(hi > lo) || lo >= hi || hi <= lo)
        return 1;
    if (!(upper > lower) || !(lower < upper) || upper <= lower || lower >= upper)
        return 2;
    if (!(upper >= upper) || !(lower <= lower) || upper < upper || lower > lower)
        return 3;
    if (!(ilo < ihi) || !(ihi > ilo) || ilo >= ihi || ihi <= ilo)
        return 4;
    return 7;
}
