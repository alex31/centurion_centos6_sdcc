# SDCC CPU6 performance, November 2026

The comparison starts at distribution revision `933f46c` and SDCC revision
`e69c8e6a7`. Both builds use the same application sources, compiler flags,
CENTOS seed and CPU6 microcode. The optimized SDCC revision is `abf0207e3`.

## Instruction selection

GCC 16 uses target-specific costs for instructions and addressing modes.
Its RTL forward-propagation and late-combine passes substitute a definition
into its consumers when the combined operation is profitable. For the CPU6,
the relevant costs are encoded bytes and microcycles for the actual register
and addressing mode. For example, the compiler selects a shift and AAB/SAB
for suitable constant multiplications when the sequence fits the four bytes
of immediate MUL. [GCC 16 cost hooks](https://gcc.gnu.org/onlinedocs/gcc-16.1.0/gccint/Costs.html)
· [GCC 16 RTL passes](https://gcc.gnu.org/onlinedocs/gcc-16.1.0/gccint/RTL-passes.html)
· [CPU6 instruction cost table](cpu6-instruction-costs.md)

LLVM's instruction folding lets an instruction consume a memory operand
directly and removes the intervening copy. The CPU6 backend uses this
approach for two-word operations, pointer loads and argument setup.
It keeps 32-bit intermediates in A:B when possible. The peephole pass
checks which register halves and condition flags the next instructions
need. It also treats inline assembly as a boundary. In size mode the
link-time inliner budgets the expanded instruction bytes, including any
save and restore of X. [LLVM code generator](https://releases.llvm.org/23.1.0/docs/CodeGenerator.html#instruction-folding)
· [Clang/LLVM 23.1.6 release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.6)

For a constant-byte fill of a known ordinary array, SDCC can store a seed
byte and use a forward overlapping CPU6 `CPY` to propagate it. This cuts the
sieve's initialization loop while preserving each final byte. The SDCC
binary32 library also uses native register pairs for addition, rounding,
normalization and ordered comparisons. The portable C routines provide a
separate implementation for host-side differential checks.

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
scheduler-phase experiment is described in the Life guide.

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
[Benchmark definitions](../tests/basic-benchmark.js) · [Life timing and scheduler controls](sdcc.md#centos-scheduling-in-the-bytecode-basic-benchmark)

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
