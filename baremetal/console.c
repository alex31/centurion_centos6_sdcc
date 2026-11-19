/* Polled output on Centurion MUX channel 0. */
int putchar(int c)
{
    volatile unsigned char *status = (volatile unsigned char *)0xf200;
    volatile unsigned char *data = (volatile unsigned char *)0xf201;
    while ((*status & 2) == 0)
        ;
    *data = (unsigned char)c;
    return c;
}

int puts(const char *s)
{
    while (*s)
        putchar(*s++);
    putchar('\n');
    return 0;
}
