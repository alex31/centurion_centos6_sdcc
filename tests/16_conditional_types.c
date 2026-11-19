static long choose_long(int test, int a, long b)
{
    return test ? a : b;
}

static unsigned long choose_unsigned(int test, long a, unsigned long b)
{
    return test ? a : b;
}

static int choose_byte(int test, signed char a, unsigned char b)
{
    return test ? a : b;
}

static int choose_pointer(int test, int *p)
{
    return *(test ? 0 : p);
}

int main(void)
{
    int n = 0;
    int result;
    if (choose_long(0, 1, 0x12345678L) != 0x12345678L)
        return 1;
    if (choose_long(1, -1, 0x12345678L) != -1L)
        return 2;
    if (choose_unsigned(1, -1L, 0UL) != 0xffffffffUL)
        return 3;
    if (choose_byte(0, -1, 255) != 255)
        return 4;
    if (sizeof(n ? (unsigned char)1 : (unsigned char)2) != sizeof(int))
        return 5;
    result = n == 0 ? n = 3, n + 1 : 99;
    if (result != 4 || n != 3 || choose_pointer(0, &n) != 3)
        return 6;
    return 7;
}
