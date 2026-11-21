    .setcpu 6
    .code
    .export ___memcpy

; SPDX-License-Identifier: GPL-2.0-or-later
; SDCC emits ___memcpy for structure assignment (stack ABI, 16-bit size).
; Leaf helper: caller's X is already saved by JSR at 2(S) after saving Y.
; Arguments: 4(S) destination, 6(S) source, 8(S) count.
; CPU6 CPY (0xF7) copies A+1 bytes from (B) to (Y) and preserves A, B, Y.
; CPU6 function ___memcpy begin
___memcpy:
    xfr y,a
    sta (-s)
    ldb 4(s)
    lda 8(s)
    bz memcpy_done
    xfr b,y
    ldb 6(s)
    dca
    cpy
    xfr y,b
memcpy_done:
    lda (s+)
    xay
    rsr
; CPU6 function ___memcpy end
