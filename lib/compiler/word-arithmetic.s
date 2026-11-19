    .setcpu 6
    .code
    .export __mul
    .export __div
    .export __rem
    .export __divu
    .export __remu

; SPDX-License-Identifier: GPL-2.0-or-later
; Entry: (S) saved X, 2(S) left operand, B right operand. Use native
; unsigned DIV on the magnitudes and apply the signs for signed C.
; CPU6 function __mul begin
__mul:
    lda 2(s)
    mul a,b
    bra word_arithmetic_done

__divu:
    ori b,b
    bz word_zero_divisor
    lda 2(s)
    div b,a
    bra word_arithmetic_done
__remu:
    ori b,b
    bz word_zero_divisor
    lda 2(s)
    div b,a
    xab
    bra word_arithmetic_done

__div:
    ori b,b
    bz word_zero_divisor
    lda 2(s)
    ore b,a
    sta (-s)
    lda 4(s)
    bp divide_rhs
    iva
    ina
divide_rhs:
    ori b,b
    bp unsigned_quotient
    ivr b
    inr b
unsigned_quotient:
    div b,a
    bra word_result_sign

__rem:
    ori b,b
    bz word_zero_divisor
    lda 2(s)
    sta (-s)
    ori a,a
    bp remainder_rhs
    iva
    ina
remainder_rhs:
    ori b,b
    bp unsigned_remainder
    ivr b
    inr b
unsigned_remainder:
    div b,a
    xab
word_result_sign:
    lda (s+)
    bp word_arithmetic_done
    ivr b
    inr b
    bra word_arithmetic_done

word_zero_divisor:
    clr b
word_arithmetic_done:
    lda (s)
    sta 2(s)
    inr s,2
    rsr
; CPU6 function __mul end
