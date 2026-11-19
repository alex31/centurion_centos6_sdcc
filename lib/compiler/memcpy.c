/* SDCC emits this helper for structure assignment. Stack ABI, 16-bit size. */
void *__memcpy(void *destination, const void *source, unsigned count)
{
    unsigned char *out = destination;
    const unsigned char *in = source;
    while (count--) *out++ = *in++;
    return destination;
}
