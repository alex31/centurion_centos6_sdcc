    .setcpu 6
    .code
    .export ___c6_fadd

; The aligned significands occupy two words. Keeping eight guard bits lets
; a pair of byte-offset loads put the fraction directly into position.
; X points to the larger magnitude and Y to the smaller after sorting.
; At 0(S) and 2(S) are the high and low halves of the larger significand.
; 4(S) holds its biased exponent and 6(S) its original high word for sign.
; CPU6 function ___c6_fadd begin
___c6_fadd:
    push xh,zl
    dcr s,8
    add s,16,x
    add s,20,y

    lda (x)
    and 0x7fff,a
    ldb (y)
    and 0x7fff,b
    sab
    bnl add_swap
    bnz add_sorted
    lda 2(x)
    ldb 2(y)
    sab
    bl add_sorted
add_swap:
    xfr x,a
    xfr y,x
    xfr a,y
add_sorted:
    lda (x)
    sta 6(s)
    and 0x7fff,a
    sub a,0x7f80,b
    bnl add_nan
    bnz add_finite
    ldb 2(x)
    bnz add_nan
    ; With infinity as the largest magnitude, the other argument may
    ; also be infinite. An opposite-signed pair is invalid.
    ldb (y)
    and 0x7fff,b
    sub 0x7f80,b
    bnz add_unchanged
    lda (x)
    ore (y),a
    bm add_nan
add_unchanged:
    lda (x)
    ldb 2(x)
    bra add_return_early
add_nan:
    lda 0x7fc0
    clr b
add_return_early:
    sta (__hireg)
    inr s,8
    pop xh,zl
    rsr

add_finite:
    srr a,7
    bz add_big_subnormal
    sta 4(s)
    lda 1(x)
    ori 0x8000,a
    bra add_big_ready
add_big_subnormal:
    lda 1
    sta 4(s)
    lda 1(x)
add_big_ready:
    sta (s)
    ldb 2(x)
    slr b,8
    stb 2(s)

    lda (y)
    and 0x7fff,a
    srr a,7
    bz add_small_subnormal
    xfr a,z
    lda 1(y)
    ori 0x8000,a
    bra add_small_ready
add_small_subnormal:
    clr z,1
    lda 1(y)
add_small_ready:
    ldb 4(s)
    sub b,z               ; larger exponent minus smaller exponent
    sub z,26,b
    bnl add_unchanged     ; below half an ulp even at a power of two
    ldb 2(y)
    slr b,8
    ori z,z
    bz add_aligned

    ; Shift the smaller operand by whole bytes, then by up to seven bits.
    ; XL accumulates discarded bits so cancellation and ties round correctly.
    clr x
add_shift_bytes:
    sub z,7,y
    bl add_shift_bits
    orib bl,xl
    xfrb bh,bl
    xfrb al,bh
    xfrb ah,al
    clrb ah
    dcr z,8
    bnz add_shift_bytes
    bra add_sticky
add_shift_bits:
    rl
    rrr a
    rrr b
    bnl add_shift_next
    ldx 1
add_shift_next:
    dcr z
    bnz add_shift_bits
add_sticky:
    ori x,x
    bz add_aligned
    ori 1,b
add_aligned:
    ldw z,16(s)
    ore 20(s),z           ; signs of the original arguments differ?
    bm add_difference
    add 2(s),b
    bnl add_high
    ; Preserve the carry from the low half even if the smaller high
    ; half wraps when incremented.
    add (s),a
    bl add_both_carry
    ina
    bnz add_pack
    bra add_carry
add_both_carry:
    ina
    bra add_carry
add_high:
    add (s),a
    bnl add_pack
add_carry:
    sl
    rrr a
    rrr b
    bnl add_raised
    ori 1,b
add_raised:
    inr 4(s)
    bra add_pack

add_difference:
    ; Magnitude ordering makes the difference nonnegative. Normalize until
    ; its hidden bit reaches bit 31 or the minimum exponent is reached.
    ldw x,2(s)
    sub x,b
    ldw x,(s)
    bl add_sub_high
    dcr x
add_sub_high:
    sub x,a
    bm add_pack
    bnz add_normalize
    ori b,b
    bz add_positive_zero
add_normalize:
    ldw z,4(s)
add_left:
    sub z,1,x
    bz add_normalized
    add b,b
    rlr a
    dcr z
    ori a,a
    bp add_left
add_normalized:
    stw z,4(s)

add_pack:
    ; Round on the discarded byte. The low bit of BH breaks exact ties.
    orib bl,bl
    bp add_rounded
    and b,0x01ff,x
    sub x,0x80,x
    bz add_rounded
    add 0x0100,b
    bnl add_rounded
    ina
    bnz add_rounded
    lda 0x8000
    inr 4(s)
add_rounded:
    ldw z,4(s)
    sub z,254,x
    bnl add_inf
    ori a,a
    bm add_normal
    clr z                 ; rounded subnormal or signed zero
add_normal:
    xfrb bh,bl
    xfrb al,bh
    xfrb ah,al
    and 127,a
    slr z,7
    ori z,a
    bra add_sign
add_inf:
    lda 0x7f80
    clr b
add_sign:
    ldw z,6(s)
    and 0x8000,z
    ori z,a
    bra add_return
add_positive_zero:
    cla
    clr b
add_return:
    sta (__hireg)
    inr s,8
    pop xh,zl
    rsr
; CPU6 function ___c6_fadd end
