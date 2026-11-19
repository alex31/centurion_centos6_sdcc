    .setcpu 6
    .code
    .export __divl
    .export __divul
    .export __reml
    .export __remul

; SPDX-License-Identifier: GPL-2.0-or-later
; DRM can divide signed words directly. Unsigned operands with bit 31
; set need a zero extension. Reuse the caller's left operand for the
; quotient and its saved-X word for the optional extension.
; CPU6 function __divl begin
__divul:
    cla
    bra divide_long
__remul:
    lda 1
    bra divide_long
__divl:
    lda 2
    bra divide_long
__reml:
    lda 3
divide_long:
    sta (-s)
    stx (-s)
    lda 65522
    add a,s

    stb 4(s)
    lda 18(s)
    sta 12(s)
    lda (__hireg)
    sta 2(s)
    ori a,b
    bz division_zero

    lda 16(s)
    ldb 2
    nab
    bnz divide_four
    lda 2(s)
    ldb 20(s)
    ori a,b
    bp divide_four
    cla
    sta (s)
    sta 18(s)
    lda 6
    add s,a
    xfr s,b
    ldx 18
    add s,x
    rf
    ; DRM six bytes at B into six bytes at X, remainder at A.
    .byte 0x46,0x55,0x7a,0x24
    bra select_result
divide_four:
    lda 8
    add s,a
    ldb 2
    add s,b
    ldx 20
    add s,x
    rf
    .byte 0x46,0x33,0x7a,0x24
select_result:
    lda 16(s)
    ldb 1
    nab
    bnz choose_remainder
    lda 20(s)
    ldb 22(s)
    bra division_done
choose_remainder:
    lda 8(s)
    ldb 10(s)
    bra division_done
division_zero:
    cla
    clr b
division_done:
    sta (__hireg)
    ldx 14(s)
    lda 12(s)
    sta 22(s)
    lda 22
    add a,s
    rsr
; CPU6 function __divl end
