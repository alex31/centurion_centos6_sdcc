# Centurion CPU6 performance

## Application and libc update

Distribution revision `2d995a3`, which introduced the memory and string
library, used SDCC `58c95ff51` and Stefan BASIC `e89e179`. Relative to
distribution `9caf89a`, the eleven BASIC benchmarks used **2.07% fewer
microcycles** on average, weighting each program equally. Both images ran the same workloads three times.

| Workload | Previous microcycles | Updated microcycles | Reduction |
| --- | ---: | ---: | ---: |
| BM6 | 88,500,690 | 78,767,136 | 11.00% |
| BM7 | 126,331,026 | 116,698,268 | 7.63% |
| BM8 | 339,044,238 | 330,271,234 | 2.59% |
| AHL | 123,469,327 | 121,882,161 | 1.29% |
| BASIC `.LIST`, 13 numeric lines | 6,927,095 | 4,738,398 | 31.60% |
| Connect Four search, five moves | 36,431,675 | 32,807,285 | 9.95% |

The SDCC binary32 runtime uses CLZ to normalize an unsigned integer for
conversion to float. This favors small inputs. In the conversion routine
alone, the values 0 through 1,000 took 66.40% fewer microcycles, while
4,096 uniformly distributed 32-bit inputs took 19.06% more.
BASIC's decimal formatter cancels powers of two with CTZ before expanding
a fraction. The `.LIST` timings cover the echoed command through the last
displayed line, with identical output in all three runs.

In that revision, Connect Four packed column pairs and used `bswap16` to
check both diagonals. Its code shrank from 5,113 to 5,105 bytes, with the
same moves and boards.
Across all 64 CENTOS files, load-record bytes fell from 147,339 to 147,262.
Each file kept its previous on-disk size, totaling 181,200 bytes. The
[string functions](sdcc.md#memory-and-strings) are linked individually.

## Compiler comparison at revision `28ba3f7`

The comparison starts at distribution revision `933f46c` and SDCC revision
`e69c8e6a7`. Both builds use the same application sources, compiler flags,
CENTOS seed and CPU6 microcode. The optimized build uses distribution
revision `28ba3f7` and SDCC revision `abf0207e3`.

## Guest execution

Each workload contributes equally. A reduction is
`100 × (1 − candidate microcycles / baseline microcycles)`. A negative value
means that workload ran more slowly. The timings include normal guest I/O
and scheduling within the stated intervals. They were measured on the
microcoded emulator at its nominal CPU6 rate of 5 MHz.

| Workload | Baseline microcycles | Optimized microcycles | Reduction |
| --- | ---: | ---: | ---: |
| BM1 | 3,624,250 | 3,226,775 | 10.97% |
| BM2 | 20,281,218 | 11,901,321 | 41.32% |
| BM3 | 59,502,268 | 37,714,096 | 36.62% |
| BM4 | 67,517,222 | 43,252,980 | 35.94% |
| BM5 | 71,407,919 | 47,087,976 | 34.06% |
| BM6 | 122,064,813 | 88,492,536 | 27.50% |
| BM7 | 160,617,166 | 126,331,026 | 21.35% |
| BM8 | 494,677,452 | 339,044,238 | 31.46% |
| AHL | 162,054,240 | 123,465,243 | 23.81% |
| INTBITS | 3,522,872 | 3,522,735 | <0.01% |
| REALCOPY | 4,070,872 | 4,070,735 | <0.01% |
| Sieve (C) | 790,454 | 535,308 | 32.28% |
| Life1D (C) | 9,893,521 | 9,893,380 | <0.01% |
| Life1D (BASIC) | 34,504,742 | 37,049,195 | -7.37% |
| Connect Four (C) | 37,191,499 | 36,435,394 | 2.03% |
| Chess (C) | 374,183,547 | 356,298,760 | 4.78% |

The equal-weight mean reduction was **18.42%** across all 16 workloads.
The eleven BASIC benchmarks averaged **23.91%**, and the four C workloads
averaged **9.77%**. Bytecode Life took **7.37% more** microcycles in the
median run. The main comparison includes that elapsed time. A separate
scheduler-phase experiment isolated the cost of a second CENTOS scheduling pass.

In a second build, the optimized compiler and LTO rebuilt the original
binary32 library from its baseline sources. This measured a **6.86%**
mean reduction across all 16 workloads and **6.42%** across the eleven
BASIC benchmarks. The larger end-to-end
gain includes the CPU6 target library changes. Several benchmarks spend
much of their time in the binary32 routines called by SDCC-generated code.

## Validation and reproduction

The eleven BASIC benchmark programs use the same console markers as
`make bench-basic`. Timing starts after bytecode compilation and ends before
the final values are checked. Three fresh sessions ran the same program order
for each image. The sieve checks all 1,229 prime values and counts the
microcycles between its two clock calls. The C Life build uses
`-DLIFE_SEED=1` and measures from the first printed board to the last.
Bytecode Life uses the interval from the title to the last board.
Both versions verify 27 rows against the independent board oracle. The table
uses the median of three timing samples for these workloads.
[Benchmark definitions](../tests/basic-benchmark.js) · [Life timing and scheduler controls](#centos-scheduling-in-the-bytecode-basic-benchmark)

The two bare-metal C controls use the distributed game engines. Connect
Four matches the host move sequence and board hashes, including the
arguments to all 580 search calls. Chess checks legal and best moves, node
counts and board state on nine fixed positions. The compiler-generated
binaries were timed from entry to return on the microcoded core and were
also executed on the independent instruction emulator. Stack use was 174
bytes for Connect Four and 863 bytes for chess in both builds.

The 64 CENTOS executable files comprise the six applications, 44 regression
executables, the BASIC resident program, mapped module, eleven overlays
and the module probe. Each file was compared against its counterpart by
physical size and by the sum of its CBIN load records before sector padding.
All 64 files were the same size or smaller individually by both measures.
Total load-record bytes fell from 159,048 to 147,339 and disk bytes from
194,000 to 181,200. The two additional bare-metal game controls shrank from 2,950 to 2,846 bytes
and from 10,036 to 9,600 bytes respectively.

For the eleven BASIC benchmarks, build each revision in a separate checkout
using the same seed and firmware. Run the following from each checkout.
The JSON records the exact microcycle counts.

```sh
./dobuild -j4 JOBS=4 CENTURION_OS6_BACKEND=cpp
CENTURION_OS6_BACKEND=cpp node tests/basic-benchmark.js \
    build/out/image/centos-apps.img build/out/bench-basic.json \
    BM1,BM2,BM3,BM4,BM5,BM6,BM7,BM8,AHL,INTBITS,REALCOPY
```

When testing the baseline with the native core, use the updated benchmark
script in the baseline checkout as well. Its newline timestamps were
cross-checked against the original per-microcycle counter on BM1 and BM2.

The full image build passed all 44 CENTOS regression programs, 24 BASIC
games, the Life board oracle and the VT100 game checks. All 39 extended
suites passed. The 22 compiler regression programs were also linked as
bare-metal binaries at `-O0`, `-O1` and `-Os`. All 66 executed successfully
and each linked file was the same size or smaller. The instruction and
microcoded emulators both passed the float edge-case tests, including
subnormals, signed zeros, infinities, NaNs, cancellation and half-ULP ties.

## Prime sieve

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

The C kernel took 535,308 microcycles in each of three runs on the
JavaScript microcoded emulator. Including output through the timing
report, the runs took 44,050,192–44,054,384 microcycles (8.81 s).

## One-dimensional Life

`LIFE1D` applies Jon Millen's B23/S24 rule to 69 cells. The `1DLIFE_CEN.BAS`
listing linked in Usagi Electric's demonstration uses the same rule and width.
It starts with `RND(1)`, updates the display with individual `CURSOR` and
`PRINT` commands, and starts a fresh series after every 26 generations.
[Rule reference](https://jonmillen.com/1dlife/index.html) · [Published source](https://github.com/Nakazoto/CenturionComputer/blob/8682b5865979315cf9e085e75ff8547ff3be473e/Software/New%20Software/1DLIFE_CEN.BAS) · [Video demonstration](https://www.youtube.com/watch?v=bpdYBsgYF_c&t=775s)

The historical benchmark imported that source unchanged as the type A
file `ZUELIFE` on a copy of a CENTOS 13 disk. The job `P.BASIC UELIFE 1 CRT0`
compiled it with BCOM 6.03, with an empty library response.
`.LOAD BASIC` loaded the CENTOS monitor and `.RUN XUELIFE` started the
program. The emulator counted microsteps from the end of the initial
frame to the end of the 26th update, including terminal rendering.
An independent model checked the interior cells of each update.

All three timings cover the end of the initial row to the end of the 26th
update, at the CPU6's nominal 5 MHz rate.

| Program | CPU6 microsteps | Time at 5 MHz |
| --- | ---: | ---: |
| `1DLIFE_CEN.BAS` on CENTOS BASIC 6.03 | 3,789,764,651 | 757.95 s (12 min 38 s) |
| Distributed integer bytecode BASIC | 33,806,724 | 6.76 s |
| Distributed C compiled by SDCC | 9,893,529 | 1.98 s |

The CENTOS BASIC 6.03 row reports one full emulator run, with generation times
between 28.79 and 29.45 s. The C and BASIC benchmark builds used the
same initial state and rendered identical rows. Their timings are medians
of three runs with seed 1, with all 27 rows verified against an independent
model. CENTOS scheduling can vary between bytecode BASIC runs even with the
same seed. The original source uses its own random initial state and cursor
commands, so the table compares complete implementations with rendering.
The distributed BASIC program builds each row in `L$` and prints it in one statement.
The C implementation evolves packed bitsets using lookup tables.
[C implementation](../image/programs/life1d/) · [BASIC source](../image/programs/life1d/life1d.bas)

The C build used `-O1 --lto --gc-sections -DLIFE_SEED=1`.

### CENTOS scheduling in the bytecode BASIC benchmark

The bytecode `LIFE1D` benchmark times the interval from the printed title
through the final board and checks all 27 rows. With ordinary scheduling,
the baseline and `28ba3f7` builds took medians of 34,504,742 and
37,049,195 microcycles.
At some clock phases, CENTOS restores the task's mappings a second time
when handling a checkpoint. With matched clock phases, the optimized
image used about 0.11% fewer microcycles. [Life regression](../tests/basic-life.js)

## BASIC benchmarks

The Rugg/Feldman suite covers loops, arithmetic, subroutines, arrays and
transcendental functions. The Ahl benchmark exercises square root, square
and random-number generation. The CPU6 timings below were verified in
three fresh sessions on the native microcoded emulator with the image's
Stefan BASIC module `MEB8E48A1`.
They use the default real variables and count microsteps between two markers
after bytecode compilation. The benchmark validates the final values after the end marker.
The C64 column gives published timings from *Practical Computing* and
*Creative Computing*. [Practical Computing, January 1984](https://www.worldradiohistory.com/UK/Practical-Computing/80s/Practical-Computing-1984-01-S-OCR.pdf) · [Creative Computing, November 1983](https://archive.org/stream/creativecomputing-1983-11/Creative_Computing_v09_n11_1983_November_djvu.txt)

| Workload | Centurion BASIC at 5 MHz | Commodore 64 BASIC |
| --- | ---: | ---: |
| BM1 | 0.64 s | 1.4 s |
| BM2 | 2.38 s | 10.5 s |
| BM3 | 7.54 s | 19.2 s |
| BM4 | 8.65 s | 20.0 s |
| BM5 | 9.42 s | 21.0 s |
| BM6 | 15.75 s | 32.2 s |
| BM7 | 23.34 s | 51.6 s |
| BM8 | 66.05 s | 116.0 s |
| Ahl | 24.38 s | 113 s |

Across these workloads, the port is between 1.76 and 4.64 times as fast
as the published C64 reference. BM8 remains the longest workload in the
suite. The CPU6 run uses IEEE binary32 and the C64 figures describe its
original BASIC arithmetic. [Application measurements](#application-and-libc-update)

```sh
make bench-basic BASIC_BENCHMARKS=all
make bench-basic BASIC_BENCHMARKS=AHL
node tests/basic-life.js build/out/image/centos-apps.img 3
```

## CPU6 instruction costs

Each candidate instruction sequence has an encoded size in bytes and an
execution time in CPU6 microcycles. The assembler chooses the encoding
from the operands. For example, `xfr a,b` occupies one byte and takes eight
microcycles, while `xfr b,a` occupies two and takes fifteen. The comparisons
below use normal instruction fetch from RAM with no interrupt or DMA service.
[EE200 instruction encoder](../swext/fuzix-bintools/as1-ee200.c)

### Reproduce the measurements

Import the firmware, then build the JavaScript emulator and run the probe.
[Build instructions](../README.md#build)

```sh
make emulator
node tests/cpu6-instruction-costs.js build/out/cpu6-instruction-costs.tsv
```

The TSV records each encoding, disassembled operands, input conditions,
encoded size, microcycles and final PC or branch outcome. The probe resets
the microcoded CPU for each trial, prepares its registers and RAM, and
counts steps between ordinary instruction boundaries. It checks instruction
lengths against the next PC for sequential instructions and exercises both
outcomes of all 16 conditional branches. A cross-check on the native
microcoded core reproduced 57 sequential instruction timings. The separate
C instruction emulator has partial timing estimates. [Probe](../tests/cpu6-instruction-costs.js)
· [C instruction emulator](../swext/centurion-c/cpu6.c)

### Primary opcode table

The row gives the high hex digit and the column the low digit. Each entry is
the observed minimum and maximum over the sampled operands. Actual latency
can also depend on operand values, the block length, and hardware or MMU
state. `00` halts. The seven remaining dashes are reserved in the bundled
disassembler. The probe uses the assembler's `RIM` mnemonic for `0B`.
[Disassembler](../swext/centurion-js/src/cen.ts)
· [Assembler mnemonics](../swext/fuzix-bintools/as6-ee200.c)

| High / low | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | — | 4 | 5 | 5 | 8 | 8 | 5 | 5 | 5 | 21 | 30 | 43 | 6 | 9 | 22725 | 42 |
| 1 | 9–16 | 10–17 | 9–16 | 10–17 | 9–16 | 10–17 | 9–16 | 10–17 | 10–17 | 9–16 | 10–17 | 10–17 | 10–17 | 10–17 | 9–16 | 10–17 |
| 2 | 12 | 12 | 11 | 12 | 14–44 | 14–62 | 18–63 | 18–108 | 6 | 6 | 5 | 6 | 9 | 6 | 67–340 | 14–21 |
| 3 | 13–35 | 13–35 | 12–34 | 13–36 | 17–85 | 20–118 | 21–104 | 22–150 | 7 | 7 | 6 | 7 | 10 | 8 | 10 | 10 |
| 4 | 11 | 11 | 11 | 11 | 11 | 11 | 80–34513 | 72–735 | 8 | 8 | 8 | 7 | 7 | 7 | 7 | 7 |
| 5 | 15–29 | 15–29 | 15–29 | 15–29 | 15–29 | 15–29 | 5 | 5 | 9 | 9 | 9 | 8 | 8 | 8 | 8 | 8 |
| 6 | 13 | 22 | 32 | 23 | 33 | 23–38 | 59 | 69–735 | 15 | 23 | 33 | 24 | 34 | 24–39 | 19 | 18 |
| 7 | — | 13 | 23 | 14 | 24 | 14–29 | 8 | 111–124 | 18–183 | 34 | 44 | 35 | 45 | 35–50 | 25–115 | 22–97 |
| 8 | 8 | 18 | 28 | 19 | 29 | 19–34 | 8 | — | 13 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| 9 | 12 | 22 | 32 | 23 | 33 | 23–38 | 8 | — | 17 | 19 | 19 | 19 | 19 | 19 | 19 | 19 |
| A | 10 | 18 | 28 | 19 | 29 | 19–34 | 8 | — | 13 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| B | 14 | 22 | 32 | 23 | 33 | 23–38 | 10 | — | 17 | 19 | 19 | 19 | 19 | 19 | 19 | 19 |
| C | 8 | 18 | 28 | 19 | 29 | 19–34 | 10 | — | 13 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| D | 12 | 22 | 32 | 23 | 33 | 23–38 | 13–28 | 13 | 17 | 19 | 19 | 19 | 19 | 19 | 19 | 19 |
| E | 11 | 18 | 28 | 19 | 29 | 19–34 | 16 | — | 13 | 15 | 15 | 15 | 15 | 15 | 15 | 15 |
| F | 15 | 22 | 32 | 23 | 33 | 23–38 | 35–51 | 41–2336 | 17 | 19 | 19 | 19 | 19 | 19 | 19 | 19 |

The 11,126 trials cover all primary opcode bytes, the valid base registers,
displacement, indirection and update fields for ordinary indexed instructions,
all register descriptors for the byte and word ALU, and each assembler-legal
push/pop range. The extended page, DMA, block and multiple-precision families
use their named suboperations. The block and multiple-precision samples use
representative address and length combinations. The TSV marks the control
transfers seen for page suboperation 5. The 18-cycle DIV observation is
the zero-divisor path. The `F7` samples use A = 1, 2, 16 and 256.

### Costs that affect code generation

| SDCC assembler form | Bytes | Microcycles | Decision |
| --- | ---: | ---: | --- |
| `add a,b` (AAB) / `add b,b` | 1 / 2 | 9 / 15 | Reuse the implicit operand pair when it already holds the values. |
| `sla` / `slr b,1` | 1 / 2 | 8 / 20 | Use the A accumulator form for a one-bit shift when A holds the value. |
| `inr b,1` through `inr b,16` | 2 | 13 | A single INR covers the whole literal range at the same cost. |
| `inr (address)` / `inr 2(x)` | 4 / 4 | 34 / 35 | Consider an in-memory update when the register result is dead. |
| `lda (a)` / `lda (x)` / `lda 2(x)` | 1 / 1 / 3 | 17 / 19 / 27 | Both base choice and displacement affect the cost. |
| `lda (x+)` / `lda (-x)` | 2 / 2 | 25 / 24 | Combine the load with an existing pointer update of the correct width. |
| `lda (x)` followed by `xfr a,y` / `ldw y,(x)` | 2 / 4 | 27 / 29 | The compact pair wins when clobbering A is allowed. |
| `lda (x)` followed by AAB / `add 0(x),b` | 2 / 4 | 28 / 29 | The fold becomes worthwhile with displacement or a different register assignment. |
| `bz` fall-through / taken | 2 | 9 / 16 | Account for the direction of the frequent path. |
| `bnz` fall-through / taken | 2 | 10 / 17 | Inverting a branch changes both paths' costs. |
| `jmp label` / `bra label` | 3 / 2 | 13 / 14 | Relative relaxation trades one cycle for one byte. |
| `jsr label` / `bsr label` / `rsr` | 3 / 2 / 1 | 34 / 35 / 21 | Include return and register saves when comparing call sequences. |
| `push yh,zl` / `pop yh,zl` | 2 / 2 | 43 / 37 | Saving contiguous Y and Z shares the range instruction's fixed cost. |
| `mul 3,b` | 4 | 116 | A replacement shift/add chain must also fit the four-byte budget if size must be preserved. |

The backend's `cpu6instructionSize` estimates branch spans and uses a
conservative upper bound for unknown assembly. The switch dispatch costs in
`cpu6_port.jumptableCost` are byte estimates for full sequences. For a
word already loadable into A, multiplication by 2ⁿ ± 1 can use XAB, a
shift and AAB or SAB. The four-byte sequence takes at most 107 microcycles
for the supported shifts, including 8 for XAB and 9 for AAB or SAB.
Immediate MUL takes 116 microcycles and occupies four bytes.
[Branch relaxation](../swext/sdcc/sdcc/src/cpu6/peep.c)
· [Port costs](../swext/sdcc/sdcc/src/cpu6/main.c)
· [Instruction selection](../swext/sdcc/sdcc/src/cpu6/gen.c)

### Encoding and branch-span checks

The survey exercises each named suboperation in the disassembler's page
family (`2E`, selectors 0–5), DMA family (`2F`, 0–9), multi-precision
family (`46`, 0–9) and both block families (`47` and `67`, 0–9). It
includes all four word/byte and read/write selectors of `F6`. The TSV
includes a sample of each inter-level register transfer (`D7`, `E6`).
The suboperation names describe encodings. Privileged and I/O timings
refer to the specific reset configuration used by the probe.

A separate check compared the backend's span estimates with 215,573
assembled instruction lines from the BASIC build, wide-arithmetic tests,
sieve and targeted assembly fixtures. Every estimate covered the actual
encoded length. This includes relaxable `bra`, spaced autoindex operands and
assembler symbols such as `cf` that are also internal peephole flag names.

For a relative target outside the signed displacement range, the bundled
assembler expands `bra` to the three-byte absolute jump. Its `bsr`
pseudo-instruction also expands to opcode `71` (JMP), which loses the
return link. SDCC emits `jsr` for calls to distant functions. Handwritten
assembly should likewise use `jsr` for those calls.
[Assembler branch relaxation](../swext/fuzix-bintools/as1-ee200.c)
