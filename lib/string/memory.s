    .setcpu 6
    .code
    .export _memcpy
    .export _memmove
    .export _memset

; SPDX-License-Identifier: GPL-2.0-or-later
; Use the compiler's structure-copy routine with the identical public ABI.
; Keep the public name in its own archive member for applications that
; supply their own memcpy while still using the compiler's private helper.
; CPU6 function _memcpy begin
_memcpy:
    jmp ___memcpy
; CPU6 function _memcpy end

; CPY copies forward. A destination inside the source needs a backward copy.
; CPU6 function _memmove begin
_memmove:
    lda 2(s)
    ldb 4(s)
    sab                 ; unsigned destination - source
    bz move_done
    bnl move_forward    ; destination below source
    lda 6(s)
    sub b,a             ; gap - length
    bl move_forward     ; disjoint, including a zero length

    push yh,zl
    ldw y,6(s)
    ldw z,8(s)
    ldb 10(s)
    add b,y
    add b,z
move_back:
    ldab (-z)
    stab (-y)
    dcr b
    bnz move_back
    pop yh,zl
move_done:
    ldb 2(s)
    rsr
move_forward:
    jmp ___memcpy
; CPU6 function _memmove end

; Seed one byte, then propagate it into successive bytes with CPY.
; CPY counts from zero, so a one-byte fill ends after the seed store.
; CPU6 function _memset begin
_memset:
    ldb 2(s)
    lda 6(s)
    bz fill_done
    ldab 5(s)
    stab (b)
    lda 6(s)
    dca
    bz fill_done
    dca
    push yh,yl
    add b,1,y
    cpy
    pop yh,yl
fill_done:
    rsr
; CPU6 function _memset end
