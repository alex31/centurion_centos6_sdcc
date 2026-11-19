static unsigned char bytes[4] = {0x12, 0xfe, 0x34, 0x56};
static unsigned int words[3] = {0x1234, 0xfffe, 0x5678};

static int byte_increment(unsigned char *p, int i)
{
    p[i]++;
    return p[i]++;
}

static unsigned int word_increment(unsigned int *p, int i)
{
    p[i]++;
    return p[i]++;
}

int main(void)
{
    int old_byte = byte_increment(bytes, 1);
    unsigned int old_word = word_increment(words, 1);
    if (old_byte != 255 || bytes[1] != 0 || bytes[0] != 0x12 || bytes[2] != 0x34)
        return 1;
    if (old_word != 0xffffU || words[1] != 0 || words[0] != 0x1234 || words[2] != 0x5678)
        return 2;
    return 7;
}
