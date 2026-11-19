struct values {
    unsigned char left, a, b, right;
    unsigned int first, second;
};

static struct values value;

static unsigned int set_words(struct values *p)
{
    return p->first = p->second = 0x4567U;
}

static unsigned int set_bytes(struct values *p)
{
    return p->a = p->b = 0xab;
}

int main(void)
{
    value.left = 0x12;
    value.right = 0x34;
    if (set_words(&value) != 0x4567U || value.first != 0x4567U ||
        value.second != 0x4567U) return 1;
    if (set_bytes(&value) != 0xabU || value.a != 0xab || value.b != 0xab ||
        value.left != 0x12 || value.right != 0x34) return 2;
    return 7;
}
