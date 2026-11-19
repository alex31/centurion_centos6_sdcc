#ifndef CPU6_OS6_MMU_H
#define CPU6_OS6_MMU_H
#include <stdint.h>
#include <cpu6abi.h>

#define OS6_MMU_ABI 1U
#define OS6_MMU_MAGIC 0x4d36U
#define OS6_MMU_OK 0U
#define OS6_MMU_ARGUMENT 1U
#define OS6_MMU_BUSY 2U
#define OS6_MMU_ABSENT 3U
#define OS6_MMU_VERSION 4U
#define OS6_MMU_MODULE 5U

/* Internal transport. All of it stays in the caller's resident partition.
   No pointer into the lower MMU window may be returned through this packet. */
struct os6_mmu_packet {
    uint16_t magic, abi, module, operation, bytes, result;
    void *data;
};

/* The .LOAD BASIC <module> launch contract must have succeeded first. This
   uses the sole system BASIC-monitor role, and cannot coexist with @BASIC.
   Module IDs identify compatible SDK modules, not arbitrary legacy monitors.
   A successful transport returns the module's 16-bit result through result.
   Wide arguments/results belong in data, never in scalar return registers. */
uint16_t os6_mmu_call(uint16_t module, uint16_t operation, void *data,
                     uint16_t bytes, uint16_t *result) CPU6_STACKCALL;

/* Module authors implement this one function. The builder generates the
   validating entry point and links the module at its required origin. */
uint16_t mmu_service(uint16_t operation, void *data,
                     uint16_t bytes) CPU6_STACKCALL;
#endif
