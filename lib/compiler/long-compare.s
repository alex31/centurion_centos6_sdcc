    .setcpu 6
    .code
    .export __cceql
    .export __ccnel
    .export __ccltl
    .export __cclteql
    .export __ccgtl
    .export __ccgteql
    .export __ccltul
    .export __ccltequl
    .export __ccgtul
    .export __ccgtequl

; SPDX-License-Identifier: GPL-2.0-or-later
; The left operand follows the saved X on S, high word first.
; __hireg:B is the right operand. Return 0 or 1 in B and discard the
; left operand while preserving the saved X for RSR.
; CPU6 function __cceql begin
__cceql:
    lda 4(s)
    ore a,b
    bnz equal_false
    lda 2(s)
    ldb (__hireg)
    ore a,b
    bnz equal_false
    ldb 1
    bra equal_done
equal_false:
    clr b
equal_done:
    lda (s)
    sta 4(s)
    inr s,4
    rsr
; CPU6 function __cceql end

; CPU6 function __ccnel begin
__ccnel:
    lda 4(s)
    ore a,b
    bnz different
    lda 2(s)
    ldb (__hireg)
    ore a,b
    bz unequal_done
different:
    ldb 1
unequal_done:
    lda (s)
    sta 4(s)
    inr s,4
    rsr
; CPU6 function __ccnel end

; Bits 0, 1 and 2 of the mask carry the acceptable relations. Bit 3
; selects an unsigned high-word comparison. Low words are always unsigned.
; CPU6 function __ccltl begin
__ccltl:
    lda 1
    bra long_order
__cclteql:
    lda 3
    bra long_order
__ccgtl:
    lda 4
    bra long_order
__ccgteql:
    lda 6
    bra long_order
__ccltul:
    lda 9
    bra long_order
__ccltequl:
    lda 11
    bra long_order
__ccgtul:
    lda 12
    bra long_order
__ccgtequl:
    lda 14

long_order:
    stb (-s)
    sta (-s)
    ldb 8
    nab
    bz compare_signed_high
    lda 6(s)
    ldb (__hireg)
    sab
    bz compare_low
    bnl long_less
    bra long_greater
compare_signed_high:
    lda 6(s)
    ldb (__hireg)
    sab
    bz compare_low
    bf high_overflow
    bm long_less
    bra long_greater
high_overflow:
    bp long_less
    bra long_greater
compare_low:
    lda 8(s)
    ldb 2(s)
    sab
    bz long_equal
    bnl long_less
long_greater:
    ldb 4
    bra accept_relation
long_equal:
    ldb 2
    bra accept_relation
long_less:
    ldb 1
accept_relation:
    lda (s)
    nab
    bz order_done
    ldb 1
order_done:
    lda 4(s)
    sta 8(s)
    inr s,8
    rsr
; CPU6 function __ccltl end
