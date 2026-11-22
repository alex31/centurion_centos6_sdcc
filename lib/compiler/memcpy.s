    .setcpu 6
    .code
    .export ___memcpy

; SPDX-License-Identifier: GPL-2.0-or-later
; SDCC emits ___memcpy for structure assignment (stack ABI, 16-bit size).
; On entry: 2(S) destination, 4(S) source, 6(S) count.
; CPU6 CPY (0xF7) copies A+1 bytes from (B) to (Y) and preserves A, B, Y.
; CPU6 function ___memcpy begin
___memcpy:
    lda 6(s)
    bz memcpy_done
    push yh,yl
    ldw y,4(s)
    ldb 6(s)
    dca
    cpy
    pop yh,yl
memcpy_done:
    ldb 2(s)
    rsr
; CPU6 function ___memcpy end
