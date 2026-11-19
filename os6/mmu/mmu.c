#include "os6mmu.h"

extern uint16_t mmu_present(void) CPU6_STACKCALL;
extern uint16_t mmu_raw(struct os6_mmu_packet *) CPU6_STACKCALL;
static uint8_t busy;

static uint16_t resident_range(const void *p, uint16_t size)
{
    uint16_t address = (uint16_t)p;
    return size && address >= 0x8000U && size - 1U <= 0xffffU - address;
}

uint16_t os6_mmu_call(uint16_t module, uint16_t operation, void *data,
                     uint16_t bytes, uint16_t *result) CPU6_STACKCALL
{
    struct os6_mmu_packet packet;
    uint16_t status;
    if (busy) return OS6_MMU_BUSY;
    if (!module || !resident_range(result, 2U) ||
        (bytes && !resident_range(data, bytes))) return OS6_MMU_ARGUMENT;
    /* Read the OS-owned monitor pointer through permitted SVC 5C block 2.
       Never enter SVC 6A when the monitor is absent. */
    if (mmu_present() != 0x1000U) return OS6_MMU_ABSENT;
    packet.magic = OS6_MMU_MAGIC;
    packet.abi = OS6_MMU_ABI;
    packet.module = module;
    packet.operation = operation;
    packet.bytes = bytes;
    packet.data = data;
    packet.result = 0;
    busy = 1;
    status = mmu_raw(&packet);
    busy = 0;
    if (status == OS6_MMU_OK) *result = packet.result;
    return status;
}
