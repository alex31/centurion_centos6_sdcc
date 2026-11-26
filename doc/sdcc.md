# SDCC for CENTOS and bare metal

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
the compiler regression job. [Application image contents](../README.md#centos-applications)

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
[MMU placement](mmu.md) and [overlay support](mmu.md#c-overlays) for programs that
need banked memory or separately loaded code.

## Bare metal

`run/cpu6-bare-sdcc` compiles and links C and EE200 assembly into a raw
`.bin`. By default the first byte belongs at address `0x2000`, which is also
the entry point of the bundled startup code.

### Build and run a smoke test

```sh
make sdk
mkdir -p build/out/bare
run/cpu6-bare-sdcc -O1 --gc-sections \
    tests/smoke.c --map build/out/bare/smoke.map -o build/out/bare/smoke.bin
node tests/compiler-audit-core.js build/out/bare tests/smoke.json
```

The last command loads the raw program into emulated memory, sets the
program counter and checks the result. It uses the local firmware pack
described in [Build](../README.md#build).

The `--origin` option sets the hexadecimal link address. Use the same
address when loading the binary. For the test runner, supply it through
`CPU6_BARE_ORIGIN`. For example:

```sh
run/cpu6-bare-sdcc --origin 3000 tests/smoke.c -o build/out/bare/smoke.bin
CPU6_BARE_ORIGIN=3000 node tests/compiler-audit-core.js build/out/bare tests/smoke.json
```

The supplied startup sets up the stack, clears BSS and initializes MUX
channel 0 for the polled console. The linker checks the program against
the diagnostic ROM boundary at `0x8000`. The driver accepts separately
compiled objects and archives, as well as C and assembly sources.
Use `--map` to inspect the memory layout.

### Access hardware

`<system_mmio.h>` provides the reverse-engineered CPU6 device addresses
and `CENTURION_MMIO8(addr)` for volatile 8-bit accesses. [MMIO definitions](../baremetal/include/system_mmio.h)

`<system_finch.h>` exposes the FFC registers and the Finch driver. The
default base is `0xF800` and can be changed with `CENTURION_FFC_BASE` at
build time. The driver initializes the device, reads and writes
400-byte sectors, and formats a track or a disk. It expects exclusive
use of the FFC and CPU DMA, with buffers in identity-mapped RAM.
The format used by the driver has 29 sectors per track.
[Finch declarations and return codes](../baremetal/include/system_finch.h)

`make tests-finch` formats a scratch track, reads and replaces a sector, and
verifies that adjacent data is preserved on the microcoded emulator.
The filesystem format and boot files are separate parts of a bootable disk.

## Memory and strings

Include `<string.h>` in bare-metal or CENTOS programs. Both drivers link
the routines on demand, one function per archive member. Including the
header alone adds no code or static data, even with `--no-gc-sections`.

| Operation | Functions |
| --- | --- |
| Raw memory | `memcpy`, `memmove`, `memset`, `memcmp`, `memchr` |
| Length, copy and append | `strlen`, `strnlen`, `strcpy`, `strncpy`, `strcat`, `strncat`, `strlcpy`, `strlcat` |
| Compare and search | `strcmp`, `strncmp`, `strchr`, `strrchr`, `strstr`, `strspn`, `strcspn`, `strpbrk` |
| Split into tokens or fields | `strtok`, `strtok_r`, `strsep` |

Use `memmove` when the memory ranges overlap. `strlcpy` and `strlcat`
take the full destination capacity and return the length they tried to
produce. A return value at least as large as the capacity indicates truncation.

```c
#include <stdio.h>
#include <string.h>

void print_tokens(void)
{
    char line[] = "LOAD,  DEMO;RUN";
    const char *separators = ", ;";
    char *token = strtok(line, separators);
    while (token) {
        puts(token);
        token = strtok(NULL, separators);
    }
}
```

This prints `LOAD`, `DEMO` and `RUN`. Tokenization edits the writable
buffer in place and skips runs of delimiters. Each call may choose a new
delimiter set. `strtok` holds one shared cursor. For nested or interleaved
parsing, declare a separate `char *cursor` per buffer and use
`strtok_r(line, separators, &cursor)`, then `strtok_r(NULL, separators, &cursor)`.

Use the BSD function `strsep(&cursor, ",")` to keep empty fields. Initialize
`cursor` to the start of a writable buffer. For `"a,,b,"`, successive calls
return `"a"`, `""`, `"b"`, `""`, then `NULL`. [BSD reference](https://man.openbsd.org/strsep.3)

## Sorting, random numbers and console input

`<stdlib.h>` provides `qsort`, `rand`, `srand` and `exit` on both
targets. `qsort` sorts in place with a bounded stack. `rand` returns
values from 0 through `RAND_MAX` (32,767). The initial sequence is the one
selected by `srand(1)`. On CENTOS, `exit(status)` flushes buffered `printf`
output and passes the low eight bits of `status` to CENTOS. On bare metal,
`exit` halts the CPU.

`strtol(text, &end, base)` parses a signed 32-bit integer and reports
where the number ends. Use `end == text` to detect missing digits and
`errno == ERANGE` to detect overflow after clearing `errno`. Check the
unparsed suffix and the application's bounds before narrowing to `int`.
`<errno.h>` declares `errno` for both targets.

`getchar()` is declared in `<stdio.h>`. On CENTOS it returns the
seven-bit characters of the edited console line, including trailing
spaces, followed by one newline. Long lines can span multiple native
records. `os6_discard_line()` consumes the rest of the current line
after a `getchar()` read. At a line boundary it returns immediately.
On bare metal, `getchar()` polls the MUX channel 0 receiver and returns one byte.

For text input with a fixed buffer, `os6_getline_checked()` returns the
line length excluding the newline. It accepts printable ASCII and
horizontal tabs delivered by the console. A line with a control
character returns `OS6_LINE_INVALID`. A line exceeding the available
space returns `OS6_LINE_TOO_LONG`. In either case the function consumes
the line and sets the destination to an empty string before the next read.

The CENTOS `<time.h>` provides `time()` and an unsigned 32-bit `time_t`
counting seconds since 1970-01-01, using the configured system clock.
CENTOS stores a two-digit year. The SDK interprets years 70–99 as
1970–1999 and years 00–69 as 2000–2069. An invalid clock reading yields
`(time_t)-1`.

## CPU6 ABI and arithmetic

The target is big-endian and uses 16-bit words. The backend uses X, Y and
Z for register allocation and supports an optional A/B calling convention
and operand-aware inline assembly. The compiler runtime uses B for word
results and `__hireg:B` for 32-bit results. For two-operand arithmetic
helpers, the left operand follows the saved X at `(S)`, high word first.
The right operand uses B or `__hireg:B`. The helper consumes the stacked
operand and preserves the return link for `RSR`. Comparison helpers
return 0 or 1 in B. [Calling convention declarations](../baremetal/include/cpu6abi.h)

C99 bit-fields support `_Bool`, `int`, `signed int` and `unsigned int`.
Plain `int` fields are signed. Fields start at the low bits of a byte and
move to the next byte when they do not fit. Multi-byte fields store the
least-significant byte first. Reads sign-extend when required. Stores
preserve neighbouring bits in volatile structures and unions.

The EE200 assembler supports the CPU6 register, immediate, direct and
indexed forms of `MUL` and `DIV`. The linker preserves big-endian word
fixups in CENTOS CBIN relocations and rescans the target archives to
resolve their dependencies. The SDK builds `libcompiler-sdcc.a` and
selects the required helpers on demand.

### Bit operations

The CPU6 compiler recognizes the GCC bit-operation builtins directly in
C expressions. Use `__builtin_bswap16(x)` and `__builtin_bswap32(x)` to
reverse bytes. The SDK also provides the spellings `bswap16(x)` and
`bswap32(x)` through `<cpu6bits.h>`.

```c
#include <stdint.h>
#include <cpu6bits.h>

uint16_t network_word(uint16_t x) { return bswap16(x); }
uint32_t rotate(uint32_t x, unsigned n) { return __builtin_stdc_rotate_left(x, n); }
int first_set(uint16_t x) { return __builtin_ctzg(x, -1); }
```

| Builtin family | CPU6 behavior |
| --- | --- |
| `__builtin_` followed by `ffs`, `clz`, `ctz`, `clrsb`, `popcount` or `parity` | The unsuffixed form takes a 16-bit int or unsigned int. The suffixes `l` and `ll` select 32 and 64 bits. All return `int`. |
| The same six names with suffix `g` | Use the precision of the argument before integer promotion. `ffsg` and `clrsbg` use a signed integer. The other four use an unsigned integer. |
| `__builtin_stdc_*` | Provides `bit_ceil`, `bit_floor`, `bit_width`, `count_ones`, `count_zeros`, `has_single_bit`, `leading_ones`, `leading_zeros`, `trailing_ones`, `trailing_zeros`, all four `first_leading_*` / `first_trailing_*` positions, and `rotate_left` / `rotate_right`. |

For a zero input, `ffs` and the four first-bit operations return zero (bit
positions start at one). The classic `clz` and `ctz` leave the result
undefined. Supply an `int` fallback to `clzg` or `ctzg` to choose that
result. The `stdc_leading_zeros` and `stdc_trailing_zeros` builtins return
the operand width on zero.
Rotate counts must be nonnegative and are reduced modulo that width.
The generic operations respect the declared width of integer bit-fields
and also accept the port's 48-bit scalar types with
`--native-int48`. The 64-bit builtin ABI passes `long long` values on the
stack and returns wide results in `__cpu6_hi64` (top two words), `__hireg`
and B. General-purpose 64-bit arithmetic remains limited on this target.
[GCC bit-operation semantics](https://gcc.gnu.org/onlinedocs/gcc/Bit-Operation-Builtins.html)

Literal arguments fold at compile time. Byte swaps and constant rotations
of 8-, 16- and 32-bit operands use inline register operations where
applicable. Other operations can call width-specific helpers. Each helper
is a separate archive member in `libcompiler-sdcc.a`. A program that includes
the header and uses none of the builtins pays zero bytes in code, tables or
return-state storage, even with `--no-gc-sections`.

### Software binary32

Enable with `run/cpu6-bare-sdcc --float32` or `run/cpu6-os6-sdcc --float32`.
The native compiler option is `--cpu6-float32`. `<float.h>` and `<math.h>` are
in the SDK include directory. Use `1.0f`, including in comparisons and function
arguments. No double or long-double type is implemented. Default argument
promotions would require double, so passing float through `...` or an
unprototyped call is diagnosed. Ordinary typed calls and float function pointers
are supported with the CPU6 32-bit value ABI.

`libfloat32-sdcc.a` contains separate members for functions and their dependent
data. It is offered to the linker only when float32 is enabled. An integer-only
program remains byte-identical with this flag. `--gc-sections` can additionally
prune unused data inside included objects. The compiler itself diagnoses float
use without the option, rather than silently generating integer operations.

The integer core implements `+ - * /`, comparisons, 8/16/32-bit integer
conversions, gradual underflow, signed zero, infinities and quiet NaN results.
Arithmetic rounds to nearest, ties to even. Float/integer conversion truncates
toward zero in the C-defined range; out-of-range integer conversion is not a
portable API. Conversion to/from native 48-bit scalars is diagnosed; explicit
intermediate 32-bit conversions are possible where the application accepts them.
There are no 64-bit target integer operations in this implementation.

The math API supplies `sinf`, `cosf`, `tanf`, `atanf`, `expf`, `logf`, `sqrtf`,
`fabsf`, `copysignf`, `truncf`, `floorf`, `ceilf`, and classification macros.

The current library has no configurable rounding mode, floating exception
flags/traps, `errno` handling, NaN-payload preservation, decimal text conversion,
float `printf`, `powf` or complete hosted libc. These are deliberate limits of
this implementation; the format and tested arithmetic do not constitute full
IEC 60559/C Annex F conformance.

### Six-byte integer arithmetic

`int48.h` provides distinct signed and unsigned six-byte types and a pointer
API for CPU6 SDCC. Include `<int48.h>` and use the CPU6 build drivers,
which link `libint48-sdcc.a`.

```c
#include <int48.h>

uint48_t a, b, result;

void calculate(void)
{
    u48_from_u32(&a, 123456UL);
    u48_from_u32(&b, 789UL);
    u48_add(&result, &a, &b);
}
```

Without extra options the types are structures with `hi`, `mid`, and `lo`
16-bit members, in that order. Both have size 6. Functions provide construction
from 32 bits, copy, comparison, addition, subtraction, negation, multiplication,
division/remainder, shifts and decimal formatting. `u48_double` and `u48_div2`
are specialized in-place primitives; the latter returns the discarded bit.

Arithmetic functions permit exact input/output aliases. Addition,
subtraction, negation and multiplication wrap modulo 2^48, including the
signed pointer API. Signed division truncates toward zero and gives the
remainder the dividend's sign. `i48_shr` sign-extends; `u48_shr` zero-fills.
Shifts of 48 or more fill with zero or the sign bit as appropriate.

`u48_divmod` and `i48_divmod` return `INT48_OK`, `INT48_DIVZERO`,
`INT48_OVERFLOW` (signed minimum divided by -1), or `INT48_ALIAS` (identical
quotient/remainder output pointers). Either output can be null and either can
alias an input, but the two output objects must not overlap. Errors leave
outputs unchanged. Decimal output buffers need `INT48_DECIMAL_SIZE` (17) bytes.
There is no decimal-input parser or floating-point conversion in this API.

#### Native scalar form with SDCC

With `run/cpu6-os6-sdcc --native-int48` or the bare-metal equivalent, the same
header instead declares:

```c
typedef unsigned _BitInt(48) uint48_t;
typedef _BitInt(48) int48_t;
```

These are scalar types: ordinary assignments, arithmetic/bitwise operators,
comparisons, casts, function arguments and results are available. Their size
and big-endian representation match the structure form, so the pointer API
still works. Scalar values have no `.hi`, `.mid`, or `.lo` members.

```c
#include <int48.h>

uint48_t calculate(uint48_t a, uint48_t b)
{
    return (a + b) * 3U;
}
```

The option passes `--cpu6-bitint48` and defines `CPU6_NATIVE_INT48` for the
header. It is specific to the SDCC wrapper. Normal builds and FCC keep the
structure API. This is support for the selected scalar widths, not arbitrary
`_BitInt(N)` widths or full C23 conformance. Unsupported widths are rejected.
Generalized constant propagation remains enabled, but its automatic
bit-width narrowing remains disabled for CPU6 to avoid inventing unsupported
24- or 40-bit temporaries.

Native operators follow C scalar rules: unsigned arithmetic wraps; signed
overflow and shift counts outside 0..47 are not defined by C. The explicit
pointer functions retain their documented wrapping/error semantics.
Formatting uses the pointer API; `printf` has no new 48-bit format modifier.

The native scalar ABI passes six-byte stack arguments and returns the high
word in `__hireg`, middle word in `__cpu6_mid48`, and low word in B. This
retains the existing 8/16/32-bit ABI. As with the existing `__hireg`, interrupt
code calling C must preserve shared arithmetic scratch state. All native
48-bit translation units must use the same ABI.

## Verification

| Target | Coverage |
| --- | --- |
| `make check` | Quick check of SDCC, the JavaScript emulator and CENTOS executable generation |
| `make tests` | Quick check plus 22 regression cases built at `-O0` and `-O1`. All 44 binaries run on the C instruction emulator and the JavaScript microcoded emulator. A compile-only case checks the removal of an unreachable indirect store |
| `make alltests` | The main regression plus the generated compiler audit and extended tests for integer arithmetic, float32, ABI, inline assembly, optimizer passes, assembler and linker |
| `make tests-web` | Exercise the HTTP and WebSocket console, CPU controls, image round trip and server shutdown |
| `make tests-centos SEED=/path/to/CENTOS_13.IMG` | Run compiled programs on a new CENTOS disk copy and verify console input, calendar time, and JCL completion status |
| `make tests-games` | Play Chomp, Connect Four, chess and Wumpus from the generated application image. Check VT100 moves and Wumpus input handling. Set `APP_IMAGE` to test a different image |
| `make tests-finch` | Format a scratch Finch track, read and replace a sector, and verify that adjacent data is preserved on the microcoded simulator |
| `make bench-basic` | Run the Rugg/Feldman BM1 and BM2 BASIC benchmarks at the nominal 5 MHz CPU6 rate. Set `APP_IMAGE` to test a different image |

The independent C instruction emulator supports `-R` for raw binaries.
It starts at the designated entry without boot ROMs or wall-clock pacing
and schedules peripherals against emulated time. The port models word
increment flags, the CPU6 multiply and divide operand encodings and
multiword division. The full suite also runs generated code on the
JavaScript microcoded core.

Pass `TEST_JOBS` to run extended suites concurrently. Artifacts and logs
live in `build/out/tests/`. After building the tools, individual suites
such as `python3 tests/sdcc-inline-asm.py` and `python3 tests/sdcc-lto.py`
can run directly. If LeakSanitizer cannot run under ptrace, use
`ASAN_OPTIONS=detect_leaks=0 make alltests`.

See [benchmarks and CPU6 instruction timings](performance.md) for the
measured code-generation results and reproduction commands.
