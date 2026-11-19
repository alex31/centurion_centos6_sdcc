# Native CPU6 compiler helpers

The build assembles these sources into `libcompiler-sdcc.a`. SDCC's
CPU6 link drivers and the BASIC build use that archive for compiler helper
calls. The source for an individual helper is linked only when needed.

The arithmetic helpers follow the CPU6 ABI emitted by SDCC. On entry,
`JSR` has saved the caller's X at `(S)`. A two-operand helper receives
the left operand after that saved word, with the high word first for a
32-bit value. The right operand and result use B or `__hireg:B`.
Helpers consume their stack operand and place the saved X where `RSR`
can restore it. Comparison helpers return 0 or 1 in B.

The 32-bit division helpers use the CPU6 DRM instruction. Unsigned values
with bit 31 set are zero-extended to six bytes. The variable 32-bit shifts
use a word helper once the shift distance reaches 16. The compound
assignment adapters cover the pointer-operand ABI exercised by the
compiler audit. The ordinary drivers also accept explicitly supplied
objects and archives for assembly programs that use additional helpers.

The sources are licensed under GPL-2.0-or-later. See [COPYING](../../COPYING).
