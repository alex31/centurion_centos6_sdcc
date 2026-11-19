#pragma pack(1)
struct fcb {
    unsigned char opcode;
    unsigned char status;
    unsigned char unit;
    unsigned int buffer_addr;
    unsigned int byte_count;
};
#pragma pack()

int main(void)
{
    struct fcb f;
    unsigned char *bytes = (unsigned char *)&f;

    if (sizeof(struct fcb) != 7)
        return 1;
    f.opcode = 0x12;
    f.buffer_addr = 0x3456;
    f.byte_count = 0x789a;
    if (bytes[0] != 0x12 || bytes[3] != 0x34 || bytes[4] != 0x56)
        return 2;
    if (bytes[5] != 0x78 || bytes[6] != 0x9a)
        return 3;
    if (f.buffer_addr != 0x3456 || f.byte_count != 0x789a)
        return 4;
    return 7;
}
