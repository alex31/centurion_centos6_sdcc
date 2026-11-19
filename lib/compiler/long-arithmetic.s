    .setcpu 6
    .code
    .export __plusl
    .export __minusl
    .export __bandl
    .export __orl
    .export __xorl
    .export __mull
    .export __mullu

; SPDX-License-Identifier: GPL-2.0-or-later
; JSR pushes the caller's X at (S). The left operand follows it, high
; word first. The right operand and result occupy __hireg:B. Move the
; saved X above the consumed operand before RSR restores it.
; CPU6 function __plusl begin
__plusl:
    lda 4(s)
    aab
    stb 4(s)
    lda 2(s)
    ldb (__hireg)
    bnl add_words
    ina
add_words:
    aab
    stb (__hireg)
    ldb 4(s)
    lda (s)
    sta 4(s)
    inr s,4
    rsr
; CPU6 function __plusl end

; CPU6 function __minusl begin
__minusl:
    lda 4(s)
    sab
    stb 4(s)
    lda 2(s)
    ldb (__hireg)
    bl subtract_words
    dca
subtract_words:
    sab
    stb (__hireg)
    ldb 4(s)
    lda (s)
    sta 4(s)
    inr s,4
    rsr
; CPU6 function __minusl end

; CPU6 function __bandl begin
__bandl:
    lda 4(s)
    nab
    stb 4(s)
    lda 2(s)
    ldb (__hireg)
    nab
    stb (__hireg)
    ldb 4(s)
    lda (s)
    sta 4(s)
    inr s,4
    rsr
; CPU6 function __bandl end

; CPU6 function __orl begin
__orl:
    lda 4(s)
    ori a,b
    stb 4(s)
    lda 2(s)
    ldb (__hireg)
    ori a,b
    stb (__hireg)
    ldb 4(s)
    lda (s)
    sta 4(s)
    inr s,4
    rsr
; CPU6 function __orl end

; CPU6 function __xorl begin
__xorl:
    lda 4(s)
    ore a,b
    stb 4(s)
    lda 2(s)
    ldb (__hireg)
    ore a,b
    stb (__hireg)
    ldb 4(s)
    lda (s)
    sta 4(s)
    inr s,4
    rsr
; CPU6 function __xorl end

; CPU6 function __mull begin
__mullu:
__mull:
    ; Low-low product plus the two cross products shifted by one word.
    stb (-s)
    lda 6(s)
    mul b,a
    stb (-s)
    sta (-s)
    lda 4(s)
    ldb 8(s)
    mul a,b
    lda (s)
    aab
    stb (s)
    lda (__hireg)
    ldb 10(s)
    mul a,b
    lda (s)
    aab
    stb (__hireg)
    ldb 2(s)
    lda 6(s)
    sta 10(s)
    inr s,10
    rsr
; CPU6 function __mull end
