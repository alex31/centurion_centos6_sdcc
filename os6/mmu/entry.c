#include "os6mmu.h"
#ifndef OS6_MMU_MODULE_ID
#error "The module builder must supply OS6_MMU_MODULE_ID"
#endif

uint16_t mmu_entry(struct os6_mmu_packet *packet) CPU6_STACKCALL
{
    uint16_t address = (uint16_t)packet;
    if (address < 0x8000U || sizeof(*packet) - 1U > 0xffffU - address)
        return OS6_MMU_ARGUMENT;
    if (packet->magic != OS6_MMU_MAGIC || packet->abi != OS6_MMU_ABI)
        return OS6_MMU_VERSION;
    if (packet->module != OS6_MMU_MODULE_ID) return OS6_MMU_MODULE;
    address = (uint16_t)packet->data;
    if (packet->bytes && (address < 0x8000U ||
        packet->bytes - 1U > 0xffffU - address)) return OS6_MMU_ARGUMENT;
    packet->result = mmu_service(packet->operation, packet->data, packet->bytes);
    return OS6_MMU_OK;
}
