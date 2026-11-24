# SDCC for CENTOS

`run/cpu6-os6-sdcc` compiles and links C and EE200 assembly into a
relocatable CENTOS `.exf` executable. It uses the CPU6 SDCC backend,
Fuzix-Bintools and the CENTOS C startup and runtime. Use `run/sdcc` for
direct access to the compiler frontend.

## Compile and run an application

```sh
make sdk
run/cpu6-os6-sdcc -Os --lto --gc-sections \
    examples/hello_os6.c --map build/out/hello.map -o build/out/hello.exf
run/cpu6-os6-install /path/to/CENTOS_13.IMG build/out/hello.exf \
    XHELLO6 build/out/hello.img
run/centurion-system build/out/hello.img
```

The installer adds the executable to a new disk copy. After booting
CENTOS on CRT0, enter `.RUN XHELLO6`. The emulator uses the firmware pack
prepared in [Build](../README.md#build).

The distribution image stores its applications and compiled regressions in
the `SDCC` library. Use `.DIR SDCC. 1` to see them, `.RUN SDCC.XSIEVE`
or `.RUN SDCC.XLIFE1D` to start an application, and `RUNTESTS` to launch
the compiler regression job. [Application image contents](../image/README.md)

## Build options and runtime

| Option | Effect |
| --- | --- |
| `-c` | Compile one C or assembly source into an object for separate linking |
| `-O0`, `-O1`, `-Os` | Select stack-based, speed or size code generation |
| `--lto` | Enable link-time optimization across eligible C units |
| `--gc-sections` | Discard unreachable sections when linking |
| `--stack-size N` | Reserve `N` bytes for the CENTOS C stack. The default is 1,024 |
| `--float32` | Enable software IEEE-754 single precision |
| `--native-int48` | Enable the CPU6 port's optional 48-bit integer support |

The SDK includes console I/O, formatting, native file access and
`os6_checkpoint()` for long calculations. The build records the compiler
and source hashes alongside generated executables. See
[MMU placement](mmu.md) and [overlay support](overlays.md) for programs that
need banked memory or separately loaded code.

## Performance

The measurements below count CPU6 microsteps in the microcoded emulator.
Times use the nominal 5 MHz clock and the CENTOS console runs at 9,600 baud.
For individual instruction timings, encoding sizes and addressing modes,
see the [CPU6 cost table](cpu6-instruction-costs.md). The November 2026
[SDCC benchmark comparison](sdcc-codegen-benchmarks.md) covers all 16
workloads and checks the size of each shipped executable.

### Prime sieve

The original CENTOS 13 `USAGI.XSIEVE` executable was built from the CPL
program `USAGI.ZSIEVE` credited to ren14500. The distributed C version
uses one byte per odd candidate and clears composites directly. Both runs
were checked against an independent reference for all 1,229 primes through
9,973. The kernel interval is bounded by the program's `GTIME` calls. The
full interval begins at the first call and ends at the timing report
following the prime output. [Distributed C source](../image/programs/sieve/sieve_optimise.c)

| Program | Kernel microsteps | Kernel at 5 MHz | Through the report at 5 MHz |
| --- | ---: | ---: | ---: |
| Original CPL `USAGI.XSIEVE` | 353,559,966 | 70.71 s | 97.09 s |
| Optimized C `SDCC.XSIEVE` | 535,308 | 0.107 s | 8.81 s |

The optimized C run took 44,054,442 microcycles through the report. The
algorithms, storage layouts and console formatting contribute to the
roughly 11-fold difference through the report. Earlier
development runs also built the same C sources at `-O1` with Alan Cox's
Fuzix C compiler (FCC) and the SDCC CPU6 port. Each row below compares
the kernel for one source revision under both compilers.

| C source in the development comparison | FCC microsteps | SDCC microsteps | FCC ÷ SDCC |
| --- | ---: | ---: | ---: |
| Literal translation of the CPL algorithm | 766,016,519 | 272,010,908 | 2.82× |
| Optimized byte-flags sieve | 4,100,238 | 1,651,877 | 2.48× |

### One-dimensional Life

`SDCC.XLIFE1D` implements Jon Millen's B23/S24 rule with packed bitsets,
lookup tables and one CENTOS call per rendered row. A fixed-seed build
used `-O1 --lto --gc-sections -DLIFE_SEED=1`. The same seed and rule
produced identical output in C and BASIC, with 69 cells across 27 rows.
An independent reference checked the initial row and all 26 updates.
The table gives the median of three runs from the end of the first row to
the end of the last row. [Rule reference](https://jonmillen.com/1dlife/index.html) · [C sources](../image/programs/life1d/)

| Program | Microsteps | Time at 5 MHz |
| --- | ---: | ---: |
| C compiled by SDCC | 9,893,380 | 1.98 s |
| Integer bytecode BASIC | 36,396,704 | 7.28 s |

An earlier controlled C kernel measurement compared the same 26 updates
with and without the packed-table optimization. The cost fell from
280,846 to 125,520 microsteps. In the corresponding console run, the
first-to-last-row interval fell from 10.41 to 9.89 million microsteps.
The row output accounts for much of the elapsed time.
The BASIC guide also reports a timing for Life1D under CENTOS BASIC 6.03
using the listing linked in Usagi Electric's demonstration. [Historical Life1D comparison](basic.md#one-dimensional-life)

#### CENTOS scheduling in the bytecode BASIC benchmark

The bytecode `LIFE1D` benchmark times the interval from the printed title
through the final board and checks all 27 rows. With ordinary scheduling,
the baseline and optimized builds took medians of 34,504,742 and
37,049,195 microcycles. On entry to a system call,
CENTOS compares a task timestamp at offset `0x59` with the low byte of the
level-10 clock register Z. When the values match, the check at `0x1093`
takes the path through `0x10A0`, which invokes the scheduler before the
application checkpoint (SVC 7) itself yields. This restores the same
task's MMU mappings twice per checkpoint.
[Benchmark and board oracle](../tests/basic-life.js)

For a controlled comparison, the JavaScript microcoded core set the task
timestamp once, at the first measured syscall check, to the clock value
plus the indicated offset. The clock interrupt stayed idle throughout the
timed interval. Each cell is one execution in microcycles with the same
program, seed and board oracle. The image IDs come from the BASIC manifests.

| Task timestamp | Baseline BASIC image `7723160E` | Optimized BASIC image `01702D04` |
| --- | ---: | ---: |
| Clock + 0 | 37,100,164 | 37,058,579 |
| Clock + 3 | 34,492,322 | 34,455,429 |

The optimized image used about 0.11% fewer microcycles at each matched phase.

## Verification and sources

`make tests` exercises generated code on the JavaScript microcoded core
and the independent C instruction emulator. `make tests-centos
SEED=/path/to/CENTOS_13.IMG` installs a compiled example on a disk copy,
runs it and verifies that it returns to the native shell. The application
image additionally verifies the sieve's 1,229 primes and runs the installed
compiler regression job. [Test guide](../tests/README.md)

See [the SDCC port notes](../swext/sdcc/CENTURION.md) for the backend and
frontend changes, and [bare-metal compilation](sdcc_bare_metal.md) for the
other driver.
