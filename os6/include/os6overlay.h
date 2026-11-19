#ifndef CPU6_OS6_OVERLAY_H
#define CPU6_OS6_OVERLAY_H
#include <stdint.h>
#include <cpu6abi.h>

#define OS6_OV_ABI 1
#define OS6_OV_OK 0
#define OS6_OV_ARGUMENT 1
#define OS6_OV_BUSY 2
#define OS6_OV_SIZE 3
#define OS6_OV_LOAD 4
#define OS6_OV_STALE 5
#define OS6_OV_SLOTS 6
#define OS6_OV_EXHAUSTED 7

/* Only this entry ABI crosses separately linked programs. Wide values belong
   in the resident request block, never in a scalar argument/return register. */
typedef uint16_t (*os6_ov_entry)(void *) CPU6_STACKCALL;
struct os6_ov_desc {
    const char *name;
    uint16_t abi, bytes, entry;
};
/* Caller owns these resident objects; fields are private to the runtime.
   An initialized slot cannot be reinitialized or moved until program exit. */
struct os6_ov_slot {
    uint8_t *area;
    uint16_t capacity, generation_lo, generation_hi;
    os6_ov_entry entry;
};
struct os6_ov_handle {
    struct os6_ov_slot *slot;
    uint16_t generation_lo, generation_hi;
};
uint16_t os6_ov_init(struct os6_ov_slot *, void *, uint16_t) CPU6_STACKCALL;
uint16_t os6_ov_load(struct os6_ov_slot *, const struct os6_ov_desc *,
                     struct os6_ov_handle *) CPU6_STACKCALL;
uint16_t os6_ov_call(const struct os6_ov_handle *, void *, uint16_t *) CPU6_STACKCALL;
#endif
