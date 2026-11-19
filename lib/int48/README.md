# Six-byte integer arithmetic

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

## Native scalar form with SDCC

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

## Implementation and validation

The portable implementation is in `int48.c`. `cpu6.s` supplies six native
16-bit partial products for the low 48-bit product, short shift primitives,
and general unsigned division using the microcoded D/DRM family. The division
routine zero-extends operands to eight bytes before signed DRM, preserving
the complete unsigned 48-bit range. Power-of-two divisors use a C fast path.
`scalar48.s` supplies the additional native-scalar return word; its separate
archive member is linked only when referenced.

The compiler emits copies, comparisons, bitwise operations and one-bit shifts
directly for native scalars. Speed mode also inlines three-word addition and
subtraction, and selected updates can operate directly on nonvolatile objects.
Other operations can call the same pointer
library using private stack snapshots; a native type does not imply every
operation is expanded inline.

The SDCC archive is split into function members after compiling the complete C
unit, so its within-unit optimizations are retained while unused routines are
not linked. Shared data is separate; private cross-member symbols are renamed.
The distribution enables the host linker's archive-rescan support.
