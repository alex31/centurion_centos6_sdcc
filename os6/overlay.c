#include <os6overlay.h>

/* Four nonoverlapping slots, no heap, one active call for the whole process.
   Modules/files are trusted build artifacts, not untrusted CBIN input. */
static struct os6_ov_slot *slots[4];
static unsigned char busy;
extern uint16_t os6_ov_raw(const unsigned char *, void *) CPU6_STACKCALL;

static unsigned registered(const struct os6_ov_slot *slot)
{
    unsigned i;
    if (!slot) return 0;
    for (i = 0; i != 4; ++i) if (slots[i] == slot) return 1;
    return 0;
}

uint16_t os6_ov_init(struct os6_ov_slot *slot, void *area, uint16_t size) CPU6_STACKCALL
{
    unsigned i, free_index = 4;
    uint16_t base = (uint16_t)area, last;
    if (busy) return OS6_OV_BUSY;
    /* CENTOS relocates user partitions (e.g. into the upper 32 KiB). The
       compiler's relative 0x8000 limit is not a runtime pointer limit. */
    if (!slot || registered(slot) || base < 0x200 || !size ||
        size > 0x7e00U || size - 1U > 0xffffU - base) return OS6_OV_ARGUMENT;
    last = base + size - 1U;
    for (i = 0; i != 4; ++i) {
        if (!slots[i]) { if (free_index == 4) free_index = i; }
        else if (base <= (uint16_t)slots[i]->area + slots[i]->capacity - 1U &&
                 (uint16_t)slots[i]->area <= last) return OS6_OV_ARGUMENT;
    }
    if (free_index == 4) return OS6_OV_SLOTS;
    slot->area = area; slot->capacity = size;
    slot->generation_lo = slot->generation_hi = 0;
    slot->entry = 0;
    slots[free_index] = slot;
    return OS6_OV_OK;
}

uint16_t os6_ov_load(struct os6_ov_slot *slot, const struct os6_ov_desc *desc,
                     struct os6_ov_handle *handle) CPU6_STACKCALL
{
    unsigned char name[11];
    unsigned i;
    uint16_t entry;
    if (busy) return OS6_OV_BUSY;
    if (!registered(slot) || !desc || !handle) return OS6_OV_ARGUMENT;
    /* Any attempted replacement invalidates previous handles, even on failure.
       Never wrap generations and accidentally revive an old handle. */
    slot->entry = 0;
    if (slot->generation_lo == 0xffffU && slot->generation_hi == 0xffffU)
        return OS6_OV_EXHAUSTED;
    if (++slot->generation_lo == 0) ++slot->generation_hi;
    handle->slot = 0;
    if (desc->abi != OS6_OV_ABI || !desc->name || !desc->bytes ||
        desc->entry >= desc->bytes) return OS6_OV_ARGUMENT;
    if (desc->bytes > slot->capacity) return OS6_OV_SIZE;
    for (i = 0; i != 11; ++i) {
        unsigned char c = desc->name[i];
        if (!c) break;
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return OS6_OV_ARGUMENT;
        name[i] = c | 0x80;
    }
    if (!i || i == 11) return OS6_OV_ARGUMENT;
    name[i] = 0;
    busy = 1;
    entry = os6_ov_raw(name, slot->area);
    busy = 0;
    if (entry != (uint16_t)slot->area + desc->entry) return OS6_OV_LOAD;
    slot->entry = (os6_ov_entry)entry;
    handle->slot = slot;
    handle->generation_lo = slot->generation_lo;
    handle->generation_hi = slot->generation_hi;
    return OS6_OV_OK;
}

uint16_t os6_ov_call(const struct os6_ov_handle *handle, void *request,
                     uint16_t *result) CPU6_STACKCALL
{
    struct os6_ov_slot *slot;
    if (busy) return OS6_OV_BUSY;
    if (!handle || !result || !registered(handle->slot)) return OS6_OV_ARGUMENT;
    slot = handle->slot;
    if (!slot->entry || handle->generation_lo != slot->generation_lo ||
        handle->generation_hi != slot->generation_hi) return OS6_OV_STALE;
    busy = 1;
    *result = slot->entry(request);
    busy = 0;
    return OS6_OV_OK;
}
