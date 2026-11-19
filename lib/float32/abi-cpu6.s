; Binary32 and uint32_t share the stack and return ABI on CPU6. The
; comparison cores below return canonical 0/1 in the low byte.
	.setcpu 6
	.code
	.export ___c6_fbits
	.export ___c6_fvalue
	.export ___fsadd
	.export ___fsmul
	.export ___fsdiv
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
; CPU6 function ___fseq begin
___fseq:
	jmp ___c6_feq
; CPU6 function ___fseq end
; CPU6 function ___fslt begin
___fslt:
	jmp ___c6_flt
; CPU6 function ___fslt end
; CPU6 function ___fsle begin
___fsle:
	jmp ___c6_fle
; CPU6 function ___fsle end
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
