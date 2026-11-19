# CPU6 software binary32

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
Horner evaluation shares one polynomial loop. Trigonometric reduction multiplies
the significand by a 256-bit 2/pi table using small integer limbs; large finite
binary32 arguments follow the same reduction, rather than an overflowing cast
to a small integer quadrant. Square root uses an integer restoring algorithm.

The current library has no configurable rounding mode, floating exception
flags/traps, `errno` handling, NaN-payload preservation, decimal text conversion,
float `printf`, `powf` or complete hosted libc. These are deliberate limits of
this implementation; the format and tested arithmetic do not constitute full
IEC 60559/C Annex F conformance.
