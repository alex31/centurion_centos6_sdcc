# C overlays using CENTOS CPL LOAD

[User guide](../README.md) · [MMU paging](mmu.md)

All shell commands on this page run from the distribution root.

`run/cpu6-overlay` builds separately linked C modules. `<os6overlay.h>` provides
the resident `os6_ov_init`, `os6_ov_load` and `os6_ov_call` interface. The runtime
uses the existing application CPL LOAD service, SVC 5/9 option 2, whose returned
B register is the loaded entry. CENTOS and its system image are unchanged.

This uses the application partial-overlay path documented in
[CPL manual](https://github.com/billsargent/centurion/blob/2bf355026e3f5d4ebdf51138088b9499ccb31b09/docs/manuals/02-CPL.md), LOAD pages 179–181. The [automatic MMU placement workflow](mmu.md) uses the separate
`.LOAD BASIC`/map-6 mechanism and a different ABI.

## Build a module

```c
#include <os6overlay.h>
uint16_t ov_entry(void *resident_request) CPU6_STACKCALL
{
    uint16_t *counter = resident_request;
    return ++*counter;
}
```

```sh
mkdir -p build/out
run/cpu6-overlay module.c --name OVMODULE --slot-size 1024 -o build/out/OVMODULE.exf
```

This also creates `OVMODULE.h`, `.json` and `.map`. The header supplies
`OVMODULE_OVERLAY` (descriptor initializer) and `OVMODULE_OVERLAY_BYTES`.
The JSON records required memory, entry offset, code/data/BSS extents, options
and the CBIN hash. `--float32` and `--native-int48` enable those features *inside*
the module. `-I`, `-D`, multiple C inputs and `--save-temps` are accepted.

The builder enforces `uint16_t entry(void *)` with the stack calling convention,
links at zero without main startup, keeps the entry with general GC, rejects
unresolved references, checks the slot size and emits word relocation records.
Every BSS byte is explicitly included as zero in the CBIN payload: each reload
resets both initialized and zero-initialized statics. Statics persist between
calls while the same module remains loaded.

## Resident caller

```c
#include "OVMODULE.h"
static unsigned char area[OVMODULE_OVERLAY_BYTES];
static struct os6_ov_slot slot;
static struct os6_ov_handle handle;
static const struct os6_ov_desc module = OVMODULE_OVERLAY;
static uint16_t counter;

int main(void)
{
    uint16_t result;
    if (os6_ov_init(&slot, area, sizeof area)) return 1;
    if (os6_ov_load(&slot, &module, &handle)) return 2;
    if (os6_ov_call(&handle, &counter, &result)) return 3;
    return result == 1 ? 0 : 4;
}
```

Compile with `run/cpu6-os6-sdcc --gc-sections -Ibuild/out resident.c -o build/out/resident.exf`.
Install the resident program and named modules in a private image using the
ordinary `.NEW`/CBIN workflow; module filenames must match their descriptors.
Install the resident executable and module into a new disk image with
`scripts/install-os6-bundle.py`, then launch the emulator on that image.

## Lifetime and ABI contract

* Stack, loader, slot metadata, descriptors, handles, requests and persistent
  state stay resident. Slot buffers must be reserved live objects of sufficient
  size, not arbitrary addresses. The caller owns the request block's bounds.
* Up to four slots are registered; overlaps and duplicate initialization fail.
  A slot cannot be moved/reinitialized during the process. Runtime addresses
  may be above 0x8000 after CENTOS relocates the partition; 16-bit wrap is rejected.
* Replacing a module invalidates all old handles for that slot, even on failed
  load. Generation wrap is rejected. Missing files and an unexpected entry
  address return `OS6_OV_LOAD` and leave the slot uncallable.
* A process can have only one active overlay call. Nested calls or loads return
  `OS6_OV_BUSY`, including calls through a resident callback. Return normally
  before requesting the next module; do not use a longjmp across this boundary.
* Only a 16-bit result and resident pointer cross the boundary. Put float32,
  int32/int48 and strings into resident fields or byte arrays. Separately linked
  copies of `__hireg` do not implement cross-module scalar wide returns.
  Resident callbacks must obey the same 16-bit/request-block rule.
* Do not retain pointers to module functions/data after replacement. Direct
  unresolved imports from resident code are unsupported; an explicit resident
  callback in the request is available when needed.

The loader trusts matching CBIN files produced by the builder. It does not
authenticate files or parse their entire extent before the OS writes them.
The JSON hash is build/install evidence, not a runtime security mechanism.
Do not substitute arbitrary files under a registered module name.

## Executed tests

The SDK test loads 119-byte counter modules at two different addresses and an
8,536-byte module containing sinf, sqrtf and arithmetic. It checks callback and
register preservation, persistent and reset state, all slot canaries, stale
handles, missing files, excess size, bad ABI and rejection of nested operations.
The module builder separately rejects wrong entry prototypes and insufficient
slots. The resident runs with GC, includes the modules' slots in its BSS, ends
at relative address 0x3f55 (16,213), returns to CENTOS, and `.STA` succeeds.

This validates the software toolchain on the microcoded system emulator, not
physical hardware or arbitrary module control flow. The 1 KiB default C stack
works for this test; applications must budget and measure their own stack.
See [the BASIC architecture](../swext/stefan-basic/CENTURION.md) for a design
that keeps execution continuations and language state resident.
