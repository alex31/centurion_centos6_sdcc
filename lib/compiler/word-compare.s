    .setcpu 6
    .code
    .export __bool
    .export __boolc
    .export __not
    .export __notc
    .export __cceq
    .export __ccne
    .export __cclt
    .export __cclteq
    .export __ccgt
    .export __ccgteq
    .export __ccltu
    .export __ccltequ
    .export __ccgtu
    .export __ccgtequ

; SPDX-License-Identifier: GPL-2.0-or-later
; Word comparisons receive the flags of lhs - rhs. L means no borrow.
; For a signed comparison, N and F differ exactly when lhs < rhs.
; CPU6 function __bool begin
__boolc:
    orib bl,bl
    bra __ccne
__bool:
    ori b,b
__ccne:
    bz word_false
word_true:
    ldb 1
    rsr
word_false:
    clr b
    rsr

__notc:
    orib bl,bl
    bra __cceq
__not:
    ori b,b
__cceq:
    bz word_true
    bra word_false

__cclteq:
    bz word_true
__cclt:
    bf lt_overflow
    bm word_true
    bra word_false
lt_overflow:
    bp word_true
    bra word_false

__ccgt:
    bz word_false
__ccgteq:
    bf ge_overflow
    bp word_true
    bra word_false
ge_overflow:
    bm word_true
    bra word_false

__ccltequ:
    bz word_true
__ccltu:
    bnl word_true
    bra word_false
__ccgtu:
    bz word_false
__ccgtequ:
    bl word_true
    bra word_false
; CPU6 function __bool end
