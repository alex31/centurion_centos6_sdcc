# Centurion CPU6 SDCC

A source distribution of the SDCC cross-compiler for the Centurion CPU6,
the EE200 assembler and linker, and a microcoded JavaScript emulator.
The build is intended for Linux and WSL2.

I discovered the Centurion through Usagi Electric's videos. I was fascinated
by his restoration series, and it made me want to port a C compiler to the
machine. [Usagi Electric's Centurion series overview](https://www.youtube.com/watch?v=KVQ_oGQ6OFI)

I've been programming in C for 40 years and had never found the time to take
on a compiler port. The arrival of large language models made the project
possible for me. I used an LLM to help me understand and adapt existing
open-source projects and put this distribution together. The original
projects and their authors are credited below.

## Build

Clone the distribution and its pinned source dependencies.

```sh
git clone --recurse-submodules https://github.com/alex31/centurion_centos6_sdcc.git
cd centurion_centos6_sdcc
```

Use GitHub credentials that can read this repository and its private
submodules. For an existing checkout, synchronize the submodule URLs after
pulling so Git can fetch the pinned revisions from the private copies.

```sh
git pull --no-recurse-submodules
git submodule sync --recursive
git submodule update --init --recursive
```

On Debian or Ubuntu, install the build dependencies. In WSL2, keep the
checkout in the Linux filesystem.

```sh
sudo apt install build-essential git flex bison libboost-dev zlib1g-dev python3 curl
```

On Ubuntu 24.04, the default native build works with the distribution's
GCC 13 and Node.js 18. Install Node.js and its development headers with
`sudo apt install nodejs libnode-dev`. The standalone JavaScript emulator
and `make sdk` require Node.js 20 or newer with npm.

Download the pinned upstream CenRE `src/cen.ts` locally and extract its
firmware tables. Keep the downloaded file, extracted ROMs and firmware-derived
build products in `build/out/`, which Git ignores.

```sh
mkdir -p build/out/firmware-source
curl -fL -o build/out/firmware-source/cen.ts \
  https://raw.githubusercontent.com/billsargent/centurion/2bf355026e3f5d4ebdf51138088b9499ccb31b09/src/cen.ts
python3 scripts/import-cenre-firmware.py build/out/firmware-source/cen.ts
```

Supply a 512-byte-stride `CENTOS_13.IMG` Hawk image. See the Centurion
archive's instructions for obtaining a system image.
[Centurion software archive](https://github.com/Nakazoto/CenturionComputer/wiki/Software#operating-system)

Build the compilers, emulator, applications and bootable CENTOS image.

```sh
./dobuild -j2 JOBS=4 SEED=/path/to/CENTOS_13.IMG
```

`dobuild` shows its five stages and a time estimate for a clean C++ build.
The terminal keeps the current stage visible below the build output. Log
files and CI output show a plain-text message at each stage change.

`dobuild` uses the native C++ machine at maximum speed for disk allocation,
image validation and guest tests. It uses Node.js to run the test scripts
through the native binding. This requires GCC 13 or newer and the Node-API headers.
The Debian and Ubuntu package `libnode-dev` provides the headers for the
distribution's Node.js package. Set `NODE_CPPFLAGS=-I/path/to/include/node`
if your Node.js installation provides headers elsewhere. Use
`CENTURION_OS6_BACKEND=javascript` with `dobuild` to run the same checks on
the reference engine.

The generated image is `build/out/image/centos-apps.img`. The firmware
import writes `build/out/emulator/firmware.json` by default. To use another
location, pass `--output` to the import script and set
`CENTURION_FIRMWARE` when running the emulator or its tests.

Use `make sdk` to build the compiler tools and emulator alone. The build
also initializes the required submodules after a plain clone. Run
`make check` for a quick check after importing the firmware, `make tests`
for regressions on both emulators or `make alltests` for the extended
suite. [Test guide](tests/README.md)

## Start a CENTOS session

```sh
run/centurion-system
```

Open the [web console](http://127.0.0.1:42646/) and click **Boot CENTOS** on
CRT0. The launcher mounts the generated application image by default. Once
`CRT0 READY` appears, try the prime sieve or open BASIC.

```text
.RUN SDCC.XSIEVE
BASIC
```

In BASIC, use `.LOAD "LIFE1D"` and then `.RUN`. At the CENTOS prompt,
`.DIR SDCC. 1` lists the C applications, and `RUNTESTS` runs the compiler
regressions installed on disk. [Application image guide](image/README.md)

In BASIC, `.LOAD "G23MTCH"` and `.RUN` start 23 Matches from David H.
Ahl's collection. [Tested BASIC games and porting status](doc/basic.md#101-basic-computer-games)

The web console provides both CRTs, CPU controls, **Load image**,
**Save image**, **Stop server**, and a choice of nominal 5 MHz or maximum
speed. Save the image before stopping the server to keep disk changes.
The server listens on loopback by default. On CRT0 over Telnet (port
2324), enter `H1` at `D=`, press Enter to accept system disk 1, then
enter the date (`MMDDYY`) and time (`HHMMSS`).

### Native emulator

Build the native server with GCC 13 or newer and the Boost headers. It uses
the same firmware pack, disk images, browser page and default ports.

```sh
make -j4 emulator-cpp
run/centurion-system-cpp
```

The build assembles the seven CPU ROMs from `build/out/emulator/firmware.json`
into a local control store under `build/out/emulator-cpp/config/`. With GCC 15
or newer, the build uses C++26 and reads a binary control store with `#embed`.
With GCC 13 on Ubuntu 24.04, Python generates a C++23 initializer using its
standard library. Both builds use the same `constexpr` decoder and specialized
microcycle handlers. Without guaranteed tail calls, the dispatcher limits
each call chain to 256 microcycles. Keep the generated control store, objects
and executables in the local build directory.

The build prefers `g++-15` when installed and otherwise uses `g++`. Set
`CXX=g++-13` explicitly to select GCC 13 on a system with both compilers.

Set `FIRMWARE_JSON=/absolute/path/to/pack.json` to specialize for another pack.
At startup, the server compares every control-store byte to select the
compiled handlers. Other microcode uses the runtime decoder. Both engines
load the mapping ROM and peripheral firmware from the runtime pack and clock
peripherals at the same points.

Use `make emulator-cpp ARCHFLAGS= FIRMWARE_JSON=` for a portable build with
only the generic engine. The default build tunes the binary for the local CPU.

After the native build, `run/centurion-system` selects it automatically.
Set `CENTURION_EMULATOR=javascript` to run the reference or
`CENTURION_EMULATOR=cpp` to require the native build. Both launchers accept
an explicit Hawk image path. On the native server, `CENTURION_TELNET_BASE`
and `CENTURION_WEB_PORT` set alternate ports for running both servers side
by side.

On the Telnet control panel, `m` shows the microinstruction, ALU and
sequencer stack. Use `g` to install or remove the diagnostic ROMs and
`+` and `-` to change the diagnostic selector. To boot the diagnostic
ROM from a halted panel, reset with `x`, check that the ROMs are on with `g`,
enable sense switch 1 with `o`, then start with `r`.

After building the JavaScript reference and the application image, run
`make tests-emulator-cpp` to test the native peripherals, Telnet, Kermit
and Web console and compare two CENTOS workloads at maximum speed.
To run just the speed comparison:

```sh
node tests/apps-image.js build/out/image/centos-apps.img 44 --compare
```

## Compiler targets

Both drivers use the Centurion CPU6 port of SDCC.

| Target | Driver | Guide |
| --- | --- | --- |
| CENTOS applications | `run/cpu6-os6-sdcc` | [CENTOS SDK and executable format](doc/sdcc.md) |
| Bare metal | `run/cpu6-bare-sdcc` | [Bare-metal linking and hardware access](doc/sdcc_bare_metal.md) |

The CENTOS driver produces relocatable `.exf` files. The bare-metal driver
produces a raw `.bin` linked at a specified load address, `0x2000` by default.

## BASIC example

Once the toolchain was in place, I adapted Stefan's BASIC as an example of
how to combine C and CPU6 assembly in a CENTOS application. The performance
goal is to compete with BASIC on 1980s 8-bit microcomputers, using published
timings as references for benchmarks on the emulated CPU6.

Stored programs compile to typed bytecode. The REPL includes an editor,
and `DEFINT` selects 16-bit integer variables. BASIC also supports
single-precision real arithmetic, string variables and structured loops
with `FOR/NEXT` and `WHILE/WEND`. [BASIC guide and benchmark commands](doc/basic.md)

## Performance

- The SDCC C sieve produced all 1,229 primes through 9,973 in **9.62 s**,
  including console output. The original CENTOS CPL sieve took **97.09 s**
  for the same primes. [Sieve measurements and compiler comparisons](doc/sdcc.md#prime-sieve)
- The C and BASIC versions of `LIFE1D` reproduce Jon Millen's rule with
  the same fixed seed and 27 output rows. From the first row to the last,
  the C program took **1.98 s** and integer BASIC took **7.86 s**.
  Life1D took **12 min 38 s** for its first
  26 generations under CENTOS BASIC 6.03.
  [C Life1D](doc/sdcc.md#one-dimensional-life) · [BASIC Life1D and 8-bit benchmarks](doc/basic.md#performance)

These times use the microcoded CPU6 emulator at its nominal 5 MHz rate.

## Upstream projects and credits

| Project | Role | Upstream |
| --- | --- | --- |
| SDCC | Cross-compiler adapted to CPU6 | [Small Device C Compiler](https://sdcc.sourceforge.net/) |
| Fuzix-Bintools | EE200 assembler and linker, from Alan Cox's Fuzix tools | [Fuzix-Bintools](https://codeberg.org/EtchedPixels/Fuzix-Bintools) |
| Fuzix Compiler Kit, EE200 runtime | Historical runtime assembly and provenance | [EE200 support sources](https://github.com/EtchedPixels/Fuzix-Compiler-Kit/tree/7b2a72edea6967e7df0b32e1e898fdea1754cc67/supportee200) |
| CenRE and the Centurion JavaScript server | Modified emulator derived from Meisaka Yukara's CenRE and the server adaptation by billsargent | [Centurion JavaScript emulator](https://github.com/billsargent/centurion) |
| Centurion C emulator | Independent instruction core used by compiler tests | [EtchedPixels/Centurion](https://github.com/EtchedPixels/Centurion) |
| Stefan's BASIC | POSIX interpreter used as the base for the CENTOS application | [slviajero/tinybasic](https://github.com/slviajero/tinybasic) |
| 101 BASIC Computer Games | Listings collected by David H. Ahl and transcribed by Maury Markowitz and contributors | [maurymarkowitz/101-BASIC-Computer-Games](https://github.com/maurymarkowitz/101-BASIC-Computer-Games) |
| mcu-max and micro-Max | Chess engine on the application image | [mcu-max](https://github.com/Gissio/mcu-max) · [H. G. Muller's micro-Max](https://home.hccnet.nl/h.g.muller/max-src2.html) |
| E-Kermit | File-transfer protocol used by the CENTOS receiver | [Frank da Cruz's E-Kermit](https://www.kermitproject.org/ek.html) |
| TypeScript and ws | Emulator build and WebSocket server | [TypeScript](https://github.com/microsoft/TypeScript) · [ws](https://github.com/websockets/ws) |

The [Centurion Computer project](https://github.com/Nakazoto/CenturionComputer)
provides hardware reference material and reverse-engineering documentation.
See [source revisions](doc/sources.md) and [licenses](LICENSE.md) for original
revisions, local adaptations and component notices.
