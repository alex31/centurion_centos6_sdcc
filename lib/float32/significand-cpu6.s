    .setcpu 6
    .code
    .export ___c6_sigmul

; Hardware MUL returns an unsigned product in A:B. Multiply two
; normalized 24-bit significands and accumulate their cross products.
; Input words
; follow the saved X. Return a 27-bit significand with a sticky bit.
; Adjust the exponent if the product uses all 48 bits.
; CPU6 function ___c6_sigmul begin
___c6_sigmul:
    dcr s,8             ; product low/middle/high, temporary high
    lda 12(s)
    ldb 16(s)
    mul b,a
    stb (s)
    sta 2(s)

    lda 10(s)
    ldb 14(s)
    mul b,a
    stb 4(s)

    ; Add the first cross product shifted one word.
    lda 10(s)
    ldb 16(s)
    mul b,a
    sta 6(s)
    lda 2(s)
    aab
    stb 2(s)
    ldb 4(s)
    bnl cross1_nc
    inr b
cross1_nc:
    lda 6(s)
    aab
    stb 4(s)

    ; Add the second cross product shifted one word.
    lda 14(s)
    ldb 12(s)
    mul b,a
    sta 6(s)
    lda 2(s)
    aab
    stb 2(s)
    ldb 4(s)
    bnl cross2_nc
    inr b
cross2_nc:
    lda 6(s)
    aab
    stb 4(s)
    ori b,b
    bm shift21

    ; Product >> 20. The top word is positive in this branch.
    lda 4(s)
    srr a,4
    sta (__hireg)
    ldb 2(s)
    rl
    rrr b
    srr b,3
    lda 4(s)
    slr a,12
    ori a,b
    lda 2(s)
    slr a,12
    bra sticky

shift21:
    ldb 18(s)
    lda (b)
    ina
    sta (b)
    lda 4(s)
    rl
    rrr a
    srr a,4
    sta (__hireg)
    ldb 2(s)
    rl
    rrr b
    srr b,4
    lda 4(s)
    slr a,11
    ori a,b
    lda 2(s)
    slr a,11

sticky:
    bnz sticky_set
    lda (s)
    bz sigmul_done
sticky_set:
    ldab 1
    orib al,bl
sigmul_done:
    inr s,8
    rsr
; CPU6 function ___c6_sigmul end

    .export ___c6_fjam
; Shift an unsigned word pair right, merging all discarded bits into
; the low bit. The count is a separate unsigned word argument.
; CPU6 function ___c6_fjam begin
___c6_fjam:
    ldb 6(s)
    bz jam_identity
    lda 32
    sub b,a
    bl jam_large
    xfr y,a
    sta (-s)
    xfr z,a
    sta (-s)
    xfr b,y
    clr z
    lda 6(s)
    ldb 8(s)
jam_bit:
    rl
    rrr a
    rrr b
    bnl jam_no_bit
    inr z
jam_no_bit:
    dcr y
    bnz jam_bit
    sta (__hireg)
    ori z,z
    bz jam_restore
    ldab 1
    orib al,bl
jam_restore:
    lda (s+)
    xaz
    lda (s+)
    xay
    rsr
jam_identity:
    lda 2(s)
    sta (__hireg)
    ldb 4(s)
    rsr
jam_large:
    lda 2(s)
    ldb 4(s)
    ori a,b
    bz jam_zero
    ldb 1
jam_zero:
    cla
    sta (__hireg)
    rsr
; CPU6 function ___c6_fjam end
