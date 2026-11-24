# Centurion CPU6 SDCC

A source distribution of the SDCC cross-compiler for the Centurion CPU6, the EE200 assembler and linker, and a microcoded JavaScript emulator. The project supports Ubuntu 24.04 LTS and 26.04 LTS on native Linux and under WSL2.

I discovered the Centurion through Usagi Electric's videos. I was fascinated by his restoration series, and it made me want to port a C compiler to the machine. [Usagi Electric's Centurion series overview](https://www.youtube.com/watch?v=KVQ_oGQ6OFI)

I've been programming in C for 40 years and had never found the time to take on a compiler port. The arrival of large language models made the project possible for me. I used an LLM to help me understand and adapt existing open-source projects and put this distribution together. The original projects and their authors are credited below.

## Build

The full C++ build has been tested on Ubuntu 24.04 and 26.04 in Linux environments and on Ubuntu 24.04 under WSL2.

### Optional setup for Windows 11 with WSL2

If WSL reports that hardware virtualization is disabled, enable it in the PC's UEFI settings.

**In PowerShell**, run the command for your chosen Ubuntu release from an administrator window.

- Ubuntu 24.04 LTS — `wsl --install -d Ubuntu-24.04`

- Ubuntu 26.04 LTS — `wsl --install -d Ubuntu-26.04`

Restart Windows if prompted. After signing back in, check the distribution and its WSL version.

```
wsl -l -v
```

If your Ubuntu distribution is missing from the list, rerun its install command. If it shows version `1`, convert it with `wsl --set-version <distribution> 2`. Open Ubuntu from the Start menu or run `wsl -d <distribution>`. Replace `<distribution>` with the name shown by `wsl -l -v`. Choose a Linux username and password at the first launch. [Microsoft WSL installation guide](https://learn.microsoft.com/en-us/windows/wsl/install)

**In Ubuntu**, prepare a directory in your Linux home for the checkout, then continue with the [shared build steps below](#shared-build-steps-for-linux-and-wsl2). Keeping the checkout and build output under `~/src` gives Linux tools faster file access. Use `/mnt/c` to copy inputs from Windows when needed. [Working across Windows and Linux file systems](https://learn.microsoft.com/en-us/windows/wsl/filesystems)

```
mkdir -p "$HOME/src"
cd "$HOME/src"
```

### Shared build steps for Linux and WSL2

On Debian or Ubuntu, install the build dependencies in the Linux shell.

```
sudo apt update
sudo apt install build-essential git flex bison libboost-dev zlib1g-dev python3 curl
sudo apt install nodejs libnode-dev
```

The native build works with the GCC 13 and Node.js 18 packages on Ubuntu 24.04. For the JavaScript fallback and `make sdk`, use Node.js 20 or newer with npm in the Linux shell. If your distribution ships an older version, install a current LTS release inside Linux and check `node --version` and `npm --version`. [Node.js downloads](https://nodejs.org/en/download)

Clone the distribution and its pinned source dependencies. Use GitHub credentials with access to this repository and its private submodules.

```
git clone --recurse-submodules https://github.com/alex31/centurion_centos6_sdcc.git
cd centurion_centos6_sdcc
```

For an existing checkout, synchronize the submodule URLs after pulling so Git can fetch the pinned revisions from the private copies.

```
git pull --no-recurse-submodules
git submodule sync --recursive
git submodule update --init --recursive
```

Download the pinned upstream CenRE `src/cen.ts` locally and extract its firmware tables. Keep the downloaded file, extracted ROMs and firmware-derived build products in `build/out/`, which Git ignores.

```
mkdir -p build/out/firmware-source
curl -fL -o build/out/firmware-source/cen.ts \
  https://raw.githubusercontent.com/billsargent/centurion/2bf355026e3f5d4ebdf51138088b9499ccb31b09/src/cen.ts
python3 scripts/import-cenre-firmware.py build/out/firmware-source/cen.ts
```

Supply a 512-byte-stride `CENTOS_13.IMG` Hawk image. See the Centurion archive's instructions for obtaining a system image. [Centurion software archive](https://github.com/Nakazoto/CenturionComputer/wiki/Software#operating-system)

Build the compilers, emulator, applications and bootable CENTOS image.

```
./dobuild -j2 JOBS=4 SEED=/path/to/CENTOS_13.IMG
```

For example, to test the native build in WSL2 with a seed saved in Windows Downloads, run the following in Ubuntu from the project root after importing the firmware. Replace `YOUR_WINDOWS_USER` with your Windows username.

```
mkdir -p build/out/image
cp "/mnt/c/Users/YOUR_WINDOWS_USER/Downloads/CENTOS_13.IMG" build/out/image/
./dobuild -j4 JOBS=4 CENTURION_OS6_BACKEND=cpp SEED="$PWD/build/out/image/CENTOS_13.IMG"
```

`dobuild` shows its five stages and a time estimate for a clean C++ build. The terminal keeps the current stage visible below the build output. Log files and CI output show a plain-text message at each stage change.

`dobuild` tries the native C++ emulator first. If its build fails, `dobuild` prints a message, builds the JavaScript emulator and continues with it for disk allocation, image validation and guest tests. These steps usually take longer with the JavaScript emulator.

The native build uses GCC 13 or newer and the Node-API headers. Set `NODE_CPPFLAGS=-I/path/to/include/node` if your Node.js installation provides headers elsewhere. Set `CENTURION_OS6_BACKEND=javascript` to select JavaScript directly, or `CENTURION_OS6_BACKEND=cpp` to require the native build.

The generated image is `build/out/image/centos-apps.img`. The firmware import writes `build/out/emulator/firmware.json` by default. To use another location, pass `--output` to the import script and set `CENTURION_FIRMWARE` when running the emulator or its tests.

Use `make sdk` to build the compiler tools and emulator alone. The build also initializes the required submodules after a plain clone. Run `make check` for a quick check after importing the firmware, `make tests` for regressions on both emulators or `make alltests` for the extended suite. [Test guide](doc/sdcc.md#verification)

## Start a CENTOS session

```sh
run/centurion-system
```

Pass an explicit disk image path to open a different copy. Build reports,
executable maps and the mapping of installed regression names to their
sources are grouped by run under `build/out/image/work/`.

Open the [web console](http://127.0.0.1:42646/) and click **Boot CENTOS** on CRT0. The launcher mounts the generated application image by default. Once `CRT0 READY` appears, try the prime sieve or open BASIC.

```
.RUN SDCC.XSIEVE
BASIC
```

In BASIC, use `.LOAD "LIFE1D"` and then `.RUN`. At the CENTOS prompt, `.DIR SDCC. 1` lists the C applications, and `RUNTESTS` runs the compiler regressions installed on disk. [Application image guide](#centos-applications)

In BASIC, `.LOAD "G23MTCH"` and `.RUN` start 23 Matches from David H. Ahl's collection. [BASIC games](doc/basic.md#101-basic-computer-games)

The web console provides both CRTs, CPU controls, **Load image**, **Save image**, **Stop server**, and a choice of nominal 5 MHz or maximum speed. Save the image before stopping the server to keep disk changes. The server listens on loopback by default. On CRT0 over Telnet (port 2324), enter `H1` at `D=`, press Enter to accept system disk 1, then enter the date (`MMDDYY`) and time (`HHMMSS`).

### CENTOS applications

| CENTOS command | Program |
| --- | --- |
| `BASIC` | Start Stefan BASIC from the `SBASIC` library |
| `.RUN SDCC.XSIEVE` | SDCC-compiled prime sieve |
| `.RUN SDCC.XCONNECT4` | Connect Four |
| `.RUN SDCC.XCHOMP` | Chomp against the computer on boards from 1 × 1 through 9 × 9 |
| `.RUN SDCC.XCHESS` | Chess |
| `.RUN SDCC.XLIFE1D` | One-dimensional Life in C |
| `.RUN SDCC.XKERMIT` | E-Kermit receiver for CRT1 |
| `RUNTESTS` | Run the 44 CENTOS executables from the main CPU6 compiler regression suite |

Chomp accepts boards from 1 × 1 to 9 × 9. Enter the row then the column,
as in `1 2`, and use `Q` to return to CENTOS. To build a separate Chomp
image with a root `.RUN XCHOMP` command, use
`make -C image/programs/chomp image SEED=/path/to/input.img CHOMP_IMAGE=/path/to/new.img`.

For E-Kermit, assign CRT1 to `SYS000` and a preallocated temporary CENTOS
executable file on the target disk to `SYS001` before running
`SDCC.XKERMIT`. Transfer over the raw CRT1 serial port. The receiver
carries packets in printable lines and supports long packets and a 16-bit
CRC. [Verified command sequence](tests/cpp-telnet.py)

The web console shares the machine with Telnet. Upload a Hawk image
from the console and save changes to a new copy. **Boot CENTOS** follows
the boot prompts with the browser's local date and time and opens the
image picker if the drive is empty. In VT100 application cursor mode,
the browser sends the keys used by BASIC `.EDIT` and `.LIST`.
The JavaScript build regenerates its code from TypeScript with locked npm
dependencies. It validates the local firmware pack before starting the CPU.
See [firmware provenance](LICENSE.md#source-provenance).

### Native emulator

Build the native server with GCC 13 or newer and the Boost headers. It uses the same firmware pack, disk images, browser page and default ports.

```
make -j4 emulator-cpp
run/centurion-system-cpp
```

GCC 15 and newer use C++26 and `#embed`. GCC 13 uses a C++23
initializer for the same ROMs. The dispatcher limits each call chain
to 256 microcycles when the compiler cannot guarantee tail calls.
[Native emulator guide](emulator/README.md)

The build prefers `g++-15` when installed and otherwise uses `g++`. Set `CXX=g++-13` explicitly to select GCC 13 on a system with both compilers.

Set `FIRMWARE_JSON=/absolute/path/to/pack.json` to specialize for another pack. At startup, the server compares every control-store byte to select the compiled handlers. Other microcode uses the runtime decoder. Both engines load the mapping ROM and peripheral firmware from the runtime pack and clock peripherals at the same points.

Use `make emulator-cpp ARCHFLAGS= FIRMWARE_JSON=` for a portable build with only the generic engine. The default build tunes the binary for the local CPU.

After the native build, `run/centurion-system` selects it automatically. Set `CENTURION_EMULATOR=javascript` to run the reference or `CENTURION_EMULATOR=cpp` to require the native build. Both launchers accept an explicit Hawk image path. On the native server, `CENTURION_TELNET_BASE` and `CENTURION_WEB_PORT` set alternate ports for running both servers side by side.

On the Telnet control panel, `m` shows the microinstruction, ALU and sequencer stack. Use `g` to install or remove the diagnostic ROMs and `+` and `-` to change the diagnostic selector. To boot the diagnostic ROM from a halted panel, reset with `x`, check that the ROMs are on with `g`, enable sense switch 1 with `o`, then start with `r`.

After building the JavaScript reference and the application image, run `make tests-emulator-cpp` to test the native peripherals, Telnet, Kermit and Web console and compare two CENTOS workloads at maximum speed. To run just the speed comparison:

```
node tests/apps-image.js build/out/image/centos-apps.img 44 --compare
```

## Compiler targets

Both drivers use the Centurion CPU6 port of SDCC.

| Target | Driver | Guide |
| - | - | - |
| CENTOS applications | `run/cpu6-os6-sdcc` | [CENTOS SDK and executable format](doc/sdcc.md) |
| Bare metal | `run/cpu6-bare-sdcc` | [Bare-metal linking and hardware access](doc/sdcc.md#bare-metal) |


The CENTOS driver produces relocatable `.exf` files. The bare-metal driver produces a raw `.bin` linked at a specified load address, `0x2000` by default.

## BASIC example

Once the toolchain was in place, I adapted Stefan's BASIC as an example of how to combine C and CPU6 assembly in a CENTOS application. The performance goal is to compete with BASIC on 1980s 8-bit microcomputers, using published timings as references for benchmarks on the emulated CPU6.

Stored programs compile to typed bytecode. The REPL includes an editor, and `DEFINT` selects 16-bit integer variables. BASIC also supports single-precision real arithmetic, string variables and structured loops with `FOR/NEXT` and `WHILE/WEND`. [BASIC guide](doc/basic.md)

## Performance

- The SDCC C sieve produced all 1,229 primes through 9,973 in **8.81 s**, including console output. The original CENTOS CPL sieve took **97.09 s** for the same primes. [Sieve measurements and compiler comparisons](doc/performance.md#prime-sieve)

- The C and BASIC versions of `LIFE1D` reproduce Jon Millen's rule with the same fixed seed and 27 output rows. From the first row to the last, the C program took **1.98 s** and integer BASIC took **6.76 s**. Life1D took **12 min 38 s** for its first 26 generations under CENTOS BASIC 6.03. [C Life1D](doc/performance.md#one-dimensional-life) · [BASIC Life1D and 8-bit benchmarks](doc/performance.md#basic-benchmarks)

These times use the microcoded CPU6 emulator at its nominal 5 MHz rate.  [Benchmark results](doc/performance.md)

## Upstream projects and credits

| Project | Role | Upstream |
| --- | --- | --- |
| SDCC | Cross-compiler adapted to CPU6 | [Small Device C Compiler](https://sdcc.sourceforge.net/) |
| - | - | - |
| Fuzix-Bintools | EE200 assembler and linker, from Alan Cox's Fuzix tools | [Fuzix-Bintools](https://codeberg.org/EtchedPixels/Fuzix-Bintools) |
| Fuzix Compiler Kit, EE200 runtime | Historical runtime assembly and provenance | [EE200 support sources](https://github.com/EtchedPixels/Fuzix-Compiler-Kit/tree/7b2a72edea6967e7df0b32e1e898fdea1754cc67/supportee200) |
| CenRE and the Centurion JavaScript server | Modified emulator derived from Meisaka Yukara's CenRE and the server adaptation by billsargent | [Centurion JavaScript emulator](https://github.com/billsargent/centurion) |
| Centurion C emulator | Independent instruction core used by compiler tests | [EtchedPixels/Centurion](https://github.com/EtchedPixels/Centurion) |
| Stefan's BASIC | POSIX interpreter used as the base for the CENTOS application | [slviajero/tinybasic](https://github.com/slviajero/tinybasic) |
| 101 BASIC Computer Games | Listings collected by David H. Ahl and transcribed by Maury Markowitz and contributors | [maurymarkowitz/101-BASIC-Computer-Games](https://github.com/maurymarkowitz/101-BASIC-Computer-Games) |
| mcu-max and micro-Max | Chess engine on the application image | [mcu-max](https://github.com/Gissio/mcu-max) · [H. G. Muller's micro-Max](https://home.hccnet.nl/h.g.muller/max-src2.html) |
| E-Kermit | File-transfer protocol used by the CENTOS receiver | [Frank da Cruz's E-Kermit](https://www.kermitproject.org/ek.html) |
| TypeScript and ws | Emulator build and WebSocket server | [TypeScript](https://github.com/microsoft/TypeScript) · [ws](https://github.com/websockets/ws) |


The [Centurion Computer project](https://github.com/Nakazoto/CenturionComputer) provides hardware reference material and reverse-engineering documentation. See [source revisions](LICENSE.md#source-provenance) and [licenses](LICENSE.md) for original revisions, local adaptations and component notices.
