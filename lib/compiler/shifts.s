    .setcpu 6
    .code
    .export __shl
    .export __shru
    .export __shr
    .export __shll
    .export __shrul
    .export __shrl

; SPDX-License-Identifier: GPL-2.0-or-later
; The count arrives in B. Shift the left operand following the saved
; X on S and discard that operand. Return in B or __hireg:B.
; Saturate out-of-range counts.
; CPU6 function __shl begin
__shl:
    lda 16
    sub b,a
    bl left_word_zero
    lda 2(s)
    ori b,b
    bz left_word_done
left_word_bit:
    slr a
    dcr b
    bnz left_word_bit
left_word_done:
    xab
left_word_return:
    lda (s)
    sta 2(s)
    inr s,2
    rsr
left_word_zero:
    clr b
    bra left_word_return
; CPU6 function __shl end

; CPU6 function __shru begin
__shru:
    lda 16
    sub b,a
    bl unsigned_word_zero
    lda 2(s)
    ori b,b
    bz unsigned_word_done
    rl
    rrr a
    dcr b
    bz unsigned_word_done
unsigned_word_bit:
    srr a
    dcr b
    bnz unsigned_word_bit
unsigned_word_done:
    xab
unsigned_word_return:
    lda (s)
    sta 2(s)
    inr s,2
    rsr
unsigned_word_zero:
    clr b
    bra unsigned_word_return
; CPU6 function __shru end

; CPU6 function __shr begin
__shr:
    lda 16
    sub b,a
    bnl signed_word_start
    ldb 15
signed_word_start:
    lda 2(s)
    ori b,b
    bz signed_word_done
signed_word_bit:
    srr a
    dcr b
    bnz signed_word_bit
signed_word_done:
    xab
    lda (s)
    sta 2(s)
    inr s,2
    rsr
; CPU6 function __shr end

; CPU6 function __shll begin
__shll:
    lda 32
    sub b,a
    bl left_long_zero
    lda 16
    sub b,a
    bl left_long_one_word
    xfr y,a
    sta (-s)
    xfr b,y
    lda 4(s)
    ldb 6(s)
    ori y,y
    bz left_long_done
left_long_bit:
    slr b
    rlr a
    dcr y
    bnz left_long_bit
left_long_done:
    sta (__hireg)
    lda (s+)
    xay
left_long_return:
    lda (s)
    sta 4(s)
    inr s,4
    rsr
left_long_zero:
    clr b
    stb (__hireg)
    bra left_long_return
left_long_one_word:
    xab
    lda 4(s)
    sta (-s)
    jsr __shl
    stb (__hireg)
    clr b
    bra left_long_return
; CPU6 function __shll end

; CPU6 function __shrul begin
__shrul:
    lda 32
    sub b,a
    bl unsigned_long_zero
    lda 16
    sub b,a
    bl unsigned_long_one_word
    xfr y,a
    sta (-s)
    xfr b,y
    lda 4(s)
    ldb 6(s)
    ori y,y
    bz unsigned_long_done
unsigned_long_bit:
    rl
    rrr a
    rrr b
    dcr y
    bnz unsigned_long_bit
unsigned_long_done:
    sta (__hireg)
    lda (s+)
    xay
unsigned_long_return:
    lda (s)
    sta 4(s)
    inr s,4
    rsr
unsigned_long_zero:
    clr b
    stb (__hireg)
    bra unsigned_long_return
unsigned_long_one_word:
    xab
    lda 2(s)
    sta (-s)
    jsr __shru
    cla
    sta (__hireg)
    bra unsigned_long_return
; CPU6 function __shrul end

; CPU6 function __shrl begin
__shrl:
    lda 32
    sub b,a
    bnl signed_long_start
    ldb 31
signed_long_start:
    lda 16
    sub b,a
    bl signed_long_one_word
    xfr y,a
    sta (-s)
    xfr b,y
    lda 4(s)
    ldb 6(s)
    ori y,y
    bz signed_long_done
signed_long_bit:
    srr a
    rrr b
    dcr y
    bnz signed_long_bit
signed_long_done:
    sta (__hireg)
    lda (s+)
    xay
signed_long_return:
    lda (s)
    sta 4(s)
    inr s,4
    rsr
signed_long_one_word:
    xab
    lda 2(s)
    sta (-s)
    jsr __shr
    cla
    ori b,b
    bp signed_long_store_high
    dca
signed_long_store_high:
    sta (__hireg)
    bra signed_long_return
; CPU6 function __shrl end
