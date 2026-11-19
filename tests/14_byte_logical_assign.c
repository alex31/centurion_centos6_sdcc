static unsigned char bytes[4];

static int or_assign(unsigned char *p, unsigned int mask)
{
    return *p |= mask;
}

static int xor_assign(unsigned char *p, unsigned int mask)
{
    return *p ^= mask;
}

static int and_assign(unsigned char *p, unsigned int mask)
{
    return *p &= mask;
}

int main(void)
{
    bytes[0] = 0x12;
    bytes[1] = 0x23;
    bytes[2] = 0xa5;
    bytes[3] = 0x34;

    if (or_assign(&bytes[1], 0x80) != 0xa3 || bytes[1] != 0xa3)
        return 1;
    if (xor_assign(&bytes[1], 0xf0) != 0x53 || bytes[1] != 0x53)
        return 2;
    if (or_assign(&bytes[2], 0x1234) != 0xb5 || bytes[2] != 0xb5)
        return 3;
    if (xor_assign(&bytes[2], 0x1ff) != 0x4a || bytes[2] != 0x4a)
        return 4;
    if (and_assign(&bytes[2], 0xf0) != 0x40 || bytes[2] != 0x40)
        return 5;
    if (bytes[0] != 0x12 || bytes[3] != 0x34)
        return 6;
    return 7;
}
