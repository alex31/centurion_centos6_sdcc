/* The E-Kermit control block contains more than fifty fields. */
struct large {
    unsigned char f00, f01, f02, f03, f04, f05, f06, f07, f08, f09;
    unsigned char f10, f11, f12, f13, f14, f15, f16, f17, f18, f19;
    unsigned char f20, f21, f22, f23, f24, f25, f26, f27, f28, f29;
    unsigned char f30, f31, f32, f33, f34, f35, f36, f37, f38, f39;
    unsigned char f40, f41, f42, f43, f44, f45, f46, f47, f48, f49;
    unsigned char f50, f51, f52, f53, f54, f55, f56, f57, f58, f59;
    unsigned char f60;
    unsigned int word;
    unsigned char tail;
};

static struct large control;

int main(void)
{
    unsigned char *raw = (unsigned char *)&control;
    if (sizeof(control) != 64) return 1;
    control.f00 = 0x12;
    control.f50 = 0x34;
    control.f60 = 0x56;
    control.word = 0x789a;
    control.tail = 0xbc;
    if (raw[0] != 0x12 || raw[50] != 0x34 || raw[60] != 0x56 ||
        raw[61] != 0x78 || raw[62] != 0x9a || raw[63] != 0xbc) return 2;
    return 7;
}
