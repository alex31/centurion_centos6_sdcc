; SPDX-License-Identifier: GPL-2.0-or-later
    .setcpu 6
    .code
    .export __bc_pop16
    .export __bc_lz
    .export __bc_tz
    .export __bc_floor16

; B in, count in B out. Accumulate groups of 2, 4 and 8 bits in parallel.
; The final byte addition sums both lanes. Only A and B are clobbered.
; CPU6 function __bc_pop16 begin
__bc_pop16:
    xfr b,a
    sra
    and 0x5555,a
    sub b,a
    xab
    and 0x3333,b
    srr a,2
    and 0x3333,a
    aab
    xfr b,a
    srr a,4
    aab
    and 0x0f0f,b
    addb bh,bl
    clrb bh
    rsr
; CPU6 function __bc_pop16 end

; Nonzero word in B and starting count in A. Skip the empty byte and
; nibble in one operation each; at most three single shifts remain.
; CPU6 function __bc_lz begin
__bc_lz:
    orib bh,bh
    bnz lz_byte
    xfrb bl,bh
    inr a,8
lz_byte:
    xfrb al,bl
    ldab 0xf0
    andb bh,al
    bnz lz_nibble
    inrb bl,4
    slrb bh,4
lz_nibble:
    orib bh,bh
    bm lz_done
lz_bit:
    inrb bl
    slrb bh
    bp lz_bit
lz_done:
    clrb bh
    rsr
; CPU6 function __bc_lz end

; CPU6 function __bc_tz begin
__bc_tz:
    orib bl,bl
    bnz tz_byte
    xfrb bh,bl
    inr a,8
tz_byte:
    xfrb al,bh
    ldab 15
    andb bl,al
    bnz tz_bit
    inrb bh,4
    srrb bl,4
tz_bit:
    srrb bl
    bl tz_done
    inrb bh
    bra tz_bit
tz_done:
    xfrb bh,bl
    clrb bh
    rsr
; CPU6 function __bc_tz end

; Highest set bit as a power of two, including floor(0) = 0.
; The 16-byte lookup belongs only to this archive member.
; CPU6 function __bc_floor16 begin
__bc_floor16:
    orib bh,bh
    bz floor_low_byte
    xfrb bh,al
    jsr floor_byte
    xfrb al,bh
    clrb bl
    rsr
floor_low_byte:
    xfrb bl,al
    jsr floor_byte
    xabb
    clrb bh
    rsr
floor_byte:
    clrb ah
    sub a,15,b
    bl floor_low_nibble
    srr a,4
    add floor_nibbles,a
    ldab (a)
    slrb al,4
    rsr
floor_low_nibble:
    add floor_nibbles,a
    ldab (a)
    rsr
floor_nibbles:
    .byte 0,1,2,2,4,4,4,4,8,8,8,8,8,8,8,8
; CPU6 function __bc_floor16 end
