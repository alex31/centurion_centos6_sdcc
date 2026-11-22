# BASIC for CENTOS

This example combines C and CPU6 assembly in a CENTOS application based
on Stefan's BASIC. Its performance goal is to be competitive with BASIC
on 1980s 8-bit microcomputers. The distribution includes two BASIC examples
and a C version of the one-dimensional Life example for comparison.

## Start the REPL

Boot the [application image](../image/README.md) in the JavaScript emulator
and enter `BASIC` at the CENTOS prompt. This starts the job in the
`SBASIC` library and loads the native module when required. After `.BYE`,
enter `BASIC` again to reopen the REPL.

```text
.LOAD "BDEMO"
.RUN
.LOAD "LIFE1D"
.RUN
```

`BDEMO` illustrates real arithmetic, mixed data and control flow.
`LIFE1D` runs 26 generations of one-dimensional Life after displaying its
initial state.

To build a disk with just BASIC and its examples, use the same local
firmware and CENTOS seed as the full distribution build.

```sh
make basic-image SEED=/path/to/CENTOS_13.IMG
run/centurion-system build/out/basic/centos-basic.img
```

## Language and console

- The REPL executes direct commands. Numbered program lines remain
  editable and are compiled to typed bytecode on `.RUN`.
- Numeric variables default to single-precision real values. A `DEFINT`
  header selects signed 16-bit integers. Two integer operands use integer
  arithmetic for `+`, `-`, `*` and `/`. Division truncates toward
  zero. A real operand promotes the operation to real arithmetic. Under
  `DEFINT`, whole-valued literals in range, including `10.0`, are typed
  as integers.
- String variables use a `$` suffix and support up to 255 characters
  per string within a shared 4 KiB string store. The console supports
  concatenation, comparisons and common string functions.
- Structured control flow includes nested `FOR/NEXT` and `WHILE/WEND`.
  `DIM` defines numeric arrays. `DATA`, `READ` and `RESTORE` support
  mixed numeric and string data.
- `.EDIT` opens a line editor. `.LIST` pages through the program.
  `.DIR` lists numbered BASIC sources in alphabetical order, accepts
  quoted wildcard filters and pages long results. The pagers use Up
  and Down for lines, Left and Right or Page Up and Page Down for pages,
  and Q or Enter to exit.
- `.LOAD` and `.SAVE` read and write native CENTOS text in `SBASIC`.
  Newly saved programs use the image's preallocated file slots.

## 101 BASIC Computer Games

The image includes 11 ports from David H. Ahl's 1975 DEC collection.
Run `BASIC` from CENTOS, then use `.LOAD "G23MTCH"` and `.RUN`.
Use these names to load other ports.

| Name | Program |
| --- | --- |
| `G23MTCH` | 23 Matches |
| `CHEMST` | Chemist |
| `CHIEF` | Chief |
| `DICE` | Dice |
| `HILO` | Hi-Lo |
| `HURKLE` | Hurkle |
| `LETTER` | Letter |
| `NICOMA` | Nicomachus |
| `ROCKSP` | Rock, Scissors, Paper |
| `STARS` | Stars |
| `TRAIN` | Train |

Hurkle takes the two coordinates at separate prompts. Chief draws its
picture with spaces and printable characters on the VT100. Use Ctrl+C
to return to CENTOS from games that keep offering another round.

At the CENTOS prompt, `.DIR BAS101. 1` lists all 108 original source
files. Press Enter at each `(MORE)` prompt. CENTOS filenames use
uppercase, drop hyphens, and add a leading
`G` to names starting with a digit. The original listings use several
BASIC dialects, including instructions and array layouts outside the
Stefan BASIC port's current support. BUNNY and SNOOPY appear in the book
but have no listings in this archive. [Upstream catalog and transcription notes](https://github.com/maurymarkowitz/101-BASIC-Computer-Games)

## Performance

### One-dimensional Life

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
| Distributed integer bytecode BASIC | 39,297,964 | 7.86 s |
| Distributed C compiled by SDCC | 9,893,516 | 1.98 s |

The historical row reports one full emulator run, with generation times
between 28.79 and 29.45 s. The distributed C and BASIC programs use the
same initial state and render identical rows. Their timings are medians
of three runs with seed 1, with all 27 rows verified against an independent
model. The original source uses its own random initial state and cursor
commands, so the table compares complete implementations with rendering.
The distributed BASIC program builds each row in `L$` and prints it in one statement.
The C implementation evolves packed bitsets using lookup tables.
[C implementation and measurements](sdcc.md#one-dimensional-life) · [BASIC source](../image/programs/life1d/life1d.bas)

In an earlier BASIC development comparison with the same rule, seed and
generation count, the integer kernel fell from 59.11 to 18.71 million
microsteps, a 68% reduction. That test measured just the generation loop.
The distribution's Life regression measures from the banner to the last
row and excludes loading and compilation. [Port measurements](../swext/stefan-basic/CENTURION.md) · [Life regression](../image/README.md)

### Real arithmetic and 8-bit BASIC references

The Rugg/Feldman suite covers loops, arithmetic, subroutines, arrays and
transcendental functions. The Ahl benchmark exercises square root, square
and random-number generation. The CPU6 timings use the default real
variables and count microsteps between two markers, after bytecode
compilation. The benchmark validates the final values after the end marker.
The C64 column gives published timings from *Practical Computing* and
*Creative Computing*. [Benchmark sources and references](../tests/README.md#cpu6-regression-tests)

| Workload | Centurion BASIC at 5 MHz | Commodore 64 BASIC |
| --- | ---: | ---: |
| BM1 | 1.25 s | 1.4 s |
| BM2 | 4.63 s | 10.5 s |
| BM3 | 13.56 s | 19.2 s |
| BM4 | 15.31 s | 20.0 s |
| BM5 | 16.22 s | 21.0 s |
| BM6 | 29.69 s | 32.2 s |
| BM7 | 38.75 s | 51.6 s |
| BM8 | 107.99 s | 116.0 s |
| Ahl | 36.70 s | 113 s |

Across these workloads, the port is between 1.07 and 3.08 times as fast
as the published C64 reference. BM8 remains the longest workload in the
suite. The CPU6 run uses IEEE binary32 and the C64 figures describe its
original BASIC arithmetic.

```sh
make bench-basic BASIC_BENCHMARKS=all
make bench-basic BASIC_BENCHMARKS=AHL
node tests/basic-life.js build/out/image/centos-apps.img 3
```

## Implementation

The distributed image reserves 5,632 bytes (5.5 KiB) for the tokenized
program, its line index, compiled bytecode and numeric arrays. The string
heap has a separate 4 KiB reservation. The CENTOS build keeps the REPL,
editor, compiler, VM, text operations and mathematical routines in code
overlays and a mapped module. The CPU6 dispatch loop has assembly fast paths for
common bytecode operations. See
[the Centurion BASIC port notes](../swext/stefan-basic/CENTURION.md) for
the supported features and memory organization.
