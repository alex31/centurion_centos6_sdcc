# CPU6 regression tests

Run the Make targets from the distribution root or with `make -C tests`.
Tests that run the JavaScript core use the
[local firmware pack](../README.md#build).

| Target | Coverage |
| --- | --- |
| `make check` | Quick check of SDCC, the JavaScript emulator and CENTOS executable generation |
| `make tests` | Quick check plus 22 regression cases built at `-O0` and `-O1`. All 44 binaries run on the C instruction emulator and the JavaScript microcoded emulator. A compile-only case checks the removal of an unreachable indirect store |
| `make alltests` | The main regression plus the generated compiler audit and extended tests for integer arithmetic, float32, ABI, inline assembly, optimizer passes, assembler and linker |
| `make tests-web` | Exercise the HTTP and WebSocket console, CPU controls, image round trip and server shutdown |
| `make tests-centos SEED=/path/to/CENTOS_13.IMG` | Install and run a compiled program in a new CENTOS disk copy and confirm return to the shell |
| `make tests-games` | Play Connect Four and chess from the generated application image and check the VT100 screen after moves. Set `APP_IMAGE` to test a different image |
| `make bench-basic` | Run the Rugg/Feldman BM1 and BM2 BASIC benchmarks at the nominal 5 MHz CPU6 rate. Set `APP_IMAGE` to test a different image |

The BASIC benchmark counts CPU6 microcycles between the start and end
markers, excluding bytecode compilation. It uses the default floating-point
variables and checks the final values. Set `BASIC_BENCHMARKS=all` for the
eight tests or give a comma-separated selection. BM6 and BM7 put the
`DIM` declaration in the header required by this BASIC dialect. Benchmark
programs and reference Commodore 64 timings follow [Practical Computing,
January 1984](https://www.worldradiohistory.com/UK/Practical-Computing/80s/Practical-Computing-1984-01-S-OCR.pdf).
Use `BASIC_BENCHMARKS=AHL` for the square-root, square and random-number
workload from [Creative Computing, November 1983](https://archive.org/stream/creativecomputing-1983-11/Creative_Computing_v09_n11_1983_November_djvu.txt).
The test uses the article's `RND(1)` variant. The published Commodore 64
time was 1 minute 53 seconds.

The web test uses the real HTTP and WebSocket parsers over in-memory
connections. If the application image is available, it also runs the
shipped console script, boots CENTOS, creates a guest file and checks that
the file survives saving and reloading the image. Set
`WEB_SEED=/path/to/CENTOS_13.IMG` to exercise another image, or `WEB_SEED=`
to run the tests that start without a bootable disk.

The compiler suites run directly in emulated memory. `test-c-emulator`
builds the C instruction emulator from its pinned submodule for the tests.
It runs raw test programs without boot or diagnostic ROM files.

The extended C language suite runs at `-O0`, `-O1` and `-Os` on both cores.
It executes the reduced BASIC reproducer, checks indirect stores through
aliases and copies of odd-sized structures, and runs selected upstream
SDCC structure, union and bit-field tests. The CPU6 bit-field cases also
verify sign extension, `_Bool` conversion, initialization, indexed
members and preservation of neighboring bits in volatile objects.

Test artifacts and logs live under `build/out/tests/`. The extended runner
keeps one log per suite, runs remaining suites after a failure and returns a
nonzero exit status if any fail. Set `TEST_JOBS` to choose the number of
concurrent extended suites. For example:

```sh
make -j3 JOBS=6 TEST_JOBS=4 alltests
```

After building the tools with `make tests`, run an individual suite directly:

```sh
python3 tests/sdcc-inline-asm.py
python3 tests/sdcc-lto.py
```

Some host-side checks use AddressSanitizer and UndefinedBehaviorSanitizer. If
LeakSanitizer reports that it cannot run under ptrace, use
`ASAN_OPTIONS=detect_leaks=0 make alltests` to keep the address and
undefined-behavior checks enabled.

This suite covers the Centurion CPU6 port. Device-specific tests for other
targets belong to SDCC's upstream suite.
