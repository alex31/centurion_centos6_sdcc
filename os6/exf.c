#include <os6exf.h>

/* Layout and control bits match the type E block I/O used by S.XCOPUT. */
struct exf_rcb {
    uint8_t status, unit;
    uint16_t control, buffer_size;
    OS6_EXF_BLOCK *buffer;
    uint16_t reserved0, flags;
    uint8_t class_code, file_type;
    uint8_t reserved1[12];
    uint16_t *position;
    uint16_t reserved2;
};

static struct exf_rcb rcb;
static uint16_t position[2];
int os6_exf_call(struct exf_rcb *control);

int os6_exf_begin(uint8_t logical_unit)
{
    unsigned int i;
    uint8_t *bytes = (uint8_t *)&rcb;
    if (sizeof(rcb) != 30) return -1;
    for (i = 0; i < sizeof(rcb); i++) bytes[i] = 0;
    position[0] = 0;
    position[1] = 0;
    rcb.unit = logical_unit;
    rcb.control = 0x2202;
    rcb.buffer_size = OS6_EXF_SECTOR_SIZE;
    rcb.flags = 5;
    rcb.class_code = 2;
    rcb.file_type = 0xff;
    rcb.position = position;
    return 0;
}

int os6_exf_write(OS6_EXF_BLOCK *block)
{
    int status;
    if (!block || block->length != OS6_EXF_SECTOR_SIZE) return -1;
    rcb.buffer = block;
    status = os6_exf_call(&rcb);
    if (!status) position[1]++;
    return status;
}
