# CENTOS application image

From the distribution root, run either command to build the SDK, the
applications and the disk image.

```sh
./dobuild -j2 JOBS=4 SEED=/path/to/CENTOS_13.IMG
make all SEED=/path/to/CENTOS_13.IMG
```

`make -C image` also builds the complete image. The compiler-only build is
`make sdk`. The image uses the distribution's SDCC and the sources under
`image/programs/`, `tests/` and the pinned submodules.

Provide a local 512-byte-stride CENTOS 13 Hawk image that you have
permission to use. The build accepts it through `SEED` or at
`build/out/image/CENTOS_13.IMG`. See the
[Centurion archive notice](https://github.com/Nakazoto/CenturionComputer/wiki/Software#operating-system)
for information on access to operating system images. Generated images
live under `build/out/image/`. Intermediate
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
| `BASIC` | Start Stefan BASIC. Use `.LOAD "LIFE1D"`, then `.RUN`, for the integer Life example |
| `.RUN SDCC.XSIEVE` | SDCC-compiled prime sieve |
| `.RUN SDCC.XCONNECT4` | Connect Four |
| `.RUN SDCC.XCHOMP` | Chomp against the computer on boards from 1 × 1 through 9 × 9 |
| `.RUN SDCC.XCHESS` | Chess |
| `.RUN SDCC.XLIFE1D` | One-dimensional Life in C |
| `.RUN SDCC.XKERMIT` | E-Kermit receiver for CRT1 |
| `RUNTESTS` | Run the 44 CENTOS executables from the main CPU6 compiler regression suite |

The `SBASIC` library on disk 1 holds the BASIC job, its executable, modules
and examples. The root `BASIC` command starts `SBASIC.BASIC`. Its job checks
the native monitor entry and loads the module when it is absent. Use `BASIC`
again after `.BYE` to return to the REPL. Use `.DIR SBASIC. 1` at the CENTOS
prompt to browse the library. BASIC reads and saves source files there.

In BASIC, `.LOAD "BDEMO"` followed by `.RUN` demonstrates real arithmetic,
mixed numeric and string data, and nested `WHILE` and `FOR` loops.

Use `.DIR SDCC. 1` to browse the SDCC library. It contains the application
executables and the CENTOS regression tests `T01O0` through `T22O1`.
Each build records the mapping from qualified names
to C cases in `tests.md`. The device-independent tools under `tests/` run
the raw CPU6 binaries separately on the C and JavaScript cores.

The disk builder checks BASIC, runs XSIEVE and checks the test job output
before publishing the image. It plays Chomp, Connect Four and chess under CENTOS
and checks the resulting boards through the emulator's VT100 screen model.
Chess updates only the squares changed by a move, including captures,
en passant and castling. Run `make tests-games` to repeat the game checks.

`LIFE1D` buffers each complete row in `L$` and prints it once to reduce the
BASIC overhead of displaying the cells. The image build compares all 27 rows
against an independent Life implementation.

To measure the complete LIFE1D program over three runs, use this command.

```sh
node tests/basic-life.js build/out/image/centos-apps.img 3
```

The measurement covers execution and output from the first Life banner
through the last of the 27 rows. It excludes loading and compilation.
