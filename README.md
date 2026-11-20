# Centurion SDCC

Source distribution of the SDCC cross-compiler for Centurion CPU6, the EE200
assembler and linker, and the microcoded JavaScript emulator. The build runs
on Linux and targets the Linux environment under WSL2.

## Build

Install Git, GNU Make, a C/C++ toolchain with binutils, Flex, Bison or Berkeley
Yacc, Boost headers, zlib headers, Python 3.9 or newer, and Node.js 20 or newer
with npm. For Debian or Ubuntu, install the native build dependencies with:

```sh
sudo apt install build-essential git flex bison libboost-dev zlib1g-dev python3
node --version
npm --version
```

Run the build from a checkout in the Linux filesystem under WSL2.

```sh
make -j2 JOBS=4
python3 scripts/import-cenre-firmware.py /path/to/local/cen.ts
make check
```

The emulator reads a firmware pack supplied locally. The import command
extracts the tables from a local firmware-bearing CenRE source file. It
writes `build/out/emulator/firmware.json` by default. To store the pack
outside the build directory, give the import command `--output` and set
`CENTURION_FIRMWARE` to that file before running the emulator or tests.

The Makefile initializes required submodules at the pinned commits even after
a plain clone. Use `make submodules` to fetch the build and test components,
including BASIC and the C instruction emulator. `make toolchain` and `make emulator` build the two
components independently. All generated files go under `build/out/`. Use
`make clean` to remove them.

`make check` compiles and runs a CPU6 arithmetic program in the JavaScript
core and builds a CENTOS executable. It runs without a disk image or network
listeners. Use `make tests` for the CPU6 regression on both emulators and
`make alltests` for the extended suite. See the [test guide](tests/README.md).

## Compile for CPU6

```sh
run/sdcc --version
run/cpu6-bare-sdcc -O1 tests/smoke.c -o build/out/smoke.bin
run/cpu6-os6-sdcc -Os --lto --gc-sections examples/hello_os6.c -o build/out/hello.exf
```

`run/sdcc` runs the underlying SDCC frontend with `-mcpu6`.
Use the two drivers to assemble and link a bare-metal program or a relocatable
CENTOS executable. Both support separate compilation with `-c`, linked
objects and archives, `--float32`, and `--native-int48`. See
[MMU placement](doc/mmu.md) and [C overlays](doc/overlays.md) for the
corresponding CENTOS drivers.

## Install and run under CENTOS

Provide a 512-byte-stride CENTOS Hawk image. Use the installer to add the
example to a new disk copy and launch the emulator on that copy.

```sh
run/cpu6-os6-install /path/to/CENTOS_13.IMG build/out/hello.exf XHELLO6 build/out/hello.img
run/centurion-system build/out/hello.img
```

Open [the web console](http://127.0.0.1:42646/) in your browser. It shows
CRT0 and CRT1, CPU registers, run state and mounted disks. Use **Real time
(5 MHz)** to pace the simulated CPU or **Maximum speed** to run as fast as
the host allows. The selection takes effect while the machine is running
and can also be changed while paused. Click a CRT to type or paste. At the
ROM's `D=` prompt, **Boot CENTOS** sends `H1`,
accepts system disk 1 and enters the browser's current local date and time
at the CENTOS prompts. On the standard ROM the prompt appears on CRT0.

**Load image** selects a raw Hawk image on your computer, mounts it in
DSK2 unit 1 and resets the CPU. Use **Run** if the CPU is paused. The
console accepts 400-byte and 512-byte sector strides. **Save image** downloads a copy of the current contents of that
drive. **Stop server** closes the emulator process. Save the image before
stopping the server to retain in-memory changes.

Connect a Telnet client to `127.0.0.1:2324`. At the ROM's `D=` prompt,
enter `H1`. Press Enter to accept system disk 1, then enter the date
(`MMDDYY`) and time (`HHMMSS`). At the CENTOS console, enter `.RUN XHELLO6`.
With a generated application image available, the launcher mounts it automatically.
When no disk is mounted, **Boot CENTOS** opens the image picker at `D=`.

The emulator listens on loopback by default. The web console uses HTTP and
WebSocket on port 42646. The Telnet control panel uses TCP
port 2323, CRT0 uses 2324, and CRT1 uses 2325. Disk management is on 2326 and
raw CRT1 is on 2327. At the server prompt, `save <new-file.img>` exports the
disk in memory and `q` stops the server.

## Build the complete application image

Run `./dobuild SEED=/path/to/CENTOS_13.IMG` to build the compiler, the
applications and the CENTOS disk image using your local seed and firmware
pack. The result is `build/out/image/centos-apps.img`.
See the [image build guide](image/README.md) for its applications and
`RUNTESTS`. Use `make sdk` for the compiler and emulator alone.

## Build BASIC

```sh
make basic
make basic-image SEED=/path/to/CENTOS_13.IMG
run/centurion-system build/out/basic/centos-basic.img
```

Enter `BASIC` at the CENTOS prompt. Its files are in the `SBASIC` library. See
[Stefan BASIC for Centurion](swext/stefan-basic/CENTURION.md).

## Sources

| Submodule | Centurion changes |
| --- | --- |
| SDCC | [CPU6 backend and frontend](swext/sdcc/CENTURION.md) |
| Fuzix-Bintools | [EE200 assembler and CBIN linker](swext/fuzix-bintools/CENTURION.md) |
| Historical EE200 runtime | Optional provenance checkout. The SDK builds its [native helpers](lib/compiler/README.md) |
| C instruction emulator | [Regression test core](swext/centurion-c/CENTURION.md) |
| JavaScript emulator | [Headless CENTOS integration](swext/centurion-js/CENTURION.md) |
| Stefan BASIC | [CENTOS port and typed bytecode VM](swext/stefan-basic/CENTURION.md) |
| mcu-max | [Chess engine](swext/mcu-max/CENTURION.md) |
| E-Kermit | [CENTOS receiver](swext/ekermit/CENTURION.md) |

See [source provenance and publication](doc/sources.md) and [licenses](LICENSE.md).
