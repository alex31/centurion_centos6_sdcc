# BASIC for CENTOS

This example combines C and CPU6 assembly in a CENTOS application based
on Stefan's BASIC. Its performance goal is to be competitive with BASIC
on 1980s 8-bit microcomputers. The distribution includes two BASIC examples
and a C version of the one-dimensional Life example for comparison.

## Start the REPL

Boot the [application image](../README.md#start-a-centos-session) in either the native or the JavaScript emulator
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

To add BASIC, its examples and the 24 BASIC games to a seed disk, use the same local
firmware and CENTOS seed as the full distribution build.

```sh
make basic-image SEED=/path/to/CENTOS_13.IMG
run/centurion-system-js build/out/basic/centos-basic.img
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
  concatenation and comparisons. The functions include `LEN`, `ASC`,
  `CHR$`, `LEFT$`, `RIGHT$`, `MID$`, `STR$` and `VAL`.
- Structured control flow includes nested `FOR/NEXT` and `WHILE/WEND`.
  `DIM` defines numeric arrays. `DATA`, `READ` and `RESTORE` support
  mixed numeric and string data. `RESTORE n` selects the n-th value
  starting at 1. The compiler allows eight nested loops across `FOR`
  and `WHILE`.
- `.EDIT` opens a line editor. `.LIST` pages through the program.
  `.DIR` lists numbered BASIC sources in alphabetical order, accepts
  quoted wildcard filters and pages long results. The pagers use Up
  and Down for lines, Left and Right or Page Up and Page Down for pages,
  and Q or Enter to exit.
- `.LOAD` and `.SAVE` read and write native CENTOS text in `SBASIC`.
  Newly saved programs use the image's preallocated file slots.

## 101 BASIC Computer Games

The image includes 24 tested ports from David H. Ahl's 1975 DEC collection.
Run `BASIC` from CENTOS, then use `.LOAD "G23MTCH"` and `.RUN`.
Use these names to load other ports.

| Name | Program |
| --- | --- |
| `G23MTCH` | 23 Matches |
| `G3DPLOT` | 3D Plot |
| `ACEYDU` | Acey Ducey |
| `CHANGE` | Change |
| `CHEMST` | Chemist |
| `CHIEF` | Chief |
| `CHOMP` | Chomp |
| `DIAMND` | Diamond |
| `DICE` | Dice |
| `EVEN` | Even Wins |
| `HILO` | Hi-Lo |
| `HMRABI` | Hammurabi |
| `HURKLE` | Hurkle |
| `KINEMA` | Kinema |
| `LETTER` | Letter |
| `LITQZ` | Literature Quiz |
| `MATHD` | Math Dice |
| `NICOMA` | Nicomachus |
| `NUMBER` | Number |
| `ROCKSP` | Rock, Scissors, Paper |
| `RUSROU` | Russian Roulette |
| `STARS` | Stars |
| `TRAIN` | Train |
| `TRAP` | Trap |

Chomp, Hurkle and Trap take the two numbers at separate prompts. Chief, Diamond
and 3D Plot draw with printable characters and spaces on the VT100. Use
Ctrl+C to return to CENTOS from games that keep offering another round.

Chomp and Diamond use `DEFINT` for bounded indices and loop counters.
The comparison runs produced identical terminal output with the same
inputs, including retries after invalid values.

Use `.DIR` in BASIC for the installed programs. The upstream submodule
holds the original listings for further porting.


## Memory and execution

The build reserves 5,632 bytes for tokenized program text, the line index,
compiled bytecode and numeric arrays. Strings use a separate 4 KiB heap.
The CPU stack is 1,536 bytes and the CENTOS mapped module has a 20 KiB
budget. The REPL and editor share an overlay. The compiler and VM have
their own overlays, and variables stay resident. The assembly dispatcher
keeps instruction and value-stack pointers in registers and handles
common bytecode operations directly. [Memory placement and overlays](mmu.md)

Use `make basic` to build the executable, mapped module and overlays
without installing them into a disk image. Use `make basic-image` for
the installation and boot test. [BASIC benchmarks](performance.md#basic-benchmarks)
