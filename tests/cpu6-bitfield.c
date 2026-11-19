/* CPU6 must diagnose bit-field accesses before emitting a whole-byte store. */
struct bits { unsigned low : 4, high : 4; };

#if defined(READ_FIELD)
unsigned read_field(const struct bits *bits)
{
    return bits->high;
}
#elif defined(WRITE_STATIC_FIELD)
static volatile struct bits bits;

void write_static_field(void)
{
    bits.high = 3;
}
#else
void write_field(struct bits *bits, unsigned value)
{
    bits->high = value;
}
#endif
