# CPU6 regression tests

Run the Make targets from the distribution root or with `make -C tests`.

| Target | Coverage |
| --- | --- |
| `make check` | Quick check of SDCC, the JavaScript emulator and CENTOS executable generation |
| `make tests` | Quick check plus 22 regression cases built at `-O0` and `-O1`. All 44 binaries run on the C instruction emulator and the JavaScript microcoded emulator. A compile-only case checks the removal of an unreachable indirect store |
| `make alltests` | The main regression plus the generated compiler audit and extended tests for integer arithmetic, float32, ABI, inline assembly, optimizer passes, assembler and linker |
| `make tests-web` | Exercise the HTTP and WebSocket console, CPU controls, image round trip and server shutdown |
| `make tests-centos SEED=/path/to/CENTOS_13.IMG` | Install and run a compiled program in a new CENTOS disk copy and confirm return to the shell |
| `make tests-games` | Play Connect Four and chess from the generated application image and check the VT100 screen after moves. Set `APP_IMAGE` to test a different image |

The web test uses the real HTTP and WebSocket parsers over in-memory
connections. If the application image is available, it also runs the
shipped console script, boots CENTOS, creates a guest file and checks that
the file survives saving and reloading the image. Set
`WEB_SEED=/path/to/CENTOS_13.IMG` to exercise another image, or `WEB_SEED=`
to run the tests that start without a bootable disk.

The compiler suites run directly in emulated memory. `test-c-emulator`
builds the C instruction emulator from its pinned submodule for the tests.
It runs raw test programs without boot or diagnostic ROM files.

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
