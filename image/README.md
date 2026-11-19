# CENTOS application image

From the distribution root, run either command to build the SDK, the
applications and the disk image.

```sh
./dobuild -j2 JOBS=4
make all
```

`make -C image` also builds the complete image. The compiler-only build is
`make sdk`. The image uses the distribution's SDCC and the sources under
`image/programs/`, `tests/` and the pinned submodules.

The default seed is the [archived CENTOS 13 Hawk image](https://github.com/Nakazoto/CenturionComputer/blob/8682b5865979315cf9e085e75ff8547ff3be473e/Software/Data%20Packs/CENTOS_13.IMG). The first complete
build downloads that revision and verifies its SHA-256. To use a local
512-byte-stride CENTOS seed, set `SEED=/path/to/CENTOS_13.IMG`. The seed
and generated output images live under `build/out/image/`. Intermediate
files, executable maps and build reports are grouped by run under
`build/out/image/work/`.

The installer follows the emulated terminal's receive status and waits for
CENTOS to complete each allocation. It checks every file's type and allocated
space in the new disk before writing the executable or text data.

After the build, run the emulator from the distribution root and open
the [web console](http://127.0.0.1:42646/).

```sh
run/centurion-system build/out/image/centos-apps.img
```

`run/centurion-system` also selects the latest generated application image
automatically when called without an image argument. At `D=`, click
**Boot CENTOS**. On the standard ROM this prompt is on CRT0.

| CENTOS command | Program |
| --- | --- |
| `STARTBASIC` | Start Stefan BASIC. Use `.LOAD "LIFEINT"` or `.LOAD "LIFELINE"`, then `.RUN`, for the two integer Life examples |
| `.RUN XSIEVE` | SDCC-compiled prime sieve |
| `.RUN XCONNECT4` | Connect Four |
| `.RUN XCHESS` | Chess |
| `.RUN XLIFE1D` | One-dimensional Life in C |
| `.RUN XKERMIT` | E-Kermit receiver for CRT1 |
| `RUNTESTS` | Run the 44 CENTOS executables from the main CPU6 compiler regression suite |

In BASIC, `.LOAD "BDEMO"` followed by `.RUN` demonstrates real arithmetic,
mixed numeric and string data, and nested `WHILE` and `FOR` loops.

The application image contains the CENTOS builds of the regression tests
named `T01O0` through `T22O1`. Each run records the mapping from file names
to C cases in `tests.md`. The device-independent tools under `tests/` run
the raw CPU6 binaries separately on the C and JavaScript cores.

The disk builder checks BASIC, runs XSIEVE and checks the test job output
before publishing the image. It plays Connect Four and chess under CENTOS
and checks the resulting boards through the emulator's VT100 screen model.
Chess updates only the squares changed by a move, including captures,
en passant and castling. Run `make tests-games` to repeat the game checks.

`LIFEINT` prints each cell with a BASIC statement. `LIFELINE` builds each
complete row in `L$` and prints it once. The image build compares all 27
rows from both programs against an independent Life implementation.

To time both complete LIFE programs on the same disk over three runs, use this command.

```sh
node tests/basic-life.js build/out/image/centos-apps.img 3
```

The measurement covers execution and output from the first Life banner
through the last of the 27 rows. It excludes loading and compilation.
