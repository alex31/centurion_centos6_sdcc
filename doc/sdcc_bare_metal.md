# SDCC for bare-metal Centurion CPU6

`run/cpu6-bare-sdcc` compiles and links C and EE200 assembly into a raw
`.bin`. By default the first byte belongs at address `0x2000`, which is also
the entry point of the bundled startup code.

## Build and run a smoke test

```sh
make sdk
mkdir -p build/out/bare
run/cpu6-bare-sdcc -O1 --gc-sections \
    tests/smoke.c --map build/out/bare/smoke.map -o build/out/bare/smoke.bin
node tests/compiler-audit-core.js build/out/bare tests/smoke.json
```

The last command loads the raw program into emulated memory, sets the
program counter and checks the result. It uses the local firmware pack
described in [Build](../README.md#build).

The `--origin` option sets the hexadecimal link address. Use the same
address when loading the binary. For the test runner, supply it through
`CPU6_BARE_ORIGIN`. For example:

```sh
run/cpu6-bare-sdcc --origin 3000 tests/smoke.c -o build/out/bare/smoke.bin
CPU6_BARE_ORIGIN=3000 node tests/compiler-audit-core.js build/out/bare tests/smoke.json
```

The supplied startup sets up the stack, clears BSS and initializes MUX
channel 0 for the polled console. The linker checks the program against
the diagnostic ROM boundary at `0x8000`. The driver accepts separately
compiled objects and archives, as well as C and assembly sources.
Use `--map` to inspect the memory layout.

## Access hardware

`<system_mmio.h>` provides the reverse-engineered CPU6 device addresses
and `CENTURION_MMIO8(addr)` for volatile 8-bit accesses. [MMIO definitions](../baremetal/include/system_mmio.h)

`<system_finch.h>` exposes the FFC registers and the Finch driver. The
default base is `0xF800` and can be changed with `CENTURION_FFC_BASE` at
build time. The driver initializes the device, reads and writes
400-byte sectors, and formats a track or a disk. It expects exclusive
use of the FFC and CPU DMA, with buffers in identity-mapped RAM.
The format used by the driver has 29 sectors per track.
[Finch declarations and return codes](../baremetal/include/system_finch.h)

`make tests-finch` formats a scratch track, reads and replaces a sector, and
verifies that adjacent data is preserved on the microcoded emulator.
The filesystem format and boot files are separate parts of a bootable disk.
For applications running under the operating system, see the
[CENTOS driver](sdcc.md).
