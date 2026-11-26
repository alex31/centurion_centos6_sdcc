# MMU placement and C overlays

[User guide](../README.md) · [C overlays](#c-overlays)

All shell commands on this page run from the distribution root.

## Automatic MMU placement

`run/cpu6-mmu-cc` implements the `mmu_page` C attribute. It uses the shared
SDCC frontend plus the existing assembler/linker and native CENTOS MMU entry.
The application uses ordinary variables, expressions and function calls.
Save this example as `source.c` in the distribution root:

```c
#include <stdio.h>

unsigned values[2500] attribute((mmu_page=auto));
unsigned bias attribute((mmu_page=3)) = 7;

unsigned lookup(unsigned i) attribute((mmu_page=4))
{
    return values[i] + bias;
}

int main(void)
{
    values[0] = 35;
    printf("%u\n", lookup(0));
    return 0;
}
```

The spelling `__attribute__((mmu_page=auto))` also works. Attributes follow the
declarator, before an initializer or a function body. Integer constant
expressions and macros may be used instead of a numeric literal.

### Usage

From the distribution root:

```sh
mkdir -p build/out
run/cpu6-mmu-cc source.c -o build/out/XAPP.exf
run/cpu6-mmu-cc --float32 source.c -o build/out/XAPP.exf

run/cpu6-mmu-cc -c first.c -o build/out/first.o
run/cpu6-mmu-cc -c second.c -o build/out/second.o
ar rcs build/out/components.a build/out/second.o
run/cpu6-mmu-cc build/out/first.o build/out/components.a -o build/out/XAPP.exf
```

All application translation units participating in a mapped program must use
this driver, including those without attributes, so the linker can validate
their address uses. Object files retain the necessary assembly and semantic
metadata; original C sources are not required at link time. Existing platform
libraries are selected and pruned by dependency. A float32 object requires
`--float32` at link time too. Double precision remains unsupported.

The compiler produces:

- `XAPP.exf`: the user executable;
- `XAPP.mmu.exf`: the companion module, with its generated CENTOS name in JSON;
- `XAPP.job`: the generated `.LOAD BASIC <module>` / `.RUN XAPP` launch sequence;
- `XAPP.map` and `XAPP.json`: final addresses, requested/actual pages, sizes,
  initialization cost and artifact hashes.

Install both executables using their names from the JSON, then execute the
generated job in a session with the BASIC-monitor role available. The `.job`
file is an ASCII source listing for the usual CENTOS job-file installation
workflow. Its load must succeed. Subsequent invocations can use `.RUN XAPP`;
the compiler reinitializes mapped C globals and local statics on each invocation.
The application contains no calls to the manual MMU SDK.

If all annotated definitions are removed as unused, the output is an ordinary
resident executable with a `.RUN`-only job and no MMU module. `-I`, `-D`,
`--stack-size` (256..8192 bytes) and `--save-temps` are supported.

The driver defaults to `-O1` (speed) and source-to-source LTO, with dependency
pruning at link time. `-Os` selects size; `-O0` selects stack temporaries and
`--no-lto` disables importing helper bodies. Functions with `mmu_page` always
retain their placement. LTO considers freshly supplied C translation units;
precompiled object/archive inputs are still validated and pruned. The generated
`.build.json` files record the compiler, options and input hashes.

### Installation and launch

First check `mode` in the generated JSON report. If it is `"resident"`, all
mapped definitions were discarded and there is no companion module: use the
[ordinary executable workflow](sdcc.md#compile-and-run-an-application) and the
single `.RUN` command in the generated job.

For `"mode": "mmu"` and a build named `build/out/XAPP.exf`, use the generated module
name when installing both CBIN files into a new image:

```sh
mmu_module_name=$(python3 -c 'import json; print(json.load(open("build/out/XAPP.json"))["moduleName"])')
python3 scripts/install-os6-bundle.py \
    /path/to/CENTOS_13.IMG \
    XAPP=build/out/XAPP.exf "${mmu_module_name}=build/out/XAPP.mmu.exf" \
    -o build/out/centos-xapp.img
cat build/out/XAPP.job
make -j4 emulator-cpp
run/centurion-system build/out/centos-xapp.img
```

The output image must be new. Follow the
[console boot procedure](../README.md#start-a-centos-session), then enter the
two commands printed by `cat build/out/XAPP.job`: `.LOAD BASIC` with the generated
module name, followed by `.RUN XAPP`. The `.job` is a text listing; producing it
does not by itself install an executable CENTOS job file.

The module must be loaded successfully before execution. Later runs in that
session can use `.RUN XAPP` while the matching module remains loaded. Repeat
the preload after booting a fresh session. The global monitor and ownership
restrictions below apply.

### Placement and execution

The current **CENTOS 7.13 profile** provides ten logical pages of 2 KiB:

| Attribute | Meaning |
| --- | --- |
| `mmu_page=auto` | The linker chooses a contiguous location. |
| `mmu_page=0` through `9` | The object starts within that logical module page. |

Page zero starts at logical address `0x1000`; page nine starts at `0x5800`.
These are not physical RAM page numbers. CENTOS allocates and owns the physical
pages. The existing loader's validated module budget is **20 KiB**, including
the compiler's small entry/ownership code. The normal user partition remains
at `0x8000..0xffff`; `0x0000..0x0fff` belongs to the OS.

Several functions/variables can share a page. One function or object can span
several pages. The linker reserves pinned definitions, packs automatic ones
into free spans and diagnoses capacity/placement conflicts. A pinned page does
not imply exclusive use or a fixed byte offset within it. Sparse pins may
increase the allocated module span. No object is silently moved to another
requested page.

All module pages are simultaneously visible in map 6. The compiler therefore
enters map 6 once before running C application code and returns to map 4 after
`main` finishes. It does not add a service call to every variable access or
function invocation. Normal OS service calls temporarily enter the kernel and
restore the caller's map. Long calculations still cooperate with CENTOS using
the existing `os6_checkpoint()` API; generated initialization does this itself.

Both images are linked together at their logical addresses. They share one C
runtime and its return-value helpers. Normal C arguments/results, including
32-bit integers and optional float32, work across the placement boundary.
There is no 16-bit-only public module entry ABI for application functions.
Initialized mutable objects are restored from a compact copy/zero stream;
constant objects need no duplicate initialization image.

### Enforced restrictions

- Mapped objects need static storage duration: globals and static locals work;
  automatic locals, parameters, typedefs and structure members cannot carry
  this placement attribute. Annotate the complete structure instead.
- Taking a mapped object's address, decaying a mapped array/subarray/member
  array to a pointer, or obtaining a pointer to a mapped function is rejected.
  Value access, subscripting, field access and `sizeof` remain supported.
  Generated internal addresses are distinct from user-visible address escape.
- Conflicting declarations are rejected, including across translation units.
- Inline assembly and application objects lacking the MMU semantic metadata
  are rejected in a mapped build. The explicit MMU SDK cannot be mixed with the
  automatic mapping profile. Mapped functions retain their placement even if
  declared `inline`.
- Unused functions/data are pruned before allocating pages. A discarded large
  object does not consume the module budget.

The native BASIC-monitor role is **global to the machine**. This profile does
not coexist with historical `@BASIC`, the explicit SDK's module, or another
unrelated MMU application. A module is paired with its resident image through
a build identity. An ownership guard prevents another partition from entering
while the application uses its mutable mapped state. A fresh invocation in the
same partition can recover its previous ownership after an abort. This is not
a general per-process bank allocator, and simultaneous multiuser execution has
not been validated. CENTOS and its physical-page tables are unchanged.

## C overlays

`run/cpu6-overlay` builds separately linked C modules. `<os6overlay.h>` provides
the resident `os6_ov_init`, `os6_ov_load` and `os6_ov_call` interface. The runtime
uses the existing application CPL LOAD service, SVC 5/9 option 2, whose returned
B register is the loaded entry. CENTOS and its system image are unchanged.

This uses the application partial-overlay path documented in
[CPL manual](https://github.com/billsargent/centurion/blob/2bf355026e3f5d4ebdf51138088b9499ccb31b09/docs/manuals/02-CPL.md), LOAD pages 179–181. The [automatic MMU placement workflow](#automatic-mmu-placement) uses the separate
`.LOAD BASIC`/map-6 mechanism and a different ABI.

### Build a module

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

### Resident caller

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

### Lifetime and ABI contract

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

The BASIC port keeps its continuation and language state resident.
[BASIC memory and execution](basic.md#memory-and-execution)
