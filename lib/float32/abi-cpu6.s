; Binary32 and uint32_t share the stack and return ABI on CPU6. The
; comparison cores below return canonical 0/1 in the low byte.
	.setcpu 6
	.code
	.export ___c6_fbits
	.export ___c6_fvalue
	.export ___fsadd
	.export ___fsmul
	.export ___fsdiv
	.export ___c6_feq
	.export ___c6_flt
	.export ___c6_fle
	.export ___fseq
	.export ___fslt
	.export ___fsle
	.export ___ulong2fs
	.export ___slong2fs
	.export ___fs2ulong
	.export ___fs2slong

; CPU6 function ___c6_fbits begin
___c6_fbits:
___c6_fvalue:
	lda 2(s)
	sta (__hireg)
	ldb 4(s)
	rsr
; CPU6 function ___c6_fbits end

; CPU6 function ___fsadd begin
___fsadd:
	jmp ___c6_fadd
; CPU6 function ___fsadd end
; CPU6 function ___fsmul begin
___fsmul:
	jmp ___c6_fmul
; CPU6 function ___fsmul end
; CPU6 function ___fsdiv begin
___fsdiv:
	jmp ___c6_fdiv
; CPU6 function ___fsdiv end
; IEEE ordered comparisons use the sign and the unsigned ordering of the
; magnitude words. The other two relations (greater and unordered) return
; false at all three entry points. A mask selects less (1) and/or equal (2).
; CPU6 function ___fseq begin
___fseq:
___c6_feq:
    ldb 2
    bra ordered_compare
___fslt:
___c6_flt:
    ldb 1
    bra ordered_compare
___fsle:
___c6_fle:
    ldb 3
ordered_compare:
    stb (-s)
    lda 4(s)
    and 0x7fff,a
    sub a,0x7f80,b       ; infinity high word minus magnitude high word
    bnl compare_false
    bnz ordered_right
    ldb 6(s)
    bnz compare_false    ; NaN with payload in the low word
ordered_right:
    lda 8(s)
    and 0x7fff,a
    sub a,0x7f80,b
    bnl compare_false
    bnz ordered_signs
    ldb 10(s)
    bnz compare_false
ordered_signs:
    lda 4(s)
    ldb 8(s)
    ore a,b
    bm signs_differ
    ldb 8(s)
    sab
    bnz magnitude_differs
    lda 6(s)
    ldb 10(s)
    sab
    bz compare_equal
magnitude_differs:
    ldab 4(s)            ; load preserves the unsigned no-borrow flag
    bm both_negative
    bnl compare_less
    bra compare_false
both_negative:
    bl compare_less
    bra compare_false
signs_differ:
    ; The two encodings of zero compare equal.
    lda 4(s)
    ori 8(s),a
    sla
    bnz compare_sign
    lda 6(s)
    ori 10(s),a
    bz compare_equal
compare_sign:
    ldab 4(s)
    bp compare_false
compare_less:
    ldb 1
    bra compare_mask
compare_equal:
    ldb 2
compare_mask:
    lda (s)
    nab
    bz compare_done
    ldb 1
    bra compare_done
compare_false:
    clr b
compare_done:
    inr s,2
    rsr
; CPU6 function ___fseq end
; CPU6 function ___ulong2fs begin
___ulong2fs:
	jmp ___c6_ffromu
; CPU6 function ___ulong2fs end
; CPU6 function ___slong2fs begin
___slong2fs:
	jmp ___c6_ffromi
; CPU6 function ___slong2fs end
; CPU6 function ___fs2ulong begin
___fs2ulong:
	jmp ___c6_ftou
; CPU6 function ___fs2ulong end
; CPU6 function ___fs2slong begin
___fs2slong:
	jmp ___c6_ftoi
; CPU6 function ___fs2slong end
